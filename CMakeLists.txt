


CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

PROJECT(Julia_Benders_Micro_ITERATIONS VERSION 1.0.0 LANGUAGES CXX C) 

set(CMAKE_CXX_STANDARD 20) 
set (CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(MPI REQUIRED)
find_package(Boost 1.66 REQUIRED COMPONENTS mpi serialization)
find_package(Eigen3 REQUIRED)
find_package(nlohmann_json REQUIRED)

add_subdirectory(utils)
add_subdirectory(cpp_plugin)