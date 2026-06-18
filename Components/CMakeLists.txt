# Components: software libraries (no hardware dependency)
# Filter, Math algorithms — pure C, portable

file(GLOB_RECURSE COMPONENT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Filter/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Math/*.c
)

if(COMPONENT_SOURCES)
    add_library(components STATIC ${COMPONENT_SOURCES})

    target_include_directories(components PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Filter
        ${CMAKE_CURRENT_SOURCE_DIR}/Math
    )

    target_link_libraries(components PUBLIC config)
else()
    add_library(components INTERFACE)

    target_link_libraries(components INTERFACE config)

    message(STATUS "Components: no Filter or Math sources found; using INTERFACE placeholder")
endif()
