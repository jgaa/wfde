
project(wfde)
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


include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if (WIN32)
 	ADD_MSVC_PRECOMPILED_HEADER(war_wfde.h pch.cpp ACTUAL_SOURCES)
	add_definitions(-DWFDE_PCH)
	set(SOURCES ${ACTUAL_SOURCES} pch.cpp ${HEADERS} ${RESFILES})
else()
	set(SOURCES ${ACTUAL_SOURCES})
	set(PTHERAD ptherad)
endif()

add_library(wfde ${SOURCES})
set_target_properties(wfde PROPERTIES DEBUG_OUTPUT_NAME wfdeD)
target_link_libraries(wfde ${BOOST_LIBRARIES}
  debug war_logD optimized war_log
  debug war_tasksD optimized war_tasks
  ${PTHREAD}
  )
