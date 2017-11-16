project(simpleftpd)

set(FTPD_ACTUAL_SOURCES
    wfded.cpp
    )

set(HEADERS
    wfded.h
    )

if (WIN32)
    ADD_MSVC_PRECOMPILED_HEADER(wfded.h pch.cpp FTPD_ACTUAL_SOURCES)
    add_definitions(-DWAR_PCH)
    set(FTPD_SOURCES ${FTPD_ACTUAL_SOURCES} pch.cpp ${HEADERS} ${RESFILES})
else()
    set(FTPD_SOURCES ${FTPD_ACTUAL_SOURCES})
endif()

add_executable(wfded ${FTPD_SOURCES})
set_target_properties(wfded PROPERTIES DEBUG_OUTPUT_NAME wfdedD)
target_link_libraries(wfded ${DEFAULT_LIBRARIES} wfde)

