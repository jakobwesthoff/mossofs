##
# This file is part of Mossofs.
#
# Mossofs is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; version 3 of the
# License.
#
# Mossofs is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Mossofs; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301  USA
#
# Copyright (C) 2009 Jakob Westhoff <jakob@westhoffswelt.de>
##

include_directories(.)

set(SRC 
	mossofs.c
	salloc.c
	simple_curl.c
	mosso.c
	cache.c
)

set(HEADER
	mosso.h
	salloc.h
	simple_curl.h
	cache.h
)

find_package(PkgConfig)
pkg_check_modules(FUSE REQUIRED fuse)
pkg_check_modules(GLIB REQUIRED glib-2.0)

find_package( CURL REQUIRED )

set(CFLAGS
	${FUSE_CFLAGS} ${FUSE_CFLAGS_OTHER}
	${GLIB_CFLAGS} ${GLIB_CFLAGS_OTHER}
	-DFUSE_USE_VERSION=26
)
add_definitions(${CFLAGS})

set(LIBS
	${FUSE_LIBRARIES}
	${GLIB_LIBRARIES}
	${CURL_LIBRARIES}
)
link_libraries(${LIBS})

add_executable(mossofs
	${SRC}
	${HEADER}
)

install(FILES 
		${CMAKE_CURRENT_BINARY_DIR}/mossofs
	DESTINATION
		bin
)
