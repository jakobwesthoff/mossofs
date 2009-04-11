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
#include <curl/curl.h>

#include "mosso.h"
#include "simple_curl.h"
#include "salloc.h"

static char* error_string = NULL;
#define set_error(e, ...) (((error_string != NULL) ? free(error_string) : NULL), asprintf( &error_string, e, ##__VA_ARGS__ ))
char* mosso_error() { return error_string; }


static void mosso_authenticate( mosso_connection_t** mosso ); 
static mosso_object_t* mosso_create_object_list_from_response_body( mosso_object_t* object, char* response_body, char* path_prefix, int type, int* num );
static mosso_object_t* mosso_object_add( mosso_object_t* object, char* name, char* request_path, int type );


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

    if ( ( response_code = simple_curl_request_complex( SIMPLE_CURL_GET, "https://api.mosso.com/auth", &response_body, &response_headers, NULL, request_headers ) ) != 204 ) 
    {
        // Mosso responded with something different than a 204. This indicates
        // an error.
        set_error( "Statuscode: %ld, Response: %s", response_code, response_body );
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
 * Retrieve a list of available containers from the mosso service.
 *
 * The returned object will be a linked list of objects.
 * 
 * If an error occured NULL will be returned and the error string will be set
 * accordingly.
 */
mosso_object_t* mosso_list_containers( mosso_connection_t* mosso ) 
{
    char* response_body    = NULL;
    int   response_code    = 0;
    mosso_object_t* object = NULL;
    int   num_objects      = 0;    

    while( TRUE ) 
    {
        char* request_url = NULL;
        if ( object == NULL ) 
        {
            // First request no marker needed
            request_url = strdup( mosso->storage_url );
        }
        else 
        {
            // We have fired a request before, therefore a marker needs to be
            // set.
            char* escaped_marker = curl_escape( object->name, 0 );
            asprintf( &request_url, "%s?marker=%s", mosso->storage_url, escaped_marker );
            curl_free( escaped_marker );
        }

        if ( ( response_code = simple_curl_request_complex( SIMPLE_CURL_GET, request_url, &response_body, NULL, NULL, mosso->auth_headers ) ) != 200 ) 
        {
            if ( response_code == 204 ) 
            {
                set_error( "No containers found." );
            }
            else 
            {
                set_error( "Statuscode: %ld, Response: %s", response_code, response_body );
            }

            free( response_body );
            free( request_url );
            return NULL;
        }
        free( request_url );

        object = mosso_create_object_list_from_response_body( object, response_body, "/", MOSSO_OBJECT_TYPE_CONTAINER, &num_objects );

        free( response_body );

        if ( num_objects < 10000 ) 
        {
            // Objects are retrieved in chunks of 10000 objects max. Therefore
            // if the retrieved object count is lower than this the transfer is
            // finished.
            break;
        }
    };
    
    return object;
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

