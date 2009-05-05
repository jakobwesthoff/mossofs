#ifndef MOSSO_H
#define MOSSO_H

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

#define _GNU_SOURCE
#define TRUE  1
#define FALSE 0

#include <time.h>

#include "simple_curl.h"

/**
 * Error codes accessible through mosso_get_error() in case something bad happened.
 *
 * They have a close connection to the HTTP status codes returned by mossos
 * REST interface, therefore they are not simple numbered but have a certain
 * structure.
 */
#define MOSSO_ERROR_OK 200
#define MOSSO_ERROR_CREATED 201
#define MOSSO_ERROR_ACCEPTED 202
/* No content does not always imply a fatal error. Some operations just do not
 * give content, like a HEAD. In this case this reaction is perfectly normal.
 */
#define MOSSO_ERROR_NOCONTENT 204
#define MOSSO_ERROR_UNAUTHORIZED 401
#define MOSSO_ERROR_NOTFOUND 404
#define MOSSO_ERROR_DIRECTORY_NOT_EMPTY 409
#define MOSSO_ERROR_CHECKSUMMISMATCH 422

/**
 * Data structure to transport all mosso cloudspace connection related data
 * between different function calls.
 */
typedef struct
{
    char* username;
    char* key;
    char* storage_token;
    char* auth_token;
    char* storage_url;
    char* cdn_management_url;
    simple_curl_header_t* auth_headers;
} mosso_connection_t;


#define MOSSO_OBJECT_TYPE_CONTAINER      0
#define MOSSO_OBJECT_TYPE_OBJECT_OR_VDIR 1
#define MOSSO_OBJECT_TYPE_OBJECT         2
#define MOSSO_OBJECT_TYPE_VDIR           3

/**
 * Structure holding information about a specific mosso_object.
 *
 * A mosso object can be a container or a data object inside a container.
 *
 * The structure provides all neccessary means to create a linked list of such
 * objects easily, to represent container or object lisitings.
 */
typedef struct mosso_object
{
    char* name;
    char* request_path;
    int type;
    struct mosso_object* next;
    struct mosso_object* root;
} mosso_object_t;
 

/**
 * Structure holding information about a tag associated with any mosso object.
 */
typedef struct mosso_tag 
{
    char* key;
    char* value;
    struct mosso_tag* next;
    struct mosso_tag* root;
} mosso_tag_t;

/**
 * Structure representing meta data stored for a given object
 */
typedef struct 
{
    char* name;
    char* request_path;
    int type;
    char* content_type;
    unsigned char checksum[16];
    struct tm* mtime;
    uint64_t size;    
    uint64_t object_count;
    mosso_tag_t* tag;
} mosso_object_meta_t;


mosso_connection_t* mosso_init( char* username, char* key );
void mosso_object_free_all( mosso_object_t* object );
mosso_object_t* mosso_list_objects( mosso_connection_t* mosso, char* request_path, int* count );
int mosso_create_directory( mosso_connection_t* mosso, char* request_path ); 
void mosso_cleanup( mosso_connection_t* mosso );
mosso_tag_t* mosso_tag_add( mosso_tag_t* tag, char* key, char* value ); 
mosso_tag_t* mosso_tag_replace_or_add( mosso_tag_t* tag, char* key, char* value );
void mosso_tag_free_all( mosso_tag_t* tag );
char* mosso_tag_get_by_key( mosso_tag_t* tag, char* key );
mosso_object_meta_t* mosso_get_object_meta( mosso_connection_t* mosso, char* request_path ); 
size_t mosso_read_object( mosso_connection_t* mosso, char* request_path, size_t size, char* buffer, off_t offset ); 
void mosso_object_meta_free( mosso_object_meta_t* meta );

char* mosso_error_string();
long mosso_error();

#endif
