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
        set_error( "Statuscode: %ld, Response: %s", response_code, response_body );
        mosso_cleanup( (*mosso) );
        free( response_body );
        simple_curl_header_free_all( request_headers );
        simple_curl_header_free_all( response_headers );
        (*mosso) = NULL;
        return;
    }

    simple_curl_header_free_all( request_headers );
    
    {
        simple_curl_header_t* cur = response_headers;
        while( cur != NULL ) 
        {
            printf( "Key: %s, Value: %s\n", cur->key, cur->value );
            cur = cur->next;
        }
    }

    printf( "\nBody:\n%s\n", response_body );
    free( response_body );
    simple_curl_header_free_all( response_headers );
}


mosso_connection_t* mosso_init( char* username, char* key ) 
{
    mosso_connection_t* mosso = smalloc( sizeof( mosso_connection_t ) );
    mosso->username = strdup( username );
    mosso->key      = strdup( key );
    mosso_authenticate( &mosso );
    return mosso;
}

void mosso_cleanup( mosso_connection_t* mosso ) 
{
    if ( mosso != NULL ) 
    {
        ( mosso->username != NULL )           ? free( mosso->username )           : NULL;
        ( mosso->key != NULL )                ? free( mosso->key )                : NULL;
        ( mosso->auth_token != NULL )         ? free( mosso->auth_token )         : NULL;
        ( mosso->storage_url != NULL )        ? free( mosso->storage_url )        : NULL;
        ( mosso->cdn_management_url != NULL ) ? free( mosso->cdn_management_url ) : NULL;
        free( mosso );
    }
}

char* mosso_error() 
{
    return error_string;
}
