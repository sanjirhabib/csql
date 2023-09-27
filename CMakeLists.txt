cmake_minimum_required(VERSION 3.12)
project(csql VERSION 1.0)

# Add the include directory for headers
include_directories(include)

# Find SQLite3 package
find_package(SQLite3 REQUIRED)

find_program(AWK_EXECUTABLE awk)

if(NOT AWK_EXECUTABLE)
    message(FATAL_ERROR "Error: 'awk' is required but not found on the system.")
endif()

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/include)


# List all your C source files from the "src" directory
set(SOURCES
	src/sql.c
	src/window.c
	src/editor.c
	src/var.c
	src/lite.c
	src/main.c
	src/browse.c
	src/structure.c
	src/tsv.c
	src/cross.c
	src/map.c
	src/code.c
	src/ewin.c
	src/database.c
	src/log.c
	src/file.c
	src/cmdline.c
	src/field.c
)

set(GENHEADER_SCRIPT ${CMAKE_SOURCE_DIR}/bin/cheader)

# Create a custom target to run the cheader shell script on each source file
foreach(SOURCE_FILE ${SOURCES})
    get_filename_component(FILENAME ${SOURCE_FILE} NAME)
	get_filename_component(BASENAME ${SOURCE_FILE} NAME_WE)
	set(HEADER_OUTPUT ${CMAKE_SOURCE_DIR}/include${BASENAME}.h)
    add_custom_command(
        OUTPUT ${HEADER_OUTPUT}
        COMMAND ${GENHEADER_SCRIPT} ${SOURCE_FILE} ${CMAKE_SOURCE_DIR}/include
        DEPENDS ${SOURCE_FILE} ${GENHEADER_SCRIPT}
		COMMENT "Generating heaader"
    )
	list(APPEND PROCESSED_HEADERS ${HEADER_OUTPUT})
endforeach()


# Create an executable target and include the processed source files
add_executable(csql ${SOURCES} ${PROCESSED_HEADERS})

# Link the SQLite3 library
target_link_libraries(csql SQLite::SQLite3)

if($ENV{SUDO_USER})
    # If installed with sudo, use the home directory of the invoking user
    execute_process(
        COMMAND sh -c "eval echo ~$ENV{SUDO_USER}"
        OUTPUT_VARIABLE USER_HOME
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    # If not installed with sudo, use the current user's home directory
    execute_process(
        COMMAND sh -c "echo $HOME"
        OUTPUT_VARIABLE USER_HOME
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

set(CSQL_HIDDEN_DIR "${USER_HOME}/.csql")
file(MAKE_DIRECTORY ${CSQL_HIDDEN_DIR})

install(TARGETS csql DESTINATION /usr/local/bin)
install(FILES etc/csql.ini DESTINATION /etc)
install(FILES etc/types.tsv DESTINATION ${CSQL_HIDDEN_DIR} PERMISSIONS OWNER_READ OWNER_WRITE)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
)

if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

