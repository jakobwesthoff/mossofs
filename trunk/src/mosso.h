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

/**
 * Data structure to transport all mosso cloudspace connection related data
 * between different function calls.
 */
typedef struct 
{
    char* username;
    char* key;
    char* auth_token;
    char* storage_url;
    char* cdn_management_url;
} mosso_connection_t;


mosso_connection_t* mosso_init( char* username, char* key );
void mosso_cleanup( mosso_connection_t* mosso );
char* mosso_error();

#endif
