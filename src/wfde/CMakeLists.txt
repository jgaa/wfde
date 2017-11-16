project(wfde VERSION 3.0 LANGUAGES CXX)

set(ACTUAL_SOURCES
    WfdeConfigurationPropertyTree.cpp
    WfdePermissions.cpp
    WfdePath.cpp
    WfdeAsciiFile.cpp
    WfdeFile.cpp
    WfdeSessionManager.cpp
    WfdeClient.cpp
    WfdeSession.cpp
    ftp/WfdeFtpList.cpp
    ftp/WfdeProtocolFtp.cpp
    ftp/WfdeReplyCodes.cpp
    ftp/WfdeFtpSession.cpp
    ftp/wfde_ftp_commands.cpp
    WfdeInterface.cpp
    WfdeProtocol.cpp
    WfdeHost.cpp
    WfdeServer.cpp
    wfde.cpp
    )

set(HEADERS
    war_wfde.h
    WfdeConfigurationPropertyTree.h
    WfdePermissions.h
    WfdePath.h
    WfdeHost.h
    WfdeProtocol.h
    ftp/WfdeFtpList.h
    ftp/WfdeProtocolFtp.h
    ftp/WfdeFtpSession.h
    WfdeServer.h
    WfdeInterface.h
    WfdeClient.h
    WfdeSession.h
    WfdeFile.h
    WfdeAsciiFile.h
    ${WFDE_ROOT_DIR}/include/wfde/wfde.h
    ${WFDE_ROOT_DIR}/include/wfde/win/FileListIterator.h
    ${WARLIB_ROOT_DIR}/include/tasks/WarThreadpool.h
    ${WARLIB_ROOT_DIR}/include/war_asio.h
    )

if (WIN32)
    set(SOURCES ${ACTUAL_SOURCES} ${HEADERS} ${RESFILES})
else()
    set(SOURCES ${ACTUAL_SOURCES})
endif()

add_library(wfde ${SOURCES})
add_dependencies(wfde externalWarlib)
set_property(TARGET wfde PROPERTY CXX_STANDARD 14)
target_include_directories(wfde
    PUBLIC $<BUILD_INTERFACE:${WFDE_ROOT}/include> $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_BINARY_DIR}/generated-include/
    PRIVATE ${WFDE_ROOT}/src/wfde
)
target_link_libraries(wfde
    PUBLIC ${Boost_LIBRARIES} warcore
    PRIVATE ${OPENSSL_LIBRARIES})
target_compile_definitions(${PROJECT_NAME} PUBLIC -DBOOST_COROUTINES_NO_DEPRECATION_WARNING=1)
target_compile_definitions(${PROJECT_NAME} PUBLIC -DBOOST_COROUTINE_NO_DEPRECATION_WARNING=1)

install(TARGETS wfde EXPORT wfde-targets
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

install(DIRECTORY ${WFDE_ROOT}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
