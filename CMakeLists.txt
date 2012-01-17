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

set(CFLAGS_LIST
  "-std=gnu99"
  "-Wall"
  "-Wextra"
  "-pedantic"
  "-ggdb"
  "-O0"
  "-fno-inline"
  "-fPIC"
  #"-O1"
  #"-O3"
  #"-Werror"
  #"-fprofile-arcs"
  #"-ftest-coverage"
  #"-fcatch-undefined-behavior"
  #"-ftrapv"
)

add_definitions(-DTEST)
add_definitions(-D_XOPEN_SOURCE=700)
add_definitions(-D_GNU_SOURCE)
#add_definitions(-DTESTPROF="cpuprof")
add_definitions(-DBROOT="${CMAKE_CURRENT_SOURCE_DIR}")

set(LFLAGS_LIST
  "-Wl,--no-undefined"
  "-Wl,--warn-unresolved-symbols"
  "-pthread"
)

include_directories(${libnih_SOURCE_DIR})

string (REPLACE ";" " " CFLAGS "${CFLAGS_LIST}")
string (REPLACE ";" " " LFLAGS "${LFLAGS_LIST}")
add_library(nih SHARED
  ${libnih_SOURCE_DIR}/src/common/err.c
  ${libnih_SOURCE_DIR}/src/gc/gc.c
  ${libnih_SOURCE_DIR}/src/gc/gc-stack.c
  ${libnih_SOURCE_DIR}/src/number/number.c
  ${libnih_SOURCE_DIR}/src/sx/sx.c
  ${libnih_SOURCE_DIR}/src/trie/pathman.c
  ${libnih_SOURCE_DIR}/src/trie/trie.c
  ${libnih_SOURCE_DIR}/src/store/store.c
)
set_target_properties(nih PROPERTIES COMPILE_FLAGS ${CFLAGS})
set_target_properties(bexec PROPERTIES LINK_FLAGS ${LFLAGS})
#target_link_libraries(nih m profiler)
#target_link_libraries(nih m)

add_custom_target(chop
  $<TARGET_FILE:butcher> $<TARGET_FILE:nih> --bexec $<TARGET_FILE:bexec> $(BFLAGS)
  DEPENDS nih butcher bexec
)
