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

#include <fuse.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>

#include "salloc.h"
#include "mosso.h"
#include "cache.h"

/**
 * Option structure used to store and transport the initially read fuse options
 */
typedef struct 
{
    char* username;
    char* apikey;
} mossofs_options_t;

/**
 * Filehandle structure used to store informations between different read and
 * write calls.
 *
 * The structure is created upon an open call and destroyed open a close call.
 * It is then stored in the fuse fh structure member.
 *
 * If the file does not exist upon a call to open the meta member will be set
 * to NULL. Furthermore the is_new flag is set to true.
 */
typedef struct
{
    int is_new;
    mosso_object_meta_t* meta; 
} mossofs_filehandle_t;

/**
 * Global pointer to a mosso option structure
 */
static mossofs_options_t* mossofs_options = NULL;

#define MOSSOFS_OPT( x, y, z ) {x, offsetof( mossofs_options_t, y ), z }

/**
 * Retrieve the stored mosso_connection_t object from the current fuse_context
 */
#define MOSSO_CONNECTION(m) \
    mosso_connection_t* m = NULL; \
    { struct fuse_context* context = fuse_get_context(); m = (mosso_connection_t*)context->private_data;};

/**
 * Retrieve the filehandle stored in a fuse_file_info structure
 */
static inline mossofs_filehandle_t* get_mossofs_filehandle( struct fuse_file_info *fi )
{
    return ( mossofs_filehandle_t* ) ( uintptr_t ) fi->fh;
}

#define INIT_DEBUGLOG debuglog = fopen( "debug.log", "a" )
#define DEBUGLOG(s, ...) fprintf( debuglog, s, ##__VA_ARGS__ ); fflush( debuglog )
FILE* debuglog = NULL;

/**
 * Called whenever a structure stored in the cache needs to be freed
 *
 * This function decides based on the provided prefix which free function needs
 * to be used, to clear the cached item successfully from memory.
 */
static void mossofs_cache_object_free( char* prefix, char* identifier, void* ptr ) 
{
    if ( strcmp( prefix, "meta" ) == 0 ) 
    {
        mosso_object_meta_free( (mosso_object_meta_t*)ptr );
    }
    else if( strcmp( prefix, "objects" ) == 0 ) 
    {
        mosso_object_free_all( (mosso_object_t*)ptr );
    }
}

/**
 * Initialize the mosso filesystem
 *
 * This function establishes a connection to the mosso cloud service and
 * retrieves the needed auth token. 
 *
 * The mosso connection struct will be returned to allow fuse to pass it to any
 * other called function.
 */
static void* mossofs_init( struct fuse_conn_info *conn ) 
{
    mosso_connection_t* mosso = NULL;

    // Initialize the cURL library enabling SSL support
    curl_global_init( CURL_GLOBAL_SSL );

    if ( ( mosso = mosso_init( mossofs_options->username, mossofs_options->apikey ) ) == NULL )
    {
        printf( "The connection to Mosso Cloudspace could not be established: %s\n", mosso_error_string() );
        free( mossofs_options->username );
        free( mossofs_options->apikey );
        free( mossofs_options );
        curl_global_cleanup();
        exit( 2 );
    }

    // Initialize new cache with 5 minutes timeout
    mosso->cache = cache_new( 300, mossofs_cache_object_free );

    // Return the connection to embed it into every fuse context.
    return mosso;
}

/**
 * Called upon filesystem destruction
 *
 * The supplied private data is the mosso connection structure created during
 * init.
 */
static void mossofs_destroy( void* mosso ) 
{
    // This one frees the allocated cache structure as well
    mosso_cleanup( ( mosso_connection_t* )mosso );    
    curl_global_cleanup();

    // Free the options struct
    free( mossofs_options->username );
    free( mossofs_options->apikey );
    free( mossofs_options );
}

/**
 * Retrieve attributes of the given path
 */
static int mossofs_getattr( const char *path, struct stat *stbuf ) 
{
    MOSSO_CONNECTION( mosso );
    mosso_object_meta_t* meta = NULL;    

    int id = rand();

    DEBUGLOG( "getattr(%d): %s\n",id, path );

    // Null the stats buffer
    memset( stbuf, 0, sizeof( struct stat ) );

    // Directory stats for the mountpoint
    if ( strcmp( path, "/" ) == 0 ) 
    {
        stbuf->st_mode  = S_IFDIR | 0755;
        stbuf->st_nlink = 2; /* Link into the dir and link inside the dir (.) */
        return 0;
    }
    
    // Try to retrieve the needed information from the cache
    if ( ( meta = (mosso_object_meta_t*)cache_get_object( mosso->cache, "meta", path ) ) == NULL ) 
    {
        DEBUGLOG( "Not cached\n" );
        // Try to retrieve meta information for the given filepath
        if ( ( meta = mosso_get_object_meta( mosso, (char*)path ) ) == NULL ) 
        {
            // The requested object is not existant
            return -ENOENT;
        }
        
        cache_add_object( mosso->cache, "meta", path, meta );
    }


    // Set the correct file/dir type
    if ( meta->type == MOSSO_OBJECT_TYPE_OBJECT ) 
    {
        stbuf->st_mode  = S_IFREG | 0444;
        stbuf->st_nlink = 2; /* Link into the dir and link inside the dir (.) */
        stbuf->st_size  = meta->size;
    }
    else 
    {
        stbuf->st_mode  = S_IFDIR | 0755;
        stbuf->st_nlink = 1;

        // If it is a container a size is available
        if ( meta->type == MOSSO_OBJECT_TYPE_CONTAINER ) 
        {
            stbuf->st_size = meta->size;
        }
    }   

    /* Everything okey */
    return 0;
}

/** 
 * Called whenever a directory contents needs to be listed
 */
static int mossofs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) 
{
    MOSSO_CONNECTION( mosso );
    mosso_object_t* objects = NULL;

    DEBUGLOG( "readdir: %s\n", path );

    if ( ( objects = cache_get_object( mosso->cache, "objects", (char*)path ) ) == NULL ) 
    {
        DEBUGLOG( "not cached\n" );
        if ( ( objects = mosso_list_objects( mosso, (char*)path, NULL ) ) == NULL ) 
        {
            DEBUGLOG( "  path does not exist\n" );
            return -ENOENT;
        }

        cache_add_object( mosso->cache, "objects", (char*)path, objects );
    }

    DEBUGLOG( "  filling: %s\n", "." );
    filler( buf, ".", NULL, 0 );
    DEBUGLOG( "  filling: %s\n", ".." );
    filler( buf, "..", NULL, 0 );

    {
        mosso_object_t* cur = objects->root;
        while( cur != NULL ) 
        {
            DEBUGLOG( "  filling: %s\n", cur->name );
            filler( buf, cur->name, NULL, 0 );
            cur = cur->next;
        }
    }

    /* Everything okey */
    return 0;
}

/**
 * Called every time a file is being opened
 */
static int mossofs_open( const char *path, struct fuse_file_info *fi ) 
{
    MOSSO_CONNECTION( mosso );
    mosso_object_meta_t* meta = NULL;    

    DEBUGLOG( "open: %s\n", path );

    if ( ( fi->flags & 3 ) != O_RDONLY ) 
    {
        return -EACCES;
    }

    // Try to retrieve meta information for the given filepath
    if ( ( meta = mosso_get_object_meta( mosso, (char*)path ) ) == NULL ) 
    {
        // The requested object is not existant
        return -ENOENT;
    }

    // Allocate a new filehandle structure and store the retrieved metadata
    // information.
    // @TODO: At the moment only existing files can be read. New files can not
    // be created. This should be changed.
    {
        mossofs_filehandle_t* filehandle = snew( mossofs_filehandle_t );
        filehandle->meta = meta;
        fi->fh = (unsigned long)(filehandle);
    }
    
    return 0;
}

/**
 * Called every time data needs to be read from a file
 */
static int mossofs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) 
{
    MOSSO_CONNECTION( mosso );
    mossofs_filehandle_t* filehandle = get_mossofs_filehandle( fi );

    size_t read_bytes = 0;
    uint64_t bytes_to_read = ( filehandle->meta->size < offset + size )
                           ? ( filehandle->meta->size - offset )
                           : ( size );

    DEBUGLOG( "toread: %lld\n", bytes_to_read );

    DEBUGLOG( "read( %s, %ld, %ld )\n", path, (long)size, (long)offset );

    if ( ( read_bytes = mosso_read_object( mosso, (char*)path, bytes_to_read, buf, offset ) ) == -1 ) 
    {
        return -ENOENT;
    }

    DEBUGLOG( "read_bytes: %ld\n", (long)read_bytes );

    return read_bytes;
}

/**
 * Called every time a opened file is released. This function will only called
 * once for each open call.
 */
static int mossofs_release( const char* path, struct fuse_file_info* fi ) 
{
    mossofs_filehandle_t* filehandle = get_mossofs_filehandle( fi );
    ( filehandle->meta != NULL ) ? ( mosso_object_meta_free( filehandle->meta ) ) : NULL;
    free( filehandle );
}

/**
 * Show the usage message of this application
 */
static void show_usage( char* executable )
{
    printf( "Mossofs FUSE module version 0.1\n" );
    printf( "Jakob Westhoff <jakob@westhoffswelt.de>\n\n" );
    printf( "Usage:\n" );
    printf( "%s mosso_username@mosso_apikey <MOUNTPOINT>\n\n", executable );
}

/**
 * Function called by the fuse_parse_opts function every time a option is read
 *
 * This function is used to read the usename and apikey string and split it at
 * the @ char into the two different strings needed for later processing.
 * Furthermore it stores this values in the mossofs_options structure.
 */
int mossofs_parse_opts( void* data, const char* arg, int key, struct fuse_args *outargs ) 
{
    switch( key ) 
    {
        case FUSE_OPT_KEY_OPT:
            /* Some option defined in the mossofs_opts array used to call the
             * appropriate fuse function */
            /* Just keep the option and let fuse handle it */
            return 1;
        case FUSE_OPT_KEY_NONOPT:
            /* This is the api key username string which should only occur once */
            if ( mossofs_options->username == NULL && mossofs_options->apikey == NULL ) 
            {
                const char* end = arg;

                // Find the splitting @ character
                while( *end != 0 && *end != '@' ) { ++end; }

                // If we are at the end of the string something bad happend
                if ( *end == 0 ) 
                {
                    fprintf( stderr, "'%s' is not a valid username@apikey string\n", arg );
                    return 0;
                }

                mossofs_options->username = (char*)smalloc( end-arg + 1 );
                mossofs_options->apikey   = (char*)smalloc( strlen( arg ) - (size_t)(end-arg) );
                memcpy( mossofs_options->username, arg, end-arg );
                memcpy( mossofs_options->apikey, ++end, strlen( arg ) - (size_t)(end-arg) );

                return 0;
            }

            // Otherwise keep the option. It might be the mountpoint
            return 1;
            
        default:
            fprintf( stderr, "Unknown option specified: %s\n", arg );
            return 0;
    }
}

/**
 * Main function called upon program execution
 *
 * All the needed initialization is done here. Furthermore the curl library is
 * initialized the given commandline parameters are parsed and a new mosso
 * session is requested.
 */
int main( int argc, char **argv )
{
    INIT_DEBUGLOG;

    struct fuse_opt mossofs_opts[] = {
        FUSE_OPT_END
    };

    struct fuse_args args = FUSE_ARGS_INIT( argc, argv );

    mossofs_options = snew( mossofs_options_t );

    if( fuse_opt_parse( &args, mossofs_options, mossofs_opts, mossofs_parse_opts ) == -1 ) 
    {
        fprintf( stderr, "Error parsing commandline options\n" );
        exit( 1 );
    }
    
    if( mossofs_options->username == NULL || mossofs_options->apikey == NULL ) 
    {
        free( mossofs_options );
        show_usage( argv[0] );
        exit( 1 );
    }

    // Initialize and call the fuse handler
    {
        struct fuse_operations mossofs_operations = 
        {
            .init    = mossofs_init,
            .destroy = mossofs_destroy,
            .getattr = mossofs_getattr,
            .readdir = mossofs_readdir,
            .open    = mossofs_open,
            .read    = mossofs_read,
            .release = mossofs_release            
        };

        return fuse_main( args.argc, args.argv, &mossofs_operations, NULL );
    }

}
