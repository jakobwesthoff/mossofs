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

void* smalloc( size_t size )
{
    void* ptr = NULL;
    if ( ( ptr = malloc( size ) ) == NULL )
    {
        printf( "Allocation of %i bytes failed\n", size );
        exit( 255 );
    }

    memset( ptr, 0, size );

    return ptr;
}

void* scalloc( size_t nelem, size_t elsize )
{
    void* ptr = NULL;
    if ( ( ptr = calloc( nelem, elsize ) ) == NULL )
    {
        printf( "Allocation of %i bytes failed\n", nelem*elsize );
        exit( 255 );
    }

    return ptr;
}

void* srealloc( void* ptr, size_t size )
{
    void* new_ptr = NULL;
    if ( ( new_ptr = realloc( ptr, size ) ) == NULL )
    {
        printf( "Reallocation of %i bytes failed\n", size );
        exit( 255 );
    }

    return new_ptr;
}
