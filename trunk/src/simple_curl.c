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

#include "simple_curl.h"

static simple_curl_header_t* simple_curl_header_add( simple_curl_header_t* header, char* data, size_t size );
static void simple_curl_header_free_all( simple_curl_header_t* header );

/**
 * Callback function for cURL called every time new data has arrived
 *
 * This function is used internally for simple_curl calls to handle data
 * retrieval of body data and store it to a dynamically allocated memory block.
 */
size_t simple_curl_write_body( void *ptr, size_t size, size_t nmemb, void *stream ) 
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
 * list.
 *
 * cURL calls this callback method one for each new header line received.
 */
size_t simple_curl_write_header( void *ptr, size_t size, size_t nmemb, void *stream ) 
{
    simple_curl_receive_header_stream_t* header_stream = ( simple_curl_receive_header_stream_t* )stream;
    header_stream->ptr = simple_curl_header_add( header_stream->ptr, (char*)ptr, size * nmemb );
    ++(header_stream->length);
    return size * nmemb;
}

/**
 * Initialize and return a new receive_body struct
 *
 * This struct which is capable of holding a char* pointer as data and the
 * current length of the pointer used to manage reallocation properly.
 */
simple_curl_receive_body_t* simple_curl_receive_body_init() 
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
void simple_curl_receive_body_free( simple_curl_receive_body_t* body ) 
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
simple_curl_receive_header_stream_t* simple_curl_receive_header_stream_init() 
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
void simple_curl_receive_header_stream_free( simple_curl_receive_header_stream_t* stream ) 
{
    if ( stream != NULL ) 
    {
        (stream->ptr != NULL) ? simple_curl_header_free_all( stream->ptr ) : NULL;
        free( stream );
    }
}

/**
 * Add a new header entry to a header linked list
 *
 * A new header string is added to supplied linked list of already available
 * headers. 
 *
 * The provided header element has to be the last element of the current linked
 * list or NULL in which case a new list is created.
 *
 * The newly created last linked list element will be returned. This element
 * needs to be supplied on the next call to this function.
 *
 * The supplied data string is copyied and stored.
 * simple_curl_receive_header_free_all needs to be called to free the space
 * occupied.
 *
 * If size is 0 data is assumed to be a null-terminated string. 
 */
static simple_curl_header_t* simple_curl_header_add( simple_curl_header_t* header, char* data, size_t size ) 
{
    // Calculate size if it is not given
    if ( size == 0 ) 
    {
        size = strlen( data );
    }

    // Initialize a new header entry
    simple_curl_header_t* new_header = (simple_curl_header_t*)smalloc( sizeof( simple_curl_header_t ) );
    // Copy the data to it
    new_header->ptr = (char*)smalloc( size + 1 );
    memcpy( new_header->ptr, data, size );

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
 * Free a header linked list
 *
 * This function frees a complete linked list, given any element inside the
 * list. The list structure itself as well as allocated strings inside are
 * freed.
 */
static void simple_curl_header_free_all( simple_curl_header_t* header ) 
{
    if ( header != NULL ) 
    {
        simple_curl_header_t* next = header->root;
        while( next != NULL ) 
        {
            simple_curl_header_t* cur = next;
            next = next->next;
            free( cur->ptr );
            free( cur );
        }
    }
}
