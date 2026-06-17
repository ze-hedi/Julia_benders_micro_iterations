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
