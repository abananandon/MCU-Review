# Config: header-only INTERFACE library
# Provides global macros and include paths for the whole project

add_library(config INTERFACE)

target_include_directories(config INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(config INTERFACE
    STM32F103xE
    USE_HAL_DRIVER
)
