cmake_minimum_required (VERSION 2.8.6)

#set(CMAKE_C_COMPILER clang)
#set(CMAKE_C_COMPILER ccc-analyzer)

add_subdirectory(butcher)

project(libnih)

#set(CMAKE_VERBOSE_MAKEFILE true)

include_directories(
  ${libnih_SOURCE_DIR}/butcher/
  ${libnih_SOURCE_DIR}/src/
)

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

add_definitions(-DTEST)
#add_definitions(-DTESTPROF="cpuprof")

add_definitions(-DBROOT="${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fpic")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--warn-unresolved-symbols")
#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -v")

#RAGEL_TARGET(sx-parser-sq-machine sx-parser-sq-machine.rl
#  ${CMAKE_CURRENT_BINARY_DIR}/sx-parser-sq-machine.c)
#RAGEL_TARGET(sx-parser-dq-machine sx-parser-dq-machine.rl
#  ${CMAKE_CURRENT_BINARY_DIR}/sx-parser-dq-machine.c)
#RAGEL_TARGET(sx-parser-plain-machine sx-parser-plain-machine.rl
#  ${CMAKE_CURRENT_BINARY_DIR}/sx-parser-plain-machine.c)

include_directories(${libnih_SOURCE_DIR})

add_library(nih SHARED
  ${libnih_SOURCE_DIR}/src/gc/gc.c
  ${libnih_SOURCE_DIR}/src/gc/gc-stack.c
  ${libnih_SOURCE_DIR}/src/number/number.c
  ${libnih_SOURCE_DIR}/src/sx/sx.c
  ${libnih_SOURCE_DIR}/src/trie/pathman.c
  ${libnih_SOURCE_DIR}/src/trie/trie.c
  ${libnih_SOURCE_DIR}/src/common/memory.c
  ${libnih_SOURCE_DIR}/src/store/store.c
)

#target_link_libraries(nih m profiler)
target_link_libraries(nih m)

add_custom_target(chop
  $<TARGET_FILE:butcher> $<TARGET_FILE:nih> --bexec $<TARGET_FILE:bexec> $(BFLAGS)
  DEPENDS nih butcher bexec
)
