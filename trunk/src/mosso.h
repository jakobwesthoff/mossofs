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


#define MOSSO_OBJECT_TYPE_CONTAINER 0
#define MOSSO_OBJECT_TYPE_OBJECT    1

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


mosso_connection_t* mosso_init( char* username, char* key );
void mosso_object_free_all( mosso_object_t* object );
mosso_object_t* mosso_list_objects( mosso_connection_t* mosso, char* request_path, int* count );
void mosso_cleanup( mosso_connection_t* mosso );

char* mosso_error_string();
long mosso_error();

#endif
