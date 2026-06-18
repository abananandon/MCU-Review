# Drivers: compile CMSIS-Core + CMSIS Device + STM32 HAL

# ── CMSIS Device source (system_stm32f1xx.c) ──
file(GLOB CMSIS_DEVICE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMSIS/Device/STM32F1xx/Source/*.c
)

if(NOT CMSIS_DEVICE_SOURCES)
    message(WARNING
        "CMSIS Device source (system_stm32f1xx.c) not found. "
        "The build will fail at link time without SystemInit()."
    )
endif()

# ── HAL peripheral sources ──
file(GLOB HAL_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/STM32F1xx_HAL_Driver/Src/*.c
)
# Exclude template stubs (they are not meant to be compiled)
list(FILTER HAL_SOURCES EXCLUDE REGEX ".*_template\.c$")

# ── Driver library ──
add_library(drivers STATIC
    ${CMSIS_DEVICE_SOURCES}
    ${HAL_SOURCES}
)

target_include_directories(drivers PUBLIC
    # CMSIS-Core headers (core_cm3.h, etc.)
    ${CMAKE_CURRENT_SOURCE_DIR}/CMSIS/Include
    # CMSIS Device headers (stm32f1xx.h, stm32f103xe.h)
    ${CMAKE_CURRENT_SOURCE_DIR}/CMSIS/Device/STM32F1xx/Include
    # HAL headers
    ${CMAKE_CURRENT_SOURCE_DIR}/STM32F1xx_HAL_Driver/Inc
)

target_link_libraries(drivers PUBLIC config)
