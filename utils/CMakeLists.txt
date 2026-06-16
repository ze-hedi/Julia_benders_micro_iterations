add_library(utils INTERFACE)

target_include_directories(utils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_sources(utils INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/micro_iterations_logger.cpp
)
