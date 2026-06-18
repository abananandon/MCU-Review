# MCU-REVIEW — STM32F103xE firmware project
#
# Requires toolchain file:
#   cmake -B Build -G Ninja -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi-gcc.cmake -DCMAKE_BUILD_TYPE=Debug
#   cmake --build Build

cmake_minimum_required(VERSION 3.15)

project(MCU-REVIEW LANGUAGES C ASM)

set(CMAKE_C_STANDARD            99)
set(CMAKE_C_STANDARD_REQUIRED   ON)

# ── Compiler warnings (C only — not passed to assembler) ──
add_compile_options(
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Wextra>
    $<$<COMPILE_LANGUAGE:C>:-Wshadow>
    $<$<COMPILE_LANGUAGE:C>:-Wundef>
    $<$<COMPILE_LANGUAGE:C>:-Wmaybe-uninitialized>
    $<$<COMPILE_LANGUAGE:C>:-Wno-unused-parameter>
)

# ── Optimization by build type ──
set(CMAKE_C_FLAGS_DEBUG   "-Og -g3")
set(CMAKE_C_FLAGS_RELEASE "-O2 -g0")

# ── Linker script ──
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/Config/STM32F103xE_FLASH.ld)

# ── Link options ──
# Arch flags already in CMAKE_C_FLAGS (from toolchain _INIT), CMake passes them to linker.
# Do NOT add CORE_FLAGS here — they'd duplicate and be mis-quoted as one argument.
add_link_options(
    -T${LINKER_SCRIPT}
    --specs=nosys.specs
    --specs=nano.specs
    -Wl,--cref
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,-Map=${CMAKE_SOURCE_DIR}/Outputs/${PROJECT_NAME}.map
)

# ── Output directories ──
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Outputs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Outputs)

# ── Subdirectories ──
add_subdirectory(Config)
add_subdirectory(Drivers)
add_subdirectory(Startup)
add_subdirectory(Hardware)
add_subdirectory(Components)
add_subdirectory(Middlewares)
add_subdirectory(App)
