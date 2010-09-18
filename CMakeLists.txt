cmake_minimum_required (VERSION 2.6)
project(libnih)

include_directories(
	/repos/butcher/
	/usr/include/sexpr
)

set(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -pedantic -g -ggdb -DTEST -fPIC -fprofile-arcs -ftest-coverage")
set(CMAKE_SHARED_LIBRARY_C_FLAGS "-Wl,--no-undefined")
#-Werror 

add_library(nih SHARED
	blobtree.c
	sx.c
	sx-pool.c
)
target_link_libraries(nih sexp m)

add_custom_target(chop
	/repos/butcher/butcher -v libnih.so $(BFLAGS)
	DEPENDS nih
)
