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
#include <regex.h>
#include <curl/curl.h>

#include "simple_curl.h"

/**
 * Stream used to transport all the needed data between different write_header
 * calls
 */
typedef struct 
{
    simple_curl_header_t* ptr;
    int length;
} simple_curl_receive_header_stream_t;

/**
 * Structure holding all information needed to be transported between different
 * calls to the write_body function, to store all needed information of the
 * received body content.
 */
typedef struct 
{
    char* ptr;
    int length;
} simple_curl_receive_body_t;


static char* error_string = NULL;
#define set_error(e, ...) (((error_string != NULL) ? free(error_string) : NULL), asprintf( &error_string, e, ##__VA_ARGS__ ))
char* simple_curl_error() { return error_string; }

static size_t simple_curl_write_body( void *ptr, size_t size, size_t nmemb, void *stream );
static size_t simple_curl_write_header( void *ptr, size_t size, size_t nmemb, void *stream );
static simple_curl_receive_body_t* simple_curl_receive_body_init();
static void simple_curl_receive_body_free( simple_curl_receive_body_t* body );
static simple_curl_receive_header_stream_t* simple_curl_receive_header_stream_init();
static void simple_curl_receive_header_stream_free( simple_curl_receive_header_stream_t* stream );
static void simple_curl_prepare_curl_headers( simple_curl_header_t* headers, struct curl_slist** curl_headers );


/**
 * Callback function for cURL called every time new data has arrived
 *
 * This function is used internally for simple_curl calls to handle data
 * retrieval of body data and store it to a dynamically allocated memory block.
 */
static size_t simple_curl_write_body( void *ptr, size_t size, size_t nmemb, void *stream ) 
{
    int new_length = 0;
    simple_curl_receive_body_t* recv = (simple_curl_receive_body_t*)stream;
    recv->ptr = (char*)srealloc( recv->ptr, ( new_length = ( size * nmemb ) + recv->length ) + 1 );
    memset( recv->ptr + recv->length, 0, (  size * nmemb ) + 1 );
    memcpy( (recv->ptr) + recv->length, ptr, size * nmemb );
    recv->length = new_length;
    return (size*nmemb);
}

/**
 * Callback function for cURL called every time a new header line is received
 *
 * This function is used internally to store newly received headers to a linked
 * list of key/value pairs, which are splitted using a simple regex.
 *
 * cURL calls this callback method one for each new header line received.
 */
static size_t simple_curl_write_header( void *ptr, size_t size, size_t nmemb, void *stream ) 
{
    regex_t re;
    regmatch_t matches[4];
    int error = 0;

    simple_curl_receive_header_stream_t* header_stream = ( simple_curl_receive_header_stream_t* )stream;    

    // Using a simple regex the header is splitted into a key value pair.
    memset( &matches, 0, 4 * sizeof( regmatch_t ) );
    
    if ( ( error = regcomp( &re, "^([^:]+): ([^\r\n]*)[\r\n]*$", REG_EXTENDED | REG_ICASE | REG_NEWLINE ) ) != 0 ) 
    {
        // An error occured during the compile of the regex.
        // This should never happen. But if it does we simply skip this
        // header extraction call
        return size*nmemb;
    }
    
    // Create a null terminated string based on the data given by curl and
    // match it against the regex
    {
        char* key    = NULL;
        char* value  = NULL;
        char* source = (char*)smalloc( (size*nmemb) + 1 );
        memcpy( source, ptr, size*nmemb );
        
        if ( ( error = regexec( &re, source, 3, matches, 0 ) ) != 0 ) 
        {
            // Match failed. Just skip the given line.
            return size*nmemb;
        }

        // Allocate the strings needed to hold subexpression 1 and 2 ( Our
        // key / value pair )
        key   = (char*)smalloc( sizeof( char ) * ( matches[1].rm_eo - matches[1].rm_so + 1 ) );
        value = (char*)smalloc( sizeof( char ) * ( matches[2].rm_eo - matches[2].rm_so + 1 ) );

        // Copy the matched strings to their new homes
        memcpy( key,   source + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so );
        memcpy( value, source + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so );

        // Store key value strings in the struct and free the temporary
        // strings.
        header_stream->ptr = simple_curl_header_add( header_stream->ptr, key, value );
    }

    ++(header_stream->length);
    return size * nmemb;
}

/**
 * Initialize and return a new receive_body struct
 *
 * This struct which is capable of holding a char* pointer as data and the
 * current length of the pointer used to manage reallocation properly.
 */
static simple_curl_receive_body_t* simple_curl_receive_body_init() 
{
    simple_curl_receive_body_t* body = (simple_curl_receive_body_t*)smalloc( sizeof( simple_curl_receive_body_t ) );
    body->ptr = malloc( sizeof( char ) );
    body->ptr[0] = 0;
    body->length = 0;
    return body;
}

/**
 * Free a receive body struct
 *
 * The struct itself as well as the internally managed string is freed upon
 * calling this.
 */
static void simple_curl_receive_body_free( simple_curl_receive_body_t* body ) 
{
    if ( body != NULL ) 
    {
        (body->ptr != NULL) ? free( body->ptr ) : NULL;
        free( body );
    }
}

/**
 * Initialize a new header_stream.
 *
 * The header stream struct is used to store a pointer to the last linked list
 * elements of headers as well as the number of header entries already stored.
 */
static simple_curl_receive_header_stream_t* simple_curl_receive_header_stream_init() 
{
    simple_curl_receive_header_stream_t* stream = (simple_curl_receive_header_stream_t*)smalloc( sizeof( simple_curl_receive_header_stream_t ) );
    stream->ptr = NULL;
    stream->length = 0;
    return stream;
}

/**
 * Free a header_stream
 *
 * Free a header stream struct including the linked list inside it storing the
 * header information. Every linked list element including all its data will be
 * freed as well.
 */
static void simple_curl_receive_header_stream_free( simple_curl_receive_header_stream_t* stream ) 
{
    if ( stream != NULL ) 
    {
        (stream->ptr != NULL) ? simple_curl_header_free_all( stream->ptr ) : NULL;
        free( stream );
    }
}

static void simple_curl_prepare_curl_headers( simple_curl_header_t* headers, struct curl_slist** curl_headers ) 
{
    simple_curl_header_t* cur = headers->root;
    while( cur != NULL ) 
    {
        char* data = NULL;
        asprintf( &data, "%s: %s", cur->key, cur->value );
        (*curl_headers) = curl_slist_append( (*curl_headers), data );
        free( data );
        cur = cur->next;
    }
}

/**
 * Add a new header entry to a header linked list
 *
 * A new header key value pair is added to supplied linked list of already
 * available headers. 
 *
 * The provided header element has to be the last element of the current linked
 * list or NULL in which case a new list is created.
 *
 * The newly created last linked list element will be returned. This element
 * needs to be supplied on the next call to this function.
 *
 * The supplied key value pair need to be null-terminated strings, which will
 * be copied for storage.
 *
 * simple_curl_header_free_all needs to be called to free the occupied data.
 */
simple_curl_header_t* simple_curl_header_add( simple_curl_header_t* header, char* key, char* value ) 
{
    // Initialize a new header entry
    simple_curl_header_t* new_header = (simple_curl_header_t*)smalloc( sizeof( simple_curl_header_t ) );

    // Copy the data to it
    new_header->key   = strdup( key );
    new_header->value = strdup( value );

    // Set the root and next accordingly
    new_header->next = NULL;
    if ( header == NULL ) 
    {
        // This was an initialization the newly created header is root
        new_header->root = new_header;
    }
    else 
    {
        new_header->root = header->root;
        header->next = new_header;
    }
    
    return new_header;
}

/**
 * Retrieve a header value by a given key from a header list
 *
 * If the key does not exist in the list NULL is returned instead of the
 * corresponding string.
 */
char* simple_curl_header_get_by_key( simple_curl_header_t* headers, char* key ) 
{
    simple_curl_header_t* cur = headers->root;
    while( cur != NULL ) 
    {
        if ( strcmp( cur->key, key ) == 0 ) 
        {
            return cur->value;
        }
        cur = cur->next;
    }
    return NULL;
}

/**
 * Free a header linked list
 *
 * This function frees a complete linked list, given any element inside the
 * list. The list structure itself as well as allocated strings inside are
 * freed.
 */
void simple_curl_header_free_all( simple_curl_header_t* header ) 
{
    if ( header != NULL ) 
    {
        simple_curl_header_t* next = header->root;
        while( next != NULL ) 
        {
            simple_curl_header_t* cur = next;
            next = next->next;
            free( cur->key );
            free( cur->value );
            free( cur );
        }
    }
}

/**
 * Url encode a given string
 *
 * Only ASCII letters and digits will be returned unencoded. Everything else
 * will be encoded using the %hexcode notation.
 *
 * If size is set to 0 the string length is determined using strlen
 *
 * Curls internal implementation seems to fail in some cases therefore this one
 * is provided here.
 *
 * The returned string has to be freed, if it is not needed any longer.
 */
char* simple_curl_urlencode( char* url, int size ) 
{
    char* hex_code = "0123456789abcdef";

    if ( size == 0 ) 
    {
        size = strlen( url );
    }
    
    // For the initial space requirement assume the worst case, aka every char
    // needs to be encoded. The string be reallocated after the encoding is
    // complete to free the not space not needed.
    char* result = (char*)smalloc( sizeof( char ) * ( ( size * 3 ) + 1 ) );

    char* cur        = url;
    char* cur_result = result;
    
    while( cur - url < size ) 
    {
        if ( ( (*cur) >= 'a' && (*cur) <= 'z' )
          || ( (*cur) >= 'A' && (*cur) <= 'Z' )
          || ( (*cur) >= '0' && (*cur) <= '9' ) ) 
        {
            // ASCII letter or digit. Simple write out the given character.
            *(cur_result++) = *cur;
        }
        else 
        {
            // Escaped character needs to be written.
            *(cur_result++) = '%';
            *(cur_result++) = hex_code[ (*cur) >> 4 ];
            *(cur_result++) = hex_code[ (*cur) & 15 ];
        }
        ++cur;
    }
    // Reallocate the string to occupy only the needed space.
    result = (char*)srealloc( result, sizeof( char ) * ( cur_result - result + 1 ) );
    return result;
}

/**
 * Execute a curl request using the given information
 *
 * Operation and url are mandatory informations.
 *
 * Operation is one of the following values:
 *  - SIMPLE_CURL_GET
 *  - SIMPLE_CURL_POST
 *  - SIMPLE_CURL_PUT
 *  - SIMPLE_CURL_HEAD
 *  - SIMPLE_CURL_DELETE
 *
 * Url is the url where the request is send to.
 *
 * Response_body will be filled with the body content of the response. It may
 * be NULL, in which case it is simple ignored and the received body content is
 * ignored.
 *
 * Response_header will be filled with a header linked list of received header
 * data splitted into key value pairs as defined by the used struct. If it is
 * NULL the headers will simply be ignored.
 *
 * Request_body is the body content to be send in the request. This is
 * especially interesting if the operation is not a default GET but a POST or a
 * PUT. It may be NULL in which case not request body will be transmitted.
 *
 * Request_headers is a linked list of header key/value pairs send to the
 * server within the request. It may be NULL in which case only the default
 * headers will be send.
 */
long simple_curl_request_complex( int operation, char* url, char** response_body, simple_curl_header_t** response_headers, char* request_body, simple_curl_header_t* request_headers ) 
{
    CURL* ch = NULL;
    struct curl_slist *curl_request_headers = NULL;
    char curl_error[CURL_ERROR_SIZE];
    simple_curl_receive_header_stream_t* received_header_stream = simple_curl_receive_header_stream_init();
    simple_curl_receive_body_t*          received_body          = simple_curl_receive_body_init();
    long response_code = 0L;
    
    ch = curl_easy_init();
    curl_easy_setopt( ch, CURLOPT_NOPROGRESS, 1 );
    curl_easy_setopt( ch, CURLOPT_ERRORBUFFER, curl_error );

    // Set the given url
    curl_easy_setopt( ch, CURLOPT_URL, url ); 
    
    // Set all the needed callbacks to receive the body and header data
    curl_easy_setopt( ch, CURLOPT_WRITEFUNCTION, simple_curl_write_body );
    curl_easy_setopt( ch, CURLOPT_WRITEDATA, (void*)received_body );
    curl_easy_setopt( ch, CURLOPT_HEADERFUNCTION, simple_curl_write_header );
    curl_easy_setopt( ch, CURLOPT_HEADERDATA, (void*)received_header_stream );

    //
    // @TODO: set appropriate read functions here, as well as the correct
    //        request type currently GET is used by default.
    //
    
    if ( request_headers != NULL ) 
    {
        simple_curl_prepare_curl_headers( request_headers, &curl_request_headers );
        curl_easy_setopt( ch, CURLOPT_HTTPHEADER, curl_request_headers );
    }
    
    if ( curl_easy_perform( ch ) != 0 ) 
    {
        set_error( "%s", curl_error );
        curl_easy_cleanup( ch );
        simple_curl_receive_header_stream_free( received_header_stream );
        simple_curl_receive_body_free( received_body );
        ( curl_request_headers != NULL ) ? curl_slist_free_all( curl_request_headers ) : NULL;
        return 0;
    }

    curl_easy_getinfo( ch, CURLINFO_RESPONSE_CODE, &response_code );

    curl_easy_cleanup( ch );

    // Free the converted request headers if there are any
    ( curl_request_headers != NULL ) ? curl_slist_free_all( curl_request_headers ) : NULL;
    
    if ( response_body == NULL ) 
    {
        // Destroy the received body
        simple_curl_receive_body_free( received_body );
    }
    else 
    {
        // Set the return value
        (*response_body) = received_body->ptr;
        // Free only the receive body struct without the inner string
        free( received_body );
    }

    if ( response_headers == NULL ) 
    {
        // Destroy the received headers
        simple_curl_receive_header_stream_free( received_header_stream );
    }
    else 
    {
        // Set the return value
        (*response_headers) = received_header_stream->ptr->root;
        // Free only the header stream struct not the internal list
        free( received_header_stream );
    }

    return response_code;
}
