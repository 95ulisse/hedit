cmake_minimum_required (VERSION 3.1)
project (HEdit)

# Paths
get_filename_component (SRC "./src" REALPATH BASE_DIR "${PROJECT_SOURCE_DIR}")
get_filename_component (GEN "./gen" REALPATH BASE_DIR "${PROJECT_BINARY_DIR}")
get_filename_component (DEPS "./deps" REALPATH BASE_DIR "${PROJECT_SOURCE_DIR}")
get_filename_component (SCRIPTS "./scripts" REALPATH BASE_DIR "${PROJECT_SOURCE_DIR}")
get_filename_component (DOCS "./docs" REALPATH BASE_DIR "${PROJECT_SOURCE_DIR}")

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif()

# Compiler options
set (CMAKE_C_FLAGS "-std=c99 -Wall -Werror -pedantic -D_POSIX_C_SOURCE=200809L")
set (CMAKE_C_FLAGS_DEBUG "-g -DDEBUG")
set (CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set (CMAKE_CXX_FLAGS "-std=c++11 -Wall -Werror -pedantic")
set (CMAKE_CXX_FLAGS_DEBUG "-g -DDEBUG")
set (CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# Version number
set (HEDIT_VERSION "0.1.0")

# Build options
option (WITH_V8 "Enable JavaScript integration with V8." ON)



# --------------------------------------------------------------------
# External libs
# --------------------------------------------------------------------

# Libtickit
add_custom_target (
   libtickit
   COMMAND "${SCRIPTS}/build-libtickit.sh" "${DEPS}"
)
include_directories ("${DEPS}/libtickit/include")

# V8
if (WITH_V8)

    # V8 version and arch
    execute_process (COMMAND /bin/sh "-c" "gcc -dumpmachine | cut -d - -f 1"
                     OUTPUT_VARIABLE ARCH
                     OUTPUT_STRIP_TRAILING_WHITESPACE)
    set (V8_VERSION 6.4)
    if ("${ARCH}" STREQUAL "x86_64")
        set (V8_ARCH "x64")
    elseif ("${ARCH}" STREQUAL "i386")
        set (V8_ARCH "ia32")
    else ()
        set (V8_ARCH "${ARCH}")
    endif ()
    
    add_custom_target (
        v8
        COMMAND "${SCRIPTS}/build-v8.sh" "${DEPS}" "${V8_VERSION}" "${V8_ARCH}"
    )
    include_directories ("${DEPS}/v8/include")

endif ()



# --------------------------------------------------------------------
# HEdit
# --------------------------------------------------------------------

# Generated header for JS modules
file (GLOB_RECURSE JS_MODULES "${SRC}/js/*.js")
add_custom_command (
    OUTPUT "${GEN}/js-builtin-modules.cc"
    COMMAND "${SCRIPTS}/gen-js.sh" "${SRC}/js" "${GEN}/js-builtin-modules.cc"
    DEPENDS ${JS_MODULES}
)

# Header file to pass the build config
configure_file (
    "${SRC}/build-config.h.in"
    "gen/build-config.h"
)

# Include directories
include_directories ("${SRC}")
include_directories ("${GEN}")

# Source files
file (GLOB_RECURSE SOURCES "${SRC}/*.c" "${SRC}/*.cc")

# Final executable
add_executable (hedit ${SOURCES})
target_link_libraries (hedit "m" "pthread" "termkey" "unibilium")
target_link_libraries (hedit "${DEPS}/libtickit/.libs/libtickit.a")
add_dependencies (hedit libtickit)
if (WITH_V8)
    target_link_libraries (hedit -Wl,--start-group "${DEPS}/v8/out/${V8_ARCH}.release/obj.target/src/libv8_libplatform.a"
                                                   "${DEPS}/v8/out/${V8_ARCH}.release/obj.target/src/libv8_base.a"
                                                   "${DEPS}/v8/out/${V8_ARCH}.release/obj.target/src/libv8_libbase.a"
                                                   "${DEPS}/v8/out/${V8_ARCH}.release/obj.target/src/libv8_nosnapshot.a"
                                                   "${DEPS}/v8/out/${V8_ARCH}.release/obj.target/src/libv8_libsampler.a"
                                                   "${DEPS}/v8/out/${V8_ARCH}.release/obj.target/src/libv8_initializers.a"
                                                   "${DEPS}/v8/out/${V8_ARCH}.release/obj.target/src/libv8_init.a"
                                                   "${DEPS}/v8/out/${V8_ARCH}.release/obj.target/third_party/icu/libicui18n.a"
                                                   "${DEPS}/v8/out/${V8_ARCH}.release/obj.target/third_party/icu/libicuuc.a"
                                 -Wl,--end-group)
    target_sources (hedit PUBLIC "${GEN}/js-builtin-modules.cc")
    set_source_files_properties ("${SRC}/format-empty.c" PROPERTIES HEADER_FILE_ONLY TRUE)
    add_dependencies (hedit v8)
else ()
    set_source_files_properties ("${SRC}/js.cc" PROPERTIES HEADER_FILE_ONLY TRUE)
endif ()

# Install
install (TARGETS hedit
         RUNTIME DESTINATION bin)

# JS Docs
add_custom_target (
    docs
    COMMAND cd "${DOCS}" && npm i && ./node_modules/.bin/jsdoc -c jsdoc.json
)
add_custom_target (
    docs-clean
    COMMAND rm -rf "${DOCS}/out" "${DOCS}/node_modules"
)
add_custom_target (
    docs-publish
	COMMAND git checkout gh-pages
	COMMAND cp -R "${DOCS}/out/*" .
	COMMAND git add .
	COMMAND (git commit -m "Documentation update." && git push origin gh-pages) || true
	COMMAND git checkout master
)
