/*
 * This file is part of Mossofs.
 *
 * Mossofs is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 3 of the
 * License.
 *
 * Mossofs is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Mossofs; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 * Copyright (C) 2009 Jakob Westhoff <jakob@westhoffswelt.de>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include <curl/curl.h>

#include "mosso.h"
#include "simple_curl.h"
#include "salloc.h"

static char* error_string = NULL;
static long  error_code = MOSSO_ERROR_OK;
#define set_error(c, e, ...) (error_code = c, ((error_string != NULL) ? free(error_string) : NULL), (( e != NULL ) ? (asprintf( &error_string, e, ##__VA_ARGS__ ), NULL) : (error_string = NULL) ))
char* mosso_error_string() { return error_string; }
long  mosso_error() { return error_code; }

#define MOSSO_PATH_TYPE_PATH 0
#define MOSSO_PATH_TYPE_FILE 1


static void mosso_authenticate( mosso_connection_t** mosso );
static mosso_object_t* mosso_create_object_list_from_response_body( mosso_object_t* object, char* response_body, char* path_prefix, int type, int* num );
static mosso_object_t* mosso_object_add( mosso_object_t* object, char* name, char* request_path, int type );
static char* mosso_construct_request_url( mosso_connection_t* mosso, char* request_path, int type, char* marker );
static char* mosso_container_from_request_path( char* request_path );
static mosso_object_meta_t* mosso_object_meta_init();
static char* mosso_name_from_request_path( char* request_path );
static inline char* mosso_lowercase( char* s );

/**
 * Convert a given string to lowercase letters and return a newly allocated one
 * containing the new one.
 *
 * The caller is responsible for freeing the returned string if it is not
 * needed any longer.
 */
static inline char* mosso_lowercase( char* s )
{
    char* src = NULL;
    char* target = NULL;
    char* tmp_lowercase = (char*)smalloc( sizeof( char ) * strlen( s ) );
    for( src = s, target = tmp_lowercase; *src!= 0; ++src )
        *(target++) = tolower( *src );
    return tmp_lowercase;
}

/**
 * Authenticate with the mosso service
 *
 * A partly initialized mosso_connection_t struct is needed for this call to
 * work. The key and username has to be available in the struct. If the
 * connection fails the given struct is freed and set to NULL. Furthermore
 * error_string is set accordingly.
 */
static void mosso_authenticate( mosso_connection_t** mosso )
{
    simple_curl_header_t* request_headers  = NULL;
    char* response_body                    = NULL;
    simple_curl_header_t* response_headers = NULL;
    long response_code = 0L;

    request_headers = simple_curl_header_add( request_headers, "X-Auth-User", (*mosso)->username );
    request_headers = simple_curl_header_add( request_headers, "X-Auth-Key", (*mosso)->key );

    if ( ( response_code = simple_curl_request_get( "https://api.mosso.com/auth", &response_body, &response_headers, request_headers ) ) != 204 )
    {
        // Mosso responded with something different than a 204. This indicates
        // an error.
        switch( response_code ) 
        {
            case 401:
                set_error( MOSSO_ERROR_UNAUTHORIZED, "The authorization has been declined: %s", response_body );
            break;            
        }
        mosso_cleanup( (*mosso) );
        free( response_body );
        simple_curl_header_free_all( request_headers );
        simple_curl_header_free_all( response_headers );
        (*mosso) = NULL;
        return;
    }

    // Free the request headers as they are not needed any longer.
    simple_curl_header_free_all( request_headers );
    // The reponse body is supposed to be an empty string at this moment.
    // Therefore it is not needed any more.
    free( response_body );

    // Fillup the mosso_connection structure with the retrieved informations
    (*mosso)->storage_token      = strdup( simple_curl_header_get_by_key( response_headers, "X-Storage-Token" ) );
    (*mosso)->auth_token         = strdup( simple_curl_header_get_by_key( response_headers, "X-Auth-Token" ) );
    (*mosso)->storage_url        = strdup( simple_curl_header_get_by_key( response_headers, "X-Storage-Url" ) );
    (*mosso)->cdn_management_url = strdup( simple_curl_header_get_by_key( response_headers, "X-CDN-Management-Url" ) );

    // The required information has been copied. Therefore the retrieved
    // headers are not needed any longer.
    simple_curl_header_free_all( response_headers );

    // Create a simple_curl header structure to be simply provided for each
    // following mosso call to be correctly authenticated.
    (*mosso)->auth_headers = simple_curl_header_add( (*mosso)->auth_headers, "X-Auth-Token", (*mosso)->auth_token );
}


/**
 * Initialize a new connection to the mosso cloudspace
 *
 * Authentication is automatically done upon calling this function.
 *
 * A valid mosso username as well as a valid API key is needed to fulfill the
 * init request.
 */
mosso_connection_t* mosso_init( char* username, char* key )
{
    mosso_connection_t* mosso = smalloc( sizeof( mosso_connection_t ) );
    mosso->username = strdup( username );
    mosso->key      = strdup( key );
    mosso_authenticate( &mosso );

    // DEBUG
    printf(
        "username: %s\nkey: %s\nstorage_token: %s\nauth_token: %s\nstorage_url: %s\ncdn_management_url: %s\n",
        mosso->username,
        mosso->key,
        mosso->storage_token,
        mosso->auth_token,
        mosso->storage_url,
        mosso->cdn_management_url
    );

    return mosso;
}

/**
 * Add a new entry to a mosso object linked list
 *
 * The provided list item has always to be the last one in the list.
 *
 * If null is given as initial list item, a new list will be created.
 */
static mosso_object_t* mosso_object_add( mosso_object_t* object, char* name, char* request_path, int type )
{
    // Initialize a new object entry
    mosso_object_t* new_object = (mosso_object_t*)smalloc( sizeof( mosso_object_t ) );

    // Copy the data to it
    new_object->type         = type;
    new_object->name         = strdup( name );
    new_object->request_path = strdup( request_path );

    // Set the root and next accordingly
    new_object->next = NULL;
    if ( object == NULL )
    {
        // This was an initialization the newly created object is root
        new_object->root = new_object;
    }
    else
    {
        new_object->root = object->root;
        object->next = new_object;
    }

    return new_object;
}

/**
 * Free all objects inside a given mosso object list.
 *
 * Internally stored and allocated strings will be freed as well.
 */
void mosso_object_free_all( mosso_object_t* object )
{
    mosso_object_t* cur = object->root;
    while ( cur != NULL )
    {
        mosso_object_t* next = cur->next;
        (cur->name != NULL)         ? free( cur->name )         : NULL;
        (cur->request_path != NULL) ? free( cur->request_path ) : NULL;
        free( cur );
        cur = next;
    }
}

/**
 * Split a given object list repsonse body into a list of object structs.
 *
 * The data will be appended to the given list of objects. If NULL is provided
 * a new list will be started.
 *
 * The num parameter is filled with the number of object entries created.
 */
static mosso_object_t* mosso_create_object_list_from_response_body( mosso_object_t* object, char* response_body, char* path_prefix, int type, int* num )
{
    int   num_objects = 0;
    char* cur         = response_body;

    while( TRUE )
    {
        char* request_path = NULL;
        char* name         = NULL;
        char* start = cur;
        char* end   = cur;

        // Find the next newline or null terminator
        while( *end != '\n' && *end != 0 )  { ++end; }

        // Allocate some space for the new name and copy it to the target
        name = (char*)smalloc( sizeof( char ) * ( end - start + 1 ) );
        memcpy( name, start, end - start );

        // Create the needed request path
        asprintf( &request_path, "%s%s", path_prefix, name );

        // Add entry to the list
        object = mosso_object_add( object, name, request_path, type );
        ++num_objects;

        // Free all the temporary created strings
        free( name );
        free( request_path );

        if ( *end == 0 || *(end+1) == 0 ) /* Stop char or next start char is 0 stop here */
        {
            // The response_body has been completely analysed
            break;
        }
        else
        {
            // Advance to the next run
            cur = end + 1;
        }
    }

    *num = num_objects;
    return object;
}

/**
 * Construct the correct request url from a given request_path and the type of
 * the request.
 *
 * The request path will be correclty escaped to based on the request_path given.
 * The initial slash is never escaped as it is part of the path selectors of the url.
 * The second slash is not escaped as well, as it specifies the seperator
 * between container and container content.
 *
 * If the type is set to MOSSO_PATH_TYPE_PATH and more than two slashes are
 * used the "path" parameter is used to select a virtual subpath.
 *
 * The marker parameter set to the escaped version of the given marker string
 * is set if it is not NULL.
 */
static char* mosso_construct_request_url( mosso_connection_t* mosso, char* request_path, int type, char* marker )
{
    char* request_url = NULL;
    // All of the empty strings are allocated on the heap, so they always can
    // be free no matter if they have been used or not.
    char* base_url    = smalloc( sizeof( char ) );
    char* path_url    = smalloc( sizeof( char ) );
    char* parameters  = smalloc( sizeof( char ) );

    // Set the correct base url
    {
        free( base_url );
        base_url = strdup( mosso->storage_url );
    }

    // Construct the correct path for the url
    {
        char* encoded_path = smalloc( sizeof( char ) );
        int   seen_slashes = 0;
        char* start = request_path;
        char* end   = request_path;

        while( (*end) != 0 )
        {
            char* tmp          = NULL;
            char* encoded_part = NULL;
            // Find the next / or string end
            while( (*end) != '/' && (*end) != 0 ) { ++end; }

            if ( end != start )
            {
                encoded_part = simple_curl_urlencode( start, end - start );
                tmp = encoded_path;
                asprintf( &encoded_path, "%s%s", tmp, encoded_part );

                free( tmp );
                free( encoded_part );
            }

            // Advance to the next string section or to the end of the string
            if ( (*end) == 0 )
            {
                start = end;
            }
            else
            {
                start = ++end;
            }

            // Handle the first slash correctly
            // The initial slash is only appended if the string does not end
            // after it.
            if ( seen_slashes == 0 && (*end) != 0 )
            {
                // Initial slash
                tmp = encoded_path;
                asprintf( &encoded_path, "%s/", tmp );
                free( tmp );
            }
            else if ( seen_slashes == 1 && type == MOSSO_PATH_TYPE_FILE && strlen( start ) != 0 )
            {
                // We are in file mode and have just entered the file part of
                // the path.
                
                /*
                 * Unfortunately the mosso service does not handle these
                 * requests correctly, as they are described in the docs. The
                 * docs propose that the object name needs to be fully url
                 * encoded that would include all slashes in it. Unfortunately
                 * the service only accepts urlencoded names accept for the
                 * slashes they need to be provided unencoded.
                 */

                end = start;
                while ( TRUE ) 
                {
                    if ( (*end) == '/' || (*end) == 0 ) 
                    {
                        encoded_part = simple_curl_urlencode( start, end - start );
                        tmp = encoded_path;
                        asprintf( &encoded_path, "%s/%s", tmp, encoded_part );
                        free( tmp );
                        free( encoded_part );

                        if ( (*end) != 0 ) 
                        {
                            start = ++end;
                            continue;
                        }
                        else 
                        {
                            // We have reached the end of the string
                            break;
                        }
                    }
                    ++end;
                }
                break;

                /* Behaviour according to the docs. This is left in here, in
                 * case mosso decides to get conform to there own documentation
                 * one day.                
                encoded_part = simple_curl_urlencode( start, 0 );
                tmp = encoded_path;
                asprintf( &encoded_path, "%s/%s", tmp, encoded_part );
                free( tmp );
                free( encoded_part );
                break;
                */
            }

            if ( seen_slashes == 1 && type == MOSSO_PATH_TYPE_PATH )
            {
                // A path parameter is always appended to ensure a directory
                // like structure is returned without listing all pseudo
                // subdirectory information. It is filled with the complete
                // left over string urlencoded. A preceeding slash is not
                // appended. In case a container has simply been reguested an
                // empty path parameter is provided which enables virtualpath
                // handling on the mosso side.
                free( parameters );
                encoded_part = simple_curl_urlencode( start, 0 );
                asprintf( &parameters, "?path=%s", encoded_part );
                free( encoded_part );
                break;
            }

            // Next section
            ++seen_slashes;
        }

        free( path_url );
        path_url = encoded_path;
    }

    // Create the correct parameters string
    {
        if ( marker != NULL )
        {
            char* encoded_part = simple_curl_urlencode( marker, 0 );

            if ( strlen( parameters ) == 0 )
            {
                // Arguments list is still empty
                asprintf( &parameters, "?marker=%s", encoded_part );
            }
            else
            {
                // Arguments list does already contain arguments
                char* tmp = parameters;
                asprintf( &parameters, "%s&marker=%s", tmp, encoded_part );
            }

            free( encoded_part );
        }
    }

    asprintf( &request_url, "%s%s%s", base_url, path_url, parameters );
    free( base_url );
    free( path_url );
    free( parameters );
    return request_url;
}

/**
 * Isolate the container name from a given full request path and return it.
 *
 * The caller needs to free the returned string if it is not needed any longer.
 */
static char* mosso_container_from_request_path( char* request_path )
{
    char* container = NULL;
    char* start = request_path + 1; /* skip the initial slash */
    char* end   = request_path + 1;

    // Find the next slash or string end
    while( (*end) != '/' && (*end) != 0 ) { ++end; }

    // Allocate the needed memory and copy the string over
    container = smalloc( sizeof( char ) * ( end - start + 1 ) );
    memcpy( container, start, end - start );
    return container;
}

/**
 * Return the name of an object from a given request path
 *
 * The caller needs to free the returned string if it is not needed any longer.
 */
static char* mosso_name_from_request_path( char* request_path ) 
{    
    char* name  = NULL;
    char* end   = request_path + strlen( request_path );
    char* start = end;
    
    // Search for the last slash in the path or the path beginning if there are
    // no slashes in it.
    while( *start != '/' && start > request_path ) { --start; };
    
    name = smalloc( sizeof( char ) * ( end - start ) );
    memcpy( name, start + 1, end-start ); /* skip the initial slash */
    return name;
}

/**
 * Initialize a new object meta structure and return it
 */
static mosso_object_meta_t* mosso_object_meta_init() 
{
    mosso_object_meta_t* meta = ( mosso_object_meta_t* )smalloc( sizeof( mosso_object_meta_t ) );
    return meta;
}

/**
 * Free a given mosso meta structure including all the linked information
 */
void mosso_object_meta_free( mosso_object_meta_t* meta ) 
{
    if ( meta != NULL ) 
    {
        (meta->name != NULL) ? free( meta->name ) : NULL;
        (meta->request_path != NULL) ? free( meta->request_path ) : NULL;
        (meta->content_type != NULL) ? free( meta->content_type ) : NULL;
        (meta->mtime != NULL) ? free( meta->mtime ) : NULL;
        (meta->tag != NULL) ? mosso_tag_free_all( meta->tag ) : NULL;
        free( meta );
    }
}

/**
 * Add a new tag to a linked list of tags
 *
 * The tag will be added to the given linked list of tags and the new end of
 * the list will be returned. You have to always provide the end of the list to
 * this function in order to append a new element. Otherwise the behaviour is
 * undefined.
 *
 * Tags are case insensitive and will always be stored in lowercase letters.
 *
 * If the provided tag is NULL a new list will be created and returned.
 *
 * This function does not check if the same key value pair does already exist
 * on the given list. A key provided twice will be added twice to the list.
 * This might cause problems during the execution of the corresponding mosso
 * call using this tag list.
 *
 * If you are working on a list which content you do not know completely or
 * want to replace an existing key with a new value use the
 * mosso_tag_replace_or_add function instead. But be warned that this function
 * is more time complex than this, because it needs to iterate through the
 * whole list checking every provided key.
 */
mosso_tag_t* mosso_tag_add( mosso_tag_t* tag, char* key, char* value ) 
{
    // Initialize a new tag entry
    mosso_tag_t* new_tag = (mosso_tag_t*)smalloc( sizeof( mosso_tag_t ) );
    
    // Lowercase the key
    char* lkey = mosso_lowercase( key );

    // Copy the data to it
    new_tag->key   = lkey;
    new_tag->value = strdup( value );

    // Set the root and next accordingly
    new_tag->next = NULL;
    if ( tag == NULL )
    {
        // This was an initialization the newly created tag is root
        new_tag->root = new_tag;
    }
    else
    {
        new_tag->root = tag->root;
        tag->next = new_tag;
    }

    return new_tag;
}

/**
 * Replace or add a provided key/value pair to a tag list
 *
 * The list to use for replacement or addition needs to be supplied as first
 * parameter. If NULL is given here a new list will be created.
 *
 * Tags are case insensitive and will always be stored in lowercase letters.
 *
 * This function will iterate through the whole list in order to check for an
 * already existing key to replace its value or to ensure the keys are only
 * added once. This ensures data integraty in contrast to the mosso_tag_add
 * function, but needs more computational time. In cases you know you are
 * creating a new list without duplicates use the mosso_tag_add function
 * instead.
 *
 * The returned tag will always be the last in the list, even if the key/value
 * pair replaced lies within the list. This is done to ensure the returned tag
 * may be used in conjunction with future calls to mosso_tag_add.
 */
mosso_tag_t* mosso_tag_replace_or_add( mosso_tag_t* tag, char* key, char* value ) 
{
    // If NULL is provided simply create a new list and return it.
    if ( tag == NULL ) 
    {
        return mosso_tag_add( NULL, key, value );
    }
    

    // Check if the key already exists and store the end of the list on our way
    // through it.
    {
        char* lkey = mosso_lowercase( key );

        mosso_tag_t* cur         = tag->root;
        mosso_tag_t* end         = NULL;
        mosso_tag_t* replacement = NULL;

        while( cur != NULL ) 
        {
            end = cur;
            if ( strcmp( cur->key, lkey ) == 0 ) 
            {
                replacement = cur;
            }
            cur = cur->next;
        }

        // The lowercased key is not needed any longer
        free( lkey );

        // If a replacement needs to be done, simply modify the value and
        // return the end of the list
        if ( replacement != NULL ) 
        {
            ( replacement->value != NULL ) ? free( replacement->value ) : NULL;
            replacement->value = strdup( value );
            return end;
        }

        // In case the key is not in the list simply append it
        return mosso_tag_add( end, key, value );
    }
}

/** 
 * Search a tag linked list for a given key and return it. 
 *
 * Tags are case insensitive and will always be stored in lowercase letters.
 *
 * If no entry with the specified key can be found NULL is returned.
 */
mosso_tag_t* mosso_get_tag_by_key( mosso_tag_t* tag, char* key ) 
{
    mosso_tag_t* cur = tag->root;
    
    char* lkey = mosso_lowercase( key );

    while( cur != NULL ) 
    {
        if ( strcmp( cur->key, key ) == 0 ) 
        {
            free( lkey );
            return cur;
        }
        cur = cur->next;
    }
    free( lkey );
    return NULL;
}

/**
 * Free all tags in the given linked list of tags
 *
 * This includes the internally stored key and value pairs.
 */
void mosso_tag_free_all( mosso_tag_t* tag ) 
{
    mosso_tag_t* cur = tag->root;
    while( cur != NULL )
    {
        mosso_tag_t* next = cur->next;
        (cur->key != NULL) ? free( cur->key ) : NULL;
        (cur->value != NULL ) ? free( cur->value ): NULL;
        free( cur );
        cur = next;
    }
}

/**
 * Retrieve a list of objects inside a given container.
 *
 * If NULL is passed to the request_path a list of available
 * containers will be returned. Otherwise the request_path has to be fully
 * blown path to the requested resource. e.g "/foo" to request the contents of
 * container foo.
 *
 * Given paths deeper than one level, will be automatically translated into a
 * virtual path request.
 *
 * The returned object will be a linked list of objects.
 *
 * If count is a value different to NULL it will be filled with the number of
 * objects retrieved.
 *
 * If an error occured NULL will be returned and the error string will be set
 * accordingly.
 */
mosso_object_t* mosso_list_objects( mosso_connection_t* mosso, char* request_path, int* count )
{
    char* response_body    = NULL;
    int   response_code    = 0;
    mosso_object_t* object = NULL;
    int   num_objects      = 0;
    int   object_count     = 0;

    // If no request path is given use an empty one
    if ( request_path == NULL )
    {
        request_path = "";
    }

    while( TRUE )
    {
        char* request_url = NULL;

        if ( object == NULL )
        {
            // Initial request no marker needed
            request_url = mosso_construct_request_url( mosso, request_path, MOSSO_PATH_TYPE_PATH, NULL );
        }
        else
        {
            // We have fired a request before, therefore a marker needs to be
            // appended.
            request_url = mosso_construct_request_url( mosso, request_path, MOSSO_PATH_TYPE_PATH, object->name );
        }

        printf( "Provided Path: %s\n", request_path );
        printf( "Requesting: %s\n", request_url );

        if ( ( response_code = simple_curl_request_get( request_url, &response_body, NULL, mosso->auth_headers ) ) != 200 )
        {
            // Something different than a 200 has been returned this might
            // indicate an error.
            switch ( response_code ) 
            {
                case 204:
                    set_error( MOSSO_ERROR_NOCONTENT, "No objects found." );
                break;
                default:
                    set_error( response_code, "Statuscode: %ld, Response body: %s", response_code, response_body );
            }
            
            free( response_body );
            free( request_url );

            if ( count != NULL )
            {
                *count = 0;
            }
            return NULL;
        }
        free( request_url );

        {
            // Add the objects to the list
            int type = ( strlen( request_path ) == 0 ) ? MOSSO_OBJECT_TYPE_CONTAINER : MOSSO_OBJECT_TYPE_OBJECT_OR_VDIR;
            char* prefix    = NULL;
            if ( strlen( request_path ) == 0 || strcmp( request_path, "/" ) == 0 )
            {
                // The prefix is a simple slash
                asprintf( &prefix, "/" );
            }
            else
            {
                // The prefix is a slash followed by the container name followed by a slash
                char* container = mosso_container_from_request_path( request_path );
                asprintf( &prefix, "/%s/", container );
                free( container );
            }
            object = mosso_create_object_list_from_response_body( object, response_body, prefix, type, &num_objects );
            free( prefix );
        }

        free( response_body );

        object_count += num_objects;

        if ( num_objects < 10000 )
        {
            // Objects are retrieved in chunks of 10000 objects max. Therefore
            // if the retrieved object count is lower than this the transfer is
            // finished.
            break;
        }
    };

    // Set the number of retrieved objects if the provided storage variable is
    // not NULL
    if ( count != NULL )
    {
        *count = object_count;
    }

    return object;
}

/**
 * Create a new directory inside your mosso cloudfs
 *
 * If a root directory aka something like "/foo" is specified a new container
 * will be created.
 * 
 * If a directory nested more deeply aka something like "/foo/bar" is
 * specified an new virtual directory in the specified container will be
 * created. This means the entry will be created as a normal mosso object and
 * the content-type will be set to "application/directory". This allows the
 * later retrieval using mosso's virtualpath parameter path.
 *
 * The function does not work recursively. If you want to create the directory
 * "/foo/bar/baz", the directories "/foo" and "/foo/bar" need to be already
 * existant.
 *
 * In case of success TRUE is returned.
 *
 * In case of an error FALSE is returned and the error string and code is
 * filled with appropriate information, which can be retrieved using the
 * mosso_error and mosso_error_code functions.
 */
int mosso_create_directory( mosso_connection_t* mosso, char* request_path ) 
{
    long response_code = 0;

    // A path type file request is made to ensure the virtual path parameter is
    // not used. The virtual directory should be specified like a normal file
    // for creation.
    char* request_url = mosso_construct_request_url( mosso, request_path, MOSSO_PATH_TYPE_FILE, NULL );

    printf( "Requesting: PUT %s\n", request_url );

    simple_curl_header_t* header = simple_curl_header_copy( mosso->auth_headers );
    header = simple_curl_header_add( header, "Content-Length", "0" );
    header = simple_curl_header_add( header, "Content-Type", "application/directory" );
    
    if ( ( response_code = simple_curl_request_put( request_url, NULL, NULL, NULL, header ) ) != 201 ) 
    {
        switch( response_code ) 
        {
            case 202:
                set_error( MOSSO_ERROR_ACCEPTED, "The directory does already exist." );                
            break;
                default:
                    set_error( response_code, "Statuscode: %ld", response_code );
        }

        simple_curl_header_free_all( header );
        free( request_url );
        return FALSE;
    }
    
    simple_curl_header_free_all( header );
    free( request_url );
    return TRUE;
}

/**
 * Delete an object on the connected mosso storage
 *
 * Delete an arbitraty object from the mosso storage.
 *
 * The specified object may either be a mosso object, a container or a virtual path node.
 * 
 * Recursive deletion is not handled in any way. If you try to delete a
 * non-empty container an error will be thrown. If you try to delete a virtual
 * directory with "content" still in it the virtual directory node will be
 * deleted. Its contents will still remain existant, but may not be easily
 * accessible until the virtual node is recreated. Take care you do not delete
 * non-empty directories.
 * 
 * On error FALSE will be returned and the error code and string is set
 * accordingly.
 */
int mosso_delete_object( mosso_connection_t* mosso, char* request_path ) 
{
    long response_code = 0;

    // The deletion is done on a file base. Virtual directories are only files
    // with a special content type.
    char* request_url = mosso_construct_request_url( mosso, request_path, MOSSO_PATH_TYPE_FILE, NULL );

    printf( "Requesting: DELETE %s\n", request_url );
    
    if ( ( response_code = simple_curl_request_delete( request_url, NULL, mosso->auth_headers ) ) != 204 ) 
    {
        switch( response_code ) 
        {
            case 404:
                set_error( MOSSO_ERROR_NOTFOUND, "The object could not be found." );                
            break;
                default:
                    set_error( response_code, "Statuscode: %ld", response_code );
        }

        free( request_url );
        return FALSE;
    }
    
    free( request_url );
    return TRUE;
}

/**
 * Isolate all metadata tags available in a list of given simple_curl_headers.
 *
 * If no tags are defined in the given header information NULL will be
 * returned. This is not considered an error, therefore no error information
 * will be set.
 */
static mosso_tag_t* mosso_create_tag_list_from_headers( simple_curl_header_t* header ) 
{
    mosso_tag_t* tag = NULL;
    simple_curl_header_t* cur = header;
    while ( cur != NULL ) 
    {
        // Scan for the mosso header indicating a tag
        if ( strstr( cur->key, "X-Object-Meta-" ) != NULL ) 
        {
            // Found a meta tag. Add it to the list
            tag = mosso_tag_add( tag, cur->key + 14, cur->value );
        }
        cur = cur->next;
    }
    return tag;
}

/**
 * Retrieve all the available meta information stored for a given request_path.
 *
 * If the object could not be found or any other error occurs NULL is returned
 * and the error information set accordingly.
 */
mosso_object_meta_t* mosso_get_object_meta( mosso_connection_t* mosso, char* request_path ) 
{
    long response_code = 0;
    simple_curl_header_t* response_header = NULL;
    char* request_url = mosso_construct_request_url( mosso, request_path, MOSSO_PATH_TYPE_FILE, NULL );

    if ( ( response_code = simple_curl_request_head( request_url, &response_header, mosso->auth_headers ) ) != 204 ) 
    {
        switch( response_code ) 
        {
            case 404:
                set_error( MOSSO_ERROR_NOTFOUND, "The object could not be found." );                
            break;
                default:
                    set_error( response_code, "Statuscode: %ld", response_code );
        }
        simple_curl_header_free_all( response_header );
        free( request_url );
        return NULL;
    }
    free( request_url );

    // Create the new meta object structure based on the retrieved information.
    {
        char* tmp = NULL;
        mosso_object_meta_t* meta = mosso_object_meta_init();

        meta->name         = mosso_name_from_request_path( request_path );
        meta->request_path = strdup( request_path );

        // Isolate the content type from the header list. The default in case
        meta->content_type = (
            ( ( tmp = simple_curl_header_get_by_key( response_header, "Content-Type" ) ) == NULL ) 
            ? ( strdup( "text/plain" ) ) 
            : ( strdup( tmp ) ) 
        );

        // Determine the type of the retrieved object meta data 
        {
            char* container = mosso_container_from_request_path( request_path );
            char* name      = mosso_name_from_request_path( request_path );

            if ( strcmp( container, name ) == 0 )
            {
                // If the container and the name are the same we have looked up
                // a container.
                meta->type = MOSSO_OBJECT_TYPE_CONTAINER;                
            }
            else 
            {
                // The requested object is not a container, therefore it might
                // be a virtual directory or a real mosso object.
                if ( strcmp( meta->content_type, "application/directory" ) == 0 ) 
                {
                    // This is a virtual directory node
                    meta->type = MOSSO_OBJECT_TYPE_VDIR;
                }
                else 
                {
                    meta->type = MOSSO_OBJECT_TYPE_OBJECT;
                }
            }
        }

        // Isolate the checksum from the header list. If it is not found a byte
        // array of zeros is used, which is created during the meta struct
        // initialization.
        {
            char* checksum_string = simple_curl_header_get_by_key( response_header, "Etag" );
            // If the checksum_string is NULL nothing needs to be done, as the
            // init value for the checksum after meta structure creation is
            // already a zero byte array.
            if ( checksum_string != NULL ) 
            {
                // Read the provided hex string and create a byte array out of
                // it.
                unsigned int md5[16];
                int i = 0;
                sscanf( checksum_string, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                   &md5[15], &md5[14], &md5[13], &md5[12], &md5[11], &md5[10], &md5[9], &md5[8], &md5[7], &md5[6], &md5[5], &md5[4], &md5[3], &md5[2], &md5[1], &md5[0]
                );                
                for ( i=0; i<16; ++i ) 
                {
                    meta->checksum[i] = (char)md5[i];
                }
            }
        }

        // Determine the size of the object
        {
            if ( meta->type == MOSSO_OBJECT_TYPE_CONTAINER ) 
            {
                // A container provides its size in a special header
                meta->size = atoll( simple_curl_header_get_by_key( response_header, "X-Container-Bytes-Used" ) );                
            }
            else 
            {
                // The Content-Length header is used or 0 if it is not provided.
                meta->size = (  
                    ( ( tmp = simple_curl_header_get_by_key( response_header, "Content-Length" ) ) == NULL ) 
                    ? ( 0 ) 
                    : ( atoll( tmp ) ) 
                );
            }
        }

        // The object count is currently only provided for containers sending
        // the "X-Container-Object-Count" header. If this header is not present
        // 0 will be assumed.
        meta->object_count = (  
            ( ( tmp = simple_curl_header_get_by_key( response_header, "X-Container-Object-Count" ) ) == NULL ) 
            ? ( 0 ) 
            : ( atoll( tmp ) ) 
        );

        // Try to isolate possibly available tags
        meta->tag = mosso_create_tag_list_from_headers( response_header );

        // Try to isolate the mtime
        {
            char* mtime = simple_curl_header_get_by_key( response_header, "Last-Modified" );
            // If it is NULL the associated mtime struct will simply be null in
            // the meta struct.
            if( mtime != NULL ) 
            {
                char* old_locale = setlocale( LC_TIME, NULL );
                setlocale( LC_TIME, "en_US" );
                meta->mtime = (struct tm*)smalloc( sizeof( struct tm ) );
                if ( strptime( mtime, "%a, %d %b %Y %H:%M:%S %Z", meta->mtime ) == 0 ) 
                {
                    setlocale( LC_TIME, old_locale );
                    free( meta->mtime );
                    meta->mtime = NULL;
                }
                setlocale( LC_TIME, old_locale );
            }
        }

        simple_curl_header_free_all( response_header );
        return meta;
    }
    
    return NULL;
}

/**
 * Free a given mosso connection structure
 */
void mosso_cleanup( mosso_connection_t* mosso )
{
    if ( mosso != NULL )
    {
        ( mosso->username != NULL )           ? free( mosso->username )                            : NULL;
        ( mosso->key != NULL )                ? free( mosso->key )                                 : NULL;
        ( mosso->storage_token != NULL )      ? free( mosso->storage_token )                       : NULL;
        ( mosso->auth_token != NULL )         ? free( mosso->auth_token )                          : NULL;
        ( mosso->storage_url != NULL )        ? free( mosso->storage_url )                         : NULL;
        ( mosso->cdn_management_url != NULL ) ? free( mosso->cdn_management_url )                  : NULL;
        ( mosso->auth_headers != NULL )       ? simple_curl_header_free_all( mosso->auth_headers ) : NULL;
        free( mosso );
    }
}

