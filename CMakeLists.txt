cmake_minimum_required (VERSION 2.6)
project(libnih)

#set(CMAKE_VERBOSE_MAKEFILE true)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../butcher/
	${libnih_SOURCE_DIR}/src/
	/usr/include/sexpr
)

#set(CMAKE_C_COMPILER clang)
#set(CMAKE_C_COMPILER ccc-analyzer)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-gnu")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-inline")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O1")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftest-coverage")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcatch-undefined-behavior")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftrapv")

#add_definitions(-DLUA)
add_definitions(-DTEST)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fpic")
#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--warn-unresolved-symbols")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--sort-section=name")
#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -v")

#RAGEL_TARGET(sx-parser-sq-machine sx-parser-sq-machine.rl
#  ${CMAKE_CURRENT_BINARY_DIR}/sx-parser-sq-machine.c)
#RAGEL_TARGET(sx-parser-dq-machine sx-parser-dq-machine.rl
#  ${CMAKE_CURRENT_BINARY_DIR}/sx-parser-dq-machine.c)
#RAGEL_TARGET(sx-parser-plain-machine sx-parser-plain-machine.rl
#  ${CMAKE_CURRENT_BINARY_DIR}/sx-parser-plain-machine.c)

include_directories(${libnih_SOURCE_DIR})

add_library(nih SHARED
#${libnih_SOURCE_DIR}/src/sr/sr.c
${libnih_SOURCE_DIR}/src/gc/gc.c
${libnih_SOURCE_DIR}/src/gc/gc-stack.c
${libnih_SOURCE_DIR}/src/number/number.c
#${libnih_SOURCE_DIR}/src/blobtree/blobtree.c
${libnih_SOURCE_DIR}/src/sx/sx.c
#${libnih_SOURCE_DIR}/src/lua/table-lua.c
${libnih_SOURCE_DIR}/src/lua/init.c
${libnih_SOURCE_DIR}/src/trie/pathman.c
${libnih_SOURCE_DIR}/src/trie/trie.c
${libnih_SOURCE_DIR}/src/common/memory.c
)

#target_link_libraries(nih sexp m lua)
target_link_libraries(nih m)

add_custom_target(chop
  ${CMAKE_CURRENT_SOURCE_DIR}/../butcher/butcher libnih.so $(BFLAGS)
  DEPENDS nih
)
