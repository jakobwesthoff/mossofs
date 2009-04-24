include_directories(.)

set(SRC 
	mossofs.c
	salloc.c
	simple_curl.c
	mosso.c
)

set(HEADER
	mosso.h
	salloc.h
	simple_curl.h
)

find_package(PkgConfig)
pkg_check_modules(FUSE REQUIRED fuse)

find_package( CURL REQUIRED )

set(CFLAGS
	${FUSE_CFLAGS} ${FUSE_CFLAGS_OTHER}
	-DFUSE_USE_VERSION=26
)
add_definitions(${CFLAGS})

set(LIBS
	${FUSE_LIBRARIES}
	${CURL_LIBRARIES}
)
link_libraries(${LIBS})

add_executable(mossofs
	${SRC}
	${HEADER}
)
