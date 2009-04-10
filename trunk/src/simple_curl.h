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

typedef struct 
{
    char* ptr;
    int length;
} simple_curl_receive_body_t;

typedef struct simple_curl_receive_header
{
    char* ptr;
    struct simple_curl_receive_header* next;
    struct simple_curl_receive_header* root;
} simple_curl_receive_header_t;

typedef struct 
{
    simple_curl_receive_header_t* ptr;
    int length;
} simple_curl_receive_header_stream_t;

size_t simple_curl_write_body( void *ptr, size_t size, size_t nmemb, void *stream );
size_t simple_curl_write_header( void *ptr, size_t size, size_t nmemb, void *stream );
simple_curl_receive_body_t* simple_curl_receive_body_init();
void simple_curl_receive_body_free( simple_curl_receive_body_t* body );
simple_curl_receive_header_stream_t* simple_curl_receive_header_stream_init();
void simple_curl_receive_header_stream_free( simple_curl_receive_header_stream_t* stream );

#endif
