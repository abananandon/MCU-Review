# Common: infrastructure libraries shared across all layers
# Data structures, utilities — pure C, no hardware dependency

file(GLOB_RECURSE COMMON_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures/*.c
)

if(COMMON_SOURCES)
    add_library(common STATIC ${COMMON_SOURCES})

    target_include_directories(common PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures
    )

    target_link_libraries(common PUBLIC config)
else()
    add_library(common INTERFACE)

    target_include_directories(common INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/DataStructures
    )

    target_link_libraries(common INTERFACE config)

    message(STATUS "Common: no DataStructures sources found; using INTERFACE placeholder")
endif()
