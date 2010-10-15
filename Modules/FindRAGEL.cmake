find_program(RAGEL_EXECUTABLE ragel DOC "path to the ragel executable")
mark_as_advanced(RAGEL_EXECUTABLE)

find_library(FL_LIBRARY NAMES fl
  DOC "path to the fl library")
MARK_AS_ADVANCED(FL_LIBRARY)
SET(RAGEL_LIBRARIES ${FL_LIBRARY})

if(RAGEL_EXECUTABLE)

  execute_process(COMMAND ${RAGEL_EXECUTABLE} -v
    OUTPUT_VARIABLE RAGEL_version_output
    ERROR_VARIABLE RAGEL_version_error
    RESULT_VARIABLE RAGEL_version_result
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT ${RAGEL_version_result} EQUAL 0)
    message(SEND_ERROR "Command \"${RAGEL_EXECUTABLE} -v\" failed with output:\n${RAGEL_version_error}")
  else()
    string(REGEX REPLACE "^Ragel State Machine Compiler version (...) .*$" "\\1"
      RAGEL_VERSION "${RAGEL_version_output}")
  endif()

  #============================================================
  # RAGEL_TARGET (public macro)
  #============================================================
  #
  macro(RAGEL_TARGET Name Input Output)
    set(RAGEL_TARGET_usage "RAGEL_TARGET(<Name> <Input> <Output> [COMPILE_FLAGS <string>]")
    if(${ARGC} GREATER 3)
      if(${ARGC} EQUAL 5)
        if("${ARGV3}" STREQUAL "COMPILE_FLAGS")
          set(RAGEL_EXECUTABLE_opts  "${ARGV4}")
          separate_arguments(RAGEL_EXECUTABLE_opts)
        else()
          message(SEND_ERROR ${RAGEL_TARGET_usage})
        endif()
      else()
        message(SEND_ERROR ${RAGEL_TARGET_usage})
      endif()
    endif()

    add_custom_command(OUTPUT ${Output}
      COMMAND ${RAGEL_EXECUTABLE}
      ARGS ${RAGEL_EXECUTABLE_opts} -o${Output} ${Input}
      DEPENDS ${Input}
      COMMENT "[RAGEL][${Name}] Building scanner with ragel ${RAGEL_VERSION}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    set(RAGEL_${Name}_DEFINED TRUE)
    set(RAGEL_${Name}_OUTPUTS ${Output})
    set(RAGEL_${Name}_INPUT ${Input})
    set(RAGEL_${Name}_COMPILE_FLAGS ${RAGEL_EXECUTABLE_opts})
  endmacro(RAGEL_TARGET)
  #============================================================


endif(RAGEL_EXECUTABLE)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(RAGEL DEFAULT_MSG RAGEL_EXECUTABLE)

# FindRAGEL.cmake ends here
