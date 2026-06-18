# Middlewares: third-party libraries (FreeRTOS placeholder)
# Will be populated when FreeRTOS kernel sources are added

add_library(middlewares INTERFACE)

target_include_directories(middlewares INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(middlewares INTERFACE drivers)
