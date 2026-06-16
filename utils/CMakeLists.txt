add_library(utils INTERFACE)

target_include_directories(utils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(utils 
                INTERFACE  
                nlohmann_json::nlohmann_json
  )
target_sources(utils INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/micro_iterations_logger.cpp
)
