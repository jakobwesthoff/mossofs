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

static simple_curl_receive_header_t* simple_curl_receive_header_add( simple_curl_receive_header_t* header, void* data, size_t size );
static void simple_curl_receive_header_free_all( simple_curl_receive_header_t* header );

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

size_t simple_curl_write_header( void *ptr, size_t size, size_t nmemb, void *stream ) 
{
    simple_curl_receive_header_stream_t* header_stream = ( simple_curl_receive_header_stream_t* )stream;
    header_stream->ptr = simple_curl_receive_header_add( header_stream->ptr, ptr, size * nmemb );
    ++(header_stream->length);
    return size * nmemb;
}

simple_curl_receive_body_t* simple_curl_receive_body_init() 
{
    simple_curl_receive_body_t* body = (simple_curl_receive_body_t*)smalloc( sizeof( simple_curl_receive_body_t ) );
    body->ptr = malloc( sizeof( char ) );
    body->ptr[0] = 0;
    body->length = 0;
    return body;
}

void simple_curl_receive_body_free( simple_curl_receive_body_t* body ) 
{
    if ( body != NULL ) 
    {
        (body->ptr != NULL) ? free( body->ptr ) : NULL;
        free( body );
    }
}

simple_curl_receive_header_stream_t* simple_curl_receive_header_stream_init() 
{
    simple_curl_receive_header_stream_t* stream = (simple_curl_receive_header_stream_t*)smalloc( sizeof( simple_curl_receive_header_stream_t ) );
    stream->ptr = NULL;
    stream->length = 0;
    return stream;
}

void simple_curl_receive_header_stream_free( simple_curl_receive_header_stream_t* stream ) 
{
    if ( stream != NULL ) 
    {
        (stream->ptr != NULL) ? simple_curl_receive_header_free_all( stream->ptr ) : NULL;
        free( stream );
    }
}

static simple_curl_receive_header_t* simple_curl_receive_header_add( simple_curl_receive_header_t* header, void* data, size_t size ) 
{
    // Initialize a new header entry
    simple_curl_receive_header_t* new_header = (simple_curl_receive_header_t*)smalloc( sizeof( simple_curl_receive_header_t ) );
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

static void simple_curl_receive_header_free_all( simple_curl_receive_header_t* header ) 
{
    if ( header != NULL ) 
    {
        simple_curl_receive_header_t* next = header->root;
        while( next != NULL ) 
        {
            simple_curl_receive_header_t* cur = next;
            next = next->next;
            free( cur->ptr );
            free( cur );
        }
    }
}
