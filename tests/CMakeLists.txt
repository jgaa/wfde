
project(wfde_unit_tests)
set(ACTUAL_SOURCES
    test_WfdeFtpSession.cpp
    test_WfdeConfigurationPropertyTree.cpp
    test_wfde.cpp
    test_WfdePermissions.cpp
    test_WfdePath.cpp
    )

set(HEADERS
	${WARLIB_ROOT_DIR}/include/tasks/WarPipeline.h
	)

if (WIN32)
	ADD_MSVC_PRECOMPILED_HEADER(war_tests.h pch.cpp ACTUAL_SOURCES)
	add_definitions(-DWFDE_PCH)
	set(SOURCES ${ACTUAL_SOURCES} pch.cpp ${HEADERS} ${RESFILES})
else()
	set(SOURCES ${ACTUAL_SOURCES})
endif()

add_definitions(-DBOOST_TEST_DYN_LINK)

add_executable(wfde_unit_tests ${SOURCES})
target_link_libraries(wfde_unit_tests
    ${DEFAULT_LIBRARIES}
    wfde
    ${BOOST_UNIT_TEST_LIBRARIES})

add_test(WFDE_UNITTESTS wfde_unit_tests)

add_subdirectory(wfded)

message(STATUS "DEFAULT_LIBRARIES: ${DEFAULT_LIBRARIES}")
