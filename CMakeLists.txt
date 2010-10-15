cmake_minimum_required (VERSION 2.6)
project(libnih)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/Modules)

find_package(RAGEL)

include_directories(
	/repos/butcher/
	/usr/include/sexpr
)

set(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -pedantic -g -ggdb -DTEST -fPIC")
set(CMAKE_SHARED_LIBRARY_C_FLAGS "-Wl,--no-undefined")
#-Werror -fprofile-arcs -ftest-coverage

RAGEL_TARGET(sx-parser-sq-machine sx-parser-sq-machine.rl
  ${CMAKE_CURRENT_BINARY_DIR}/sx-parser-sq-machine.c)
RAGEL_TARGET(sx-parser-dq-machine sx-parser-dq-machine.rl
  ${CMAKE_CURRENT_BINARY_DIR}/sx-parser-dq-machine.c)
RAGEL_TARGET(sx-parser-plain-machine sx-parser-plain-machine.rl
  ${CMAKE_CURRENT_BINARY_DIR}/sx-parser-plain-machine.c)

add_library(nih SHARED
	blobtree.c
	sx-pool.c
	sx-strgen.c
	sx-parser.c
	sx-stack-tests.c
    ${RAGEL_sx-parser-sq-machine_OUTPUTS}
    ${RAGEL_sx-parser-dq-machine_OUTPUTS}
    ${RAGEL_sx-parser-plain-machine_OUTPUTS}
)

target_link_libraries(nih sexp m)

add_custom_target(chop
	/repos/butcher/butcher -v libnih.so $(BFLAGS)
	DEPENDS nih
)
