project(wfde_unit_tests LANGUAGES CXX)

enable_testing()

MACRO(WFDE_ADD_TEST Name Source)
    add_executable(${Name} ${Source})
    add_dependencies(${Name} externalLest)
    target_link_libraries(${Name} wfde)
    target_include_directories(${Name}
        PRIVATE ${WFDE_ROOT}/include>
        PRIVATE ${CMAKE_BINARY_DIR}/generated-include/
        PRIVATE ${WFDE_ROOT}/src/wfded
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_and_run_test(${Name} ${CMAKE_CURRENT_BINARY_DIR})
ENDMACRO(WFDE_ADD_TEST)

wfde_add_test(wfde_tests test_wfde.cpp)
wfde_add_test(wfde_session test_WfdeFtpSession.cpp)
wfde_add_test(wfde_ptree test_WfdeConfigurationPropertyTree.cpp)
wfde_add_test(wfde_permissions test_WfdePermissions.cpp)
wfde_add_test(wfde_path test_WfdePath.cpp)
