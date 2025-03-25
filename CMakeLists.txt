cmake_minimum_required(VERSION 3.6)
project (wfde_project VERSION 1.3 LANGUAGES CXX)

set (WFDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

option(BOOST_USE_STATIC_LIBS "Link boost libraries statically" ON)

if (NOT DEFINED WFDE_WITH_TLS)
    option(WFDE_WITH_TLS "Enable TLS" ON)
endif()

if (NOT DEFINED WITH_APIDOC)
    option(WITH_APIDOC "Generate Doxygen documentation")
endif()

if (NOT DEFINED WFDE_WITH_UNIT_TESTS)
    option(WFDE_WITH_UNIT_TESTS "Enable Unit Testing" ON)
    if (NOT DEFINED WFDE_AUTORUN_UNIT_TESTS)
        option(WFDE_AUTORUN_UNIT_TESTS "Run unit tests during build" OFF)
    endif()
endif()

if (BOOST_USE_STATIC_LIBS)
    message("Using static boost libraries")
    set(Boost_USE_STATIC_LIBS ON CACHE INTERNAL "")
else()
    set(Boost_USE_STATIC_LIBS OFF CACHE INTERNAL "")
endif()

message(STATUS "Using ${CMAKE_CXX_COMPILER}")

include(cmake_scripts/external-projects.cmake)
include(cmake_scripts/add_and_run_test.cmake)

# Asio use depricated headers. Remove the spam warnings...
add_definitions(-DBOOST_ALLOW_DEPRECATED_HEADERS=1)

# Include Boost as an imported target
find_package(Boost 1.65 REQUIRED COMPONENTS
    system
    program_options
    filesystem
    date_time
    context
    coroutine
    chrono
    regex
    )
add_library(boost INTERFACE IMPORTED)
set_property(TARGET boost PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR}
    )

if (WFDE_WITH_TLS)
    find_package(OpenSSL REQUIRED)
endif()

add_subdirectory(src/wfde)
add_subdirectory(src/wfded)

if (WFDE_WITH_UNIT_TESTS)
    add_subdirectory(tests)
endif()

CONFIGURE_FILE(config.h.template ${CMAKE_BINARY_DIR}/generated-include/wfde/config.h)

install(DIRECTORY ${CMAKE_BINARY_DIR}/generated-include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT wfde-targets DESTINATION lib/wfde)
export(TARGETS wfde FILE wfde-exports.cmake)
