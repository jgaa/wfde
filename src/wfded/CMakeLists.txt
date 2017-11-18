project(wfded VERSION 0.21.0 LANGUAGES CXX)

set(FTPD_ACTUAL_SOURCES
    wfded.cpp
    )

set(HEADERS
    wfded.h
    )

if (WIN32)
    set(FTPD_SOURCES ${FTPD_ACTUAL_SOURCES} ${HEADERS} ${RESFILES})
else()
    set(FTPD_SOURCES ${FTPD_ACTUAL_SOURCES})
endif()

add_executable(wfded ${FTPD_SOURCES})
set_property(TARGET wfded PROPERTY CXX_STANDARD 14)
target_link_libraries(wfded PRIVATE ${Boost_LIBRARIES} wfde)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DBOOST_COROUTINES_NO_DEPRECATION_WARNING=1)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DBOOST_COROUTINE_NO_DEPRECATION_WARNING=1)

target_include_directories(wfded
    PRIVATE ${WFDE_ROOT}/include>
    PRIVATE ${CMAKE_BINARY_DIR}/generated-include/
    PRIVATE ${WFDE_ROOT}/src/wfded
)

install(TARGETS wfded EXPORT wfde-targets
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
