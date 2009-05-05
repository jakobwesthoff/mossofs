#ifndef CACHE_H
#define CACHE_H

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

typedef void (*cache_object_free_func)( char* prefix, char* identifier, void* ptr );

typedef struct 
{
    GHashTable* hashtable;
    long ttl;
    cache_object_free_func object_free_func;
} cache_t;

cache_t* cache_new( long ttl, cache_object_free_func object_free_func );
void cache_free( cache_t* cache );
void cache_add_object( cache_t* cache, const char* prefix, const char* identifier, void* ptr );
void* cache_get_object( cache_t* cache, const char* prefix, const char* identifier );
void cache_remove_object( cache_t* cache, const char* prefix, const char* identifier );

#endif
