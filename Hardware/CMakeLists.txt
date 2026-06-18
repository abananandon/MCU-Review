# Hardware: board-level drivers (BSP + SYSTEM utilities)
# Depends on drivers (HAL) for peripheral access

file(GLOB_RECURSE HW_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/SYSTEM/*.c
)

if(HW_SOURCES)
    add_library(hardware STATIC ${HW_SOURCES})

    target_include_directories(hardware PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/BSP
        ${CMAKE_CURRENT_SOURCE_DIR}/SYSTEM
    )

    target_link_libraries(hardware PUBLIC drivers)
else()
    # No sources yet — create a placeholder INTERFACE target
    # that will be upgraded to STATIC when BSP/SYSTEM sources are added
    add_library(hardware INTERFACE)

    target_link_libraries(hardware INTERFACE drivers)

    message(STATUS "Hardware: no BSP or SYSTEM sources found; using INTERFACE placeholder")
endif()
