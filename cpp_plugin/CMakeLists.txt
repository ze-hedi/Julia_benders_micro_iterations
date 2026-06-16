add_library(cpp_plugin SHARED micro_iters_cpp_plugin.cpp) 

target_include_directories(cpp_plugin PRIVATE $(CMAKE_CURRENT_SOURCE_DIR))

target_link_libraries(cpp_plugin
        PRIVATE
            MPI::MPI_CXX
            Boost::mpi
            Boost::serialization
            Eigen3::Eigen
            nlohmann_json::nlohmann_json
            utils
            )

set_target_properties(cpp_plugin PROPERTIES 
            OUTPUT_NAME "cpp_plugin"
            PREFIX ""
            SUFFIX ".so") 