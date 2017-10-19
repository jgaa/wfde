# Main project definition for wfde
#
# This project is currently being developed with Debian "stretch", using the
# latest clang++ and g++ compilers and libraries.
#

cmake_minimum_required(VERSION 3.0)
project (wfde)

enable_testing()

if (NOT DEFINED WITH_APIDOC)
    option(WITH_APIDOC "Generate Doxygen documentation")
endif()

if (NOT DEFINED WFDE_WITH_TLS)
    option(WFDE_WITH_TLS "Enable TLS" ON)
endif()

if (NOT DEFINED WITH_UNITTESTS)
    option(WITH_UNITTESTS "Enable unittests" ON)
endif()

if (NOT DEFINED WFDE_ROOT_DIR)
    set(WFDE_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# Asio trigger deprecation warnings
add_definitions(-DBOOST_COROUTINES_NO_DEPRECATION_WARNING=1)

if (NOT EMBEDDED_WFDELIB)

    if (NOT DEFINED WARLIB_ROOT_DIR)
        set(WARLIB_ROOT_DIR ${WFDE_ROOT_DIR}/externals/warlib)
    endif()

    if (WFDE_WITH_TLS AND NOT OPENSSL_ROOT_DIR AND WIN32 AND WAR_USE_WIN32_DEFAULTS)
        # Cmake's find_package(OpenSSL) is badly broken, and fails to work
        # with the default openssl build on windows.
        # In order to resolve the problems, make a 64 (or 32) bit build of
        # openssl and copy inc32/openssl to ${OPENSSL_ROOT_DIR}/include, and
        # the content of out32dll to ${OPENSSL_ROOT_DIR}/lib/VC
        # Make sure to also add ${OPENSSL_ROOT_DIR}/lib/VC to your path
        # when you run programs compiled with restc_cpp under windows, or
        # copy the dll's to the directory with your binaries.
        # (Using 3rd party C libraries under Windows with their own build systems
        # is a major pain-point. I'm planning to see if I can use Windows crypto
        # library in sted of openssl, (or somerthing that can be built as a
        # submodule to git with cmake)).
        set(OPENSSL_ROOT_DIR C:/devel/openssl)
        message(STATUS "Setting OPENSSL_ROOT_DIR to ${OPENSSL_ROOT_DIR}")
    endif()

    if (UNIX)
        set(THREADLIBS pthread)
    endif()

    if (WFDE_WITH_TLS)
        find_package(OpenSSL REQUIRED)
    endif()

    set (DEFAULT_LIBRARIES
        ${DEFAULT_LIBRARIES}
        ${THREADLIBS}
        ${OPENSSL_LIBRARIES}
        ${ZLIB_LIBRARIES}
        war_tasksD
        war_logD
        war_baseD
    )

    set(EMBEDDED_WARLIB ON)

    message(STATUS "WARLIB_ROOT_DIR: ${WARLIB_ROOT_DIR}")
    include(${WARLIB_ROOT_DIR}/cmake_scripts/boost.cmake)
    include(${WARLIB_ROOT_DIR}/cmake_scripts/pch.cmake)

    # We create a configuration file so that other code that
    # include our header files gets the correct configuration.
    set(CONF_PATH ${PROJECT_SOURCE_DIR}/include/wfde/wfde_config.h)

    message(STATUS "Using ${CMAKE_CXX_COMPILER}")

    if (UNIX)
    # For now, assume we use g++/clang++
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG=1 -o3 ")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG=1 -D_DEBUG=1 -o0 -g ")
        add_definitions(-D__USE_LARGEFILE64=1 -D__USE_FILE_OFFSET64=1 -Wall -fPIC -std=c++1y -pthread)
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG=1 ")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG=1 -D_DEBUG=1 ")
        # We will support windows from Windows Vista
        add_definitions(-D_WIN32_WINNT=0x0600)
    endif()

    if (OPENSSL_ROOT)
        include_directories(${OPENSSL_ROOT}/include)
        link_directories(${OPENSSL_ROOT}/lib/VC)
    endif()

    if (BOOST_ROOT)
        if (NOT BOOST_INCLUDE_DIR)
            set(BOOST_INCLUDE_DIR ${BOOST_ROOT})
        endif()

        if (NOT BOOST_LIBRARY_DIRS)
            set(BOOST_LIBRARY_DIRS ${BOOST_ROOT}/stage/lib CACHE PATH "path to prebuilt library")
        endif()
    else()
        message(STATUS "No BOOST_ROOT set. Assuming that boost is installed and available for the compiler")
    endif()

    if (BOOST_INCLUDE_DIR)
        message(STATUS "Setting boost include-dir to: ${BOOST_INCLUDE_DIR}")
        include_directories(${BOOST_INCLUDE_DIR})
    endif()

    if (BOOST_LIBRARY_DIRS)
        message(STATUS "Setting boost libdir to: ${BOOST_LIBRARY_DIRS}")
        link_directories(${BOOST_LIBRARY_DIRS} )
    endif()


    set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib CACHE PATH "Destination location")
    link_directories(${LIBRARY_OUTPUT_PATH})
endif() # EMBEDDED_WFDELIB

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${WARLIB_ROOT_DIR}/include
    )

add_subdirectory(${WARLIB_ROOT_DIR})
add_subdirectory(src)

if (WITH_UNITTESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (WITH_APIDOC)
    find_package(Doxygen)
    if (DOXYGEN_FOUND)

        set(DOXYGEN_INPUT Doxygen)
        set(DOXYGEN_OUTPUT doc/html)

        add_custom_command(
            OUTPUT ${DOXYGEN_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E echo_append "Building API Documentation..."
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_INPUT}
            COMMAND ${CMAKE_COMMAND} -E echo "Done."
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            DEPENDS ${DOXYGEN_INPUT}
        )

        add_custom_target(wfde_apidoc ALL DEPENDS ${DOXYGEN_OUTPUT})

    endif (DOXYGEN_FOUND)
endif()

message(STATUS "Writing the current configuration to ${CONF_PATH}")
CONFIGURE_FILE(wfde_config.h.template ${CONF_PATH})

message(STATUS "Binary files will be located here: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

