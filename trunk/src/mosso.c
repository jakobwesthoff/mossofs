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
    CURL* ch = NULL;
    struct curl_slist *headers = NULL;
    simple_curl_receive_body_t* body            = simple_curl_receive_body_init();
    simple_curl_receive_header_stream_t* header = simple_curl_receive_header_stream_init();

    ch = curl_easy_init();
    curl_easy_setopt( ch, CURLOPT_URL, "https://api.mosso.com/auth" );    
    {
        char *header;
        asprintf( &header, "X-Auth-User: %s", (*mosso)->username );
        headers = curl_slist_append( headers, header );
        free( header );
    }
    {
        char *header;
        asprintf( &header, "X-Auth-Key: %s", (*mosso)->key );
        headers = curl_slist_append( headers, header );
        free( header );
    }

    curl_easy_setopt( ch, CURLOPT_HTTPHEADER, headers );

    curl_easy_setopt( ch, CURLOPT_WRITEFUNCTION, simple_curl_write_body );
    curl_easy_setopt( ch, CURLOPT_WRITEDATA, (void*)body );
    curl_easy_setopt( ch, CURLOPT_HEADERFUNCTION, simple_curl_write_header );
    curl_easy_setopt( ch, CURLOPT_HEADERDATA, (void*)header );

    curl_easy_perform( ch );

    {
        long code = 0L;
        curl_easy_getinfo( ch, CURLINFO_RESPONSE_CODE, &code );
        if ( code != 204 ) 
        {            
            set_error( "Statuscode: %ld, Response: %s", code, body->ptr );
            mosso_cleanup( (*mosso) );
            simple_curl_receive_body_free( body );
            simple_curl_receive_header_stream_free( header );
            (*mosso) = NULL;
            return;
        }
    }
    
    curl_easy_cleanup( ch );
    curl_slist_free_all( headers );

    
    {
        simple_curl_receive_header_t* cur = header->ptr->root;
        while( cur != NULL ) 
        {
            printf( "%s\n", cur->ptr );
            cur = cur->next;
        }
    }

    printf( "\nBody:\n%s\n", body->ptr );
    simple_curl_receive_body_free( body );
    simple_curl_receive_header_stream_free( header );
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
