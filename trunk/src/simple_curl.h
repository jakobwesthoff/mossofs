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

/**
 * Stream used to transport all the needed data between different write_header
 * calls
 */
typedef struct 
{
    simple_curl_header_t* ptr;
    int length;
} simple_curl_receive_header_stream_t;


size_t simple_curl_write_body( void *ptr, size_t size, size_t nmemb, void *stream );
size_t simple_curl_write_header( void *ptr, size_t size, size_t nmemb, void *stream );
simple_curl_receive_body_t* simple_curl_receive_body_init();
void simple_curl_receive_body_free( simple_curl_receive_body_t* body );
simple_curl_receive_header_stream_t* simple_curl_receive_header_stream_init();
void simple_curl_receive_header_stream_free( simple_curl_receive_header_stream_t* stream );

#endif
