# arm-none-eabi-gcc.cmake
# CMake toolchain file for ARM Cortex-M3 / STM32F103xE
#
# Usage:
#   cmake -B Build -G Ninja -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi-gcc.cmake -DCMAKE_BUILD_TYPE=Debug
#   cmake --build Build
#
# For VSCode CMake Tools, add to .vscode/settings.json:
#   "cmake.configureArgs": ["-DCMAKE_TOOLCHAIN_FILE=arm-none-eabi-gcc.cmake"]

# ── System ──
set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          cortex-m3)

# Avoid CMake from running linker checks for bare-metal
set(CMAKE_TRY_COMPILE_TARGET_TYPE   STATIC_LIBRARY)

# ── Compilers ──
set(CMAKE_C_COMPILER                arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER              arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER              arm-none-eabi-gcc)

# ── Architecture flags (shared by C / CXX / ASM / Linker) ──
set(CORE_FLAGS "-mcpu=cortex-m3 -mthumb -mfloat-abi=soft" CACHE STRING
    "ARM Cortex-M3 common flags")

# ── Initial compiler flags (project may append via CMAKE_C_FLAGS etc.) ──
# -fdata-sections / -ffunction-sections are required for linker --gc-sections
set(CMAKE_C_FLAGS_INIT       "${CORE_FLAGS} -fdata-sections -ffunction-sections")
set(CMAKE_CXX_FLAGS_INIT     "${CORE_FLAGS} -fdata-sections -ffunction-sections")
# -x assembler-with-cpp: enables C preprocessor in .s files (#include, #define)
set(CMAKE_ASM_FLAGS_INIT     "${CORE_FLAGS} -x assembler-with-cpp")

# ── CMake search — use HOST tools, cross-compile libraries/includes ──
# NEVER: find programs (ninja, cmake, etc.) on host PATH only
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
