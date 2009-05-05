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
#include <time.h>
#include <glib.h>

#include "salloc.h"
#include "cache.h"

static void cache_key_free( gpointer key ); 
static void cache_object_data_free( gpointer key, gpointer obj, gpointer user_data );
static void cache_value_free( gpointer value );

/**
 * Structure to store one cached object including all the needed meta
 * information.
 */
typedef struct
{
    char* prefix;
    char* identifier;
    time_t timestamp;
    void* ptr;
} cache_object_t;

/**
 * Create a new cache structure and return it
 *
 * The provided time to live will be used to ensure a proper lifespan is
 * applied to each of the stored cache objects. The time is specified in
 * seconds.
 *
 * A function may be supplied which is called every time a cached objects needs
 * to be freed. If NULL is supplied here no free function will be called for
 * cached objects on destruction.
 */
cache_t* cache_new( long ttl, cache_object_free_func object_free_func ) 
{
    cache_t* cache = snew( cache_t );

    cache->ttl              = ttl;
    cache->object_free_func = object_free_func;
    cache->hashtable = g_hash_table_new_full( 
        g_str_hash,
        g_str_equal,
        cache_key_free,
        cache_value_free
    );    
}

/**
 * Free the given cache structure.
 *
 * The structure as well as all internally allocated information entities are
 * freed. 
 *
 * If a object_free_func has been supplied during creation it will be called
 * for each of the cached objects.
 */
void cache_free( cache_t* cache ) 
{
    if ( cache->object_free_func != NULL ) 
    {
        g_hash_table_foreach( cache->hashtable, cache_object_data_free, (gpointer)cache->object_free_func );
    }
    g_hash_table_destroy( cache->hashtable );
    free( cache );
}

/**
 * Free the key string used to store the cached information.
 */
static void cache_key_free( gpointer key ) 
{
    free( key );
}

/**
 * Function used to free an arbitrary cache object data using a user defined
 * free function.
 *
 * The free function needs to be supplied as a cache_object_free_func pointer
 * and been put into the user_data pointer.
 */
static void cache_object_data_free( gpointer key, gpointer value, gpointer user_data ) 
{
    cache_object_free_func free_func = ( cache_object_free_func )user_data;
    cache_object_t* obj = ( cache_object_t* )value;
    free_func( obj->prefix, obj->identifier, obj->ptr );
}

/**
 * Free the cache_object_t structure associated with a key
 */
static void cache_value_free( gpointer value ) 
{
    cache_object_t* obj = ( cache_object_t* )value;
    ( obj->prefix != NULL ) ? ( free( obj->prefix ) ) : NULL;
    ( obj->identifier != NULL ) ? ( free( obj->identifier ) ) : NULL;
    free( obj );
}

/**
 * Add an arbitrary object to the cache using a defined prefix and identifier.
 *
 * The combination of prefix and identifier needs to uniquely identify the
 * cached data.
 *
 * In case the prefix/identifier pair is already set in the cache it will be
 * replaced with the new data provided. If a free function has been supplied
 * during creation it will be called.
 *
 * If the prefix/identifier pair does not exist yet it will be created and the
 * value will be stored.
 */
void cache_add_object( cache_t* cache, const char* prefix, const char* identifier, void* ptr ) 
{
    cache_object_t* old_obj = NULL;
    void* old_ptr = NULL;
    char* key = NULL;

    cache_object_t* obj = snew( cache_object_t );
    obj->timestamp = time( NULL );
    obj->prefix = strdup( prefix );
    obj->identifier = strdup( identifier );
    obj->ptr = ptr;

    // Add the newly created object to the storage hashmap
    asprintf( &key, "%s/%s", prefix, identifier );
    
    // Retrieve the possibly already defined cache entry
    old_obj = g_hash_table_lookup( cache->hashtable, key );
    if ( old_obj != NULL && cache->object_free_func != NULL ) 
    {
        // Call the user defined free func on it
        cache->object_free_func( old_obj->prefix, old_obj->identifier, old_obj->ptr );
    }

    // Store the new cache object possibly removing the old one
    g_hash_table_replace( cache->hashtable, key, obj );
}

/**
 * Retrieve a stored cache object
 *
 * If the object is not available in the cache NULL will be returned. If the
 * time to live of the requested cache object lies within the past NULL will be
 * returned and the old cache is beeing freed.
 */
void* cache_get_object( cache_t* cache, const char* prefix, const char* identifier ) 
{
    char* key = NULL;
    time_t now = time( NULL );
    cache_object_t* obj = NULL;

    asprintf( &key, "%s/%s", prefix, identifier );

    if ( ( obj = g_hash_table_lookup( cache->hashtable, key ) ) == NULL ) 
    {
        free( key );
        return obj;
    }
    
    // Check if we are still in an acceptable ttl lifespan
    if ( obj->timestamp > now + cache->ttl ) 
    {
        // The object does not live any longer kill it
        if ( cache->object_free_func != NULL ) 
        {
            // Call the user defined free func on it
            cache->object_free_func( obj->prefix, obj->identifier, obj->ptr );
        }
        g_hash_table_remove( cache->hashtable, key );
        
        free( key );
        return NULL;
    }
    
    free( key );
    return obj->ptr;
}

/** 
 * Remove an object from cache if it is stored there.
 */
void cache_remove_object( cache_t* cache, const char* prefix, const char* identifier ) 
{
    char* key = NULL;
    cache_object_t* obj = NULL;

    asprintf( &key, "%s/%s", prefix, identifier );

    if ( ( obj = g_hash_table_lookup( cache->hashtable, key ) ) == NULL ) 
    {
        free( key );
        return;
    }

    if ( cache->object_free_func != NULL ) 
    {
        // Call the user defined free func on it
        cache->object_free_func( obj->prefix, obj->identifier, obj->ptr );
    }
    g_hash_table_remove( cache->hashtable, key );
}
