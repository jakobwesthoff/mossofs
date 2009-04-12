#ifndef SIMPLE_CURL_H
#define SIMPLE_CURL_H

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

#define SIMPLE_CURL_GET    0
#define SIMPLE_CURL_HEAD   1
#define SIMPLE_CURL_POST   2
#define SIMPLE_CURL_PUT    3
#define SIMPLE_CURL_DELETE 4

/**
 * Linked list element to store header lines
 *
 * next points to the next list element or null if the list ends here.
 *
 * root always points to the root element of the list to allow simple traversal
 * idenpendent of the given element.
 */
typedef struct simple_curl_header
{
    char* key;
    char* value;
    struct simple_curl_header* next;
    struct simple_curl_header* root;
} simple_curl_header_t;


char* simple_curl_error();
simple_curl_header_t* simple_curl_header_add( simple_curl_header_t* header, char* key, char* value );
char* simple_curl_header_get_by_key( simple_curl_header_t* headers, char* key );
void simple_curl_header_free_all( simple_curl_header_t* header );
char* simple_curl_urlencode( char* url, int size );
long simple_curl_request_complex( int operation, char* url, char** response_body, simple_curl_header_t** response_header, char* request_body, simple_curl_header_t* request_headers );

#endif
