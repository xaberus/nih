cmake_minimum_required (VERSION 2.6)
project(libnih)

#set(CMAKE_VERBOSE_MAKEFILE true)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/Modules)

find_package(RAGEL)

include_directories(
	/repos/butcher/
	/usr/include/sexpr
)

set(CMAKE_C_COMPILER clang)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -g3")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O1")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftest-coverage")

#add_definitions(-DLUA)
add_definitions(-DTEST)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fpic")
#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--warn-unresolved-symbols")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--sort-section=name")

RAGEL_TARGET(sx-parser-sq-machine sx-parser-sq-machine.rl
  ${CMAKE_CURRENT_BINARY_DIR}/sx-parser-sq-machine.c)
RAGEL_TARGET(sx-parser-dq-machine sx-parser-dq-machine.rl
  ${CMAKE_CURRENT_BINARY_DIR}/sx-parser-dq-machine.c)
RAGEL_TARGET(sx-parser-plain-machine sx-parser-plain-machine.rl
  ${CMAKE_CURRENT_BINARY_DIR}/sx-parser-plain-machine.c)

include_directories(${libnih_SOURCE_DIR})

add_library(nih SHARED
  blobtree.c
  sx-strgen.c
  sx-parser.c
  ${libnih_SOURCE_DIR}/tests/sx-stack-tests.c
  ${RAGEL_sx-parser-sq-machine_OUTPUTS}
  ${RAGEL_sx-parser-dq-machine_OUTPUTS}
  ${RAGEL_sx-parser-plain-machine_OUTPUTS}
  memory.c
  sr.c
  trie.c
  pathman.c
  lua/init.c
)

target_link_libraries(nih sexp m lua)

add_custom_target(chop
  /repos/butcher/butcher libnih.so $(BFLAGS)
  DEPENDS nih
)
