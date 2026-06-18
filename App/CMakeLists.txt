# App: application source files --> final executable target

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
)

# Executable target (ELF)
add_executable(${PROJECT_NAME} ${APP_SOURCES})

# Force .elf suffix on Generic system (CMake won't add one automatically)
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link all libraries; transitive dependencies are resolved automatically:
#   hardware  --> drivers --> config
#   components --> config
#   startup   --> config
target_link_libraries(${PROJECT_NAME}
    startup
    hardware
    components
)

# ── Generate .hex and .bin for flashing ──
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
        $<TARGET_FILE:${PROJECT_NAME}>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
        $<TARGET_FILE:${PROJECT_NAME}>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.bin
    COMMENT "Generating .hex and .bin from $<TARGET_FILE:${PROJECT_NAME}>"
)
