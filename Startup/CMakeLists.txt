# Startup: GCC assembly startup file (vector table, Reset_Handler)

file(GLOB STARTUP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/gcc/*.s
)

add_library(startup STATIC ${STARTUP_SOURCES})

# The startup file uses only GAS directives (no C preprocessor),
# so no additional include paths are required at assembly level.
# Link dependency: needs SystemInit() from drivers, main() from app.
target_link_libraries(startup PUBLIC drivers)
