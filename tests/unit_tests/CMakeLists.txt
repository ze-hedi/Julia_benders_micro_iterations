add_executable(test_micro_iterations_logger test_micro_iterations_logger.cpp)

target_link_libraries(test_micro_iterations_logger
    PRIVATE
        gtest
        utils
        Boost::mpi
        Boost::serialization
        MPI::MPI_CXX
)

add_test(
    NAME test_micro_iterations_logger
    COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 1 $<TARGET_FILE:test_micro_iterations_logger>
)

add_executable(test_plugin test_plugin.cpp)

target_include_directories(test_plugin
    PRIVATE
        ${CMAKE_SOURCE_DIR}/cpp_plugin
)

target_link_libraries(test_plugin
    PRIVATE
        gtest
        utils
        Boost::mpi
        Boost::serialization
        MPI::MPI_CXX
        Eigen3::Eigen
        nlohmann_json::nlohmann_json
)

add_test(
    NAME test_plugin
    COMMAND $<TARGET_FILE:test_plugin>
)
