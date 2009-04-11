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

#include "mosso.h"

/**
 * Show the usage message of this application
 */
static void show_usage( char* executable ) 
{
    printf( "Mossofs FUSE module version 0.1\n" );
    printf( "Jakob Westhoff <jakob@westhoffswelt.de>\n\n" );
    printf( "Usage:\n" );
    printf( "%s --username=<MOSSO_USERNAME> --key=MOSSO_APIKEY <MOUNTPOINT>\n\n", executable );
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
    char* username = NULL;
    char* key = NULL;

    mosso_connection_t* mosso = NULL;

    // Define all possible commandline options
    struct option long_options[] = 
    {
        { "username", required_argument, NULL, 'u' },
        { "key",      required_argument, NULL, 'k' },
        { 0, 0, 0, 0 }
    };

    {
        int index = 0;
        int option = 0;
        while( ( option = getopt_long( argc, argv, "u:k:", long_options, &index ) ) != -1 ) 
        {
            switch ( option ) 
            {
                case 'u':
                    username = strdup( optarg );
                break;
                case 'k':
                    key = strdup( optarg );
                break;
            }
        }

        if ( username == NULL || key == NULL || argc != optind + 1 ) 
        {
            show_usage( argv[0] );
            exit( 1 );
        }
    }

    // Initialize the cURL library enabling SSL support
    curl_global_init( CURL_GLOBAL_SSL );
    
    if ( ( mosso = mosso_init( username, key ) ) == NULL ) 
    {
        printf( "The connection to Mosso Cloudspace could not be established: %s\n", mosso_error() );
        curl_global_cleanup();
        exit( 2 );
    }

    {
        mosso_object_t* object = NULL;
        mosso_object_t* cur    = NULL;
        
        if ( ( object = mosso_list_containers( mosso ) ) == NULL ) 
        {
            printf( "Container listing failed: %s\n", mosso_error() );
            curl_global_cleanup();
            exit( 2 );
        }
        cur = object->root;

        while( cur != NULL ) 
        {
            printf( "Name: %s, RequestPath: %s\n", cur->name, cur->request_path );
            cur = cur->next;
        }
        mosso_object_free_all( object );
    }

    mosso_cleanup( mosso );


    // argv + optind
    // argv - optind

    return 0;
}
