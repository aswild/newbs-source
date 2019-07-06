cmake_minimum_required(VERSION 2.6)
set(EXECUTABLE_OUTPUT_PATH bin)

project(newbs-swdl)
set(PACKAGE_VERSION 2.2)

option(WITH_LTO "Enable link-time optimization" OFF)
option(WITH_SWDL "Build newbs-swdl" ON)
option(WITH_MKNIMAGE "Build mknImage" ON)
option(WITH_SWDL_TEST "Build SWDL test mode" OFF)

if(CMAKE_SYSTEM_PROCESSOR MATCHES x86.*)
    set(WITH_SWDL_TEST ON)
    message("-- Building for x86, enabling swdl test mode")
endif()

if(CMAKE_C_FLAGS STREQUAL "")
    set(CMAKE_C_FLAGS "-g -O2")
endif()
if(CMAKE_CXX_FLAGS STREQUAL "")
    set(CMAKE_CXX_FLAGS "-g -O2")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14")

add_compile_options(-Wall -Wextra -Werror)
if(WITH_LTO)
    add_compile_options(-flto)
endif()

include_directories("lib")
add_compile_definitions(_GNU_SOURCE PACKAGE_VERSION="${PACKAGE_VERSION}")
if(WITH_SWDL_TEST)
    add_compile_definitions(SWDL_TEST)
endif()

set(LIBSOURCES
    lib/nImage.h
    lib/common.c
    lib/crc32.c
    lib/log.c
)

set(MKNIMAGE_SOURCES
    ${LIBSOURCES}
    mknImage/mknImage.h
    mknImage/mknImage.c
    mknImage/crc32_cmd.c
    mknImage/create.c
    mknImage/check.c
)

set(SWDL_SOURCES
    ${LIBSOURCES}
    swdl/main.cpp
    swdl/flashbanks.cpp
    swdl/lib.cpp
    swdl/program.cpp
    swdl/PError.h
    swdl/PError.cpp
)

if(WITH_MKNIMAGE)
    add_executable(mknImage ${MKNIMAGE_SOURCES})
    install(TARGETS mknImage DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(WITH_SWDL)
    add_executable(newbs-swdl ${SWDL_SOURCES})
    install(TARGETS newbs-swdl DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND}
                  -E create_symlink newbs-swdl
                  \$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/swdl)
                  message(\"-- Symlinking: \"
                      \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/swdl \"
                      \"to newbs-swdl\")")
endif()
