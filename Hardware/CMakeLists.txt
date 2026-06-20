# Hardware: board-level drivers (BSP + SYSTEM utilities)
# Depends on drivers (HAL) for peripheral access

file(GLOB_RECURSE HW_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/SYSTEM/*.c
)

if(HW_SOURCES)
    add_library(hardware STATIC ${HW_SOURCES})

    # Derive include directories from source file locations so that
    # headers in any subdirectory (e.g. BSP/led/led.h) are directly
    # accessible via bare #include "led.h" from dependents
    foreach(hw_src ${HW_SOURCES})
        get_filename_component(hw_src_dir ${hw_src} DIRECTORY)
        list(APPEND HW_INCLUDE_DIRS ${hw_src_dir})
    endforeach()
    list(REMOVE_DUPLICATES HW_INCLUDE_DIRS)

    target_include_directories(hardware PUBLIC ${HW_INCLUDE_DIRS})

    target_link_libraries(hardware PUBLIC common drivers)
else()
    # No sources yet — create a placeholder INTERFACE target
    # that will be upgraded to STATIC when BSP/SYSTEM sources are added
    add_library(hardware INTERFACE)

    target_link_libraries(hardware INTERFACE common drivers)

    message(STATUS "Hardware: no BSP or SYSTEM sources found; using INTERFACE placeholder")
endif()
