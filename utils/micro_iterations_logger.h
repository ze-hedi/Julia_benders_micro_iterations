// This header file will contain all the necessary objects to handle micro iterations workflow
// logging


#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include "micro_iterations_logger.h"

#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/mpi.hpp>
#include "iostream"

namespace mpi = boost::mpi;


/*
This structure contains the master iteration data that we want want to log
    - num_iter : number of master iteration
    - PTDF_compute_time : elapsed time to compute the new PTDF
    - removing_rows_per_sub_time : elapsed time to remove the constraints added to subproblem
workers
*/

/*
This map will be used will be attached to each master iteration.
we map each subproblem into the different micoiteration during a benders iteration
*/
// using MicroIterationsPerSub = std::map<std::string, std::vector<MicroIterationLog>>;

class MicroIterationsLog
{
public:
    /*

        Constructor
        @input :
            - options : configuration of the study
            - sub_constraints_map : mapping sub to constraints
            - constraints_per_line : mapping constraint keys to the list of constraints to add
            - warm_start : handle warm_start mechanism
            - world : the mpi communicator
            - log_level : log level necessary to determine if we dump the added keys at each micro
       iteration if >= 3

    */
    MicroIterationsLog(const std::filesystem::path& output_root,
                       bool warm_start,
                       mpi::communicator* world,
                       int log_level);

    /*
        Called in the benders master iteration start callback.
        It will set the number of iteration and the time to compute the new PTDF.
        @input
            - num_iter : number of master iteration
            - elapsed_time : elapsed time of computing PTDF
    */
    void AddMasterIterationLog(int num_iter, std::string elapsed_time = "");

    /*
        Called in the micro iteration end callback.
        It will set the output data we want to dump for micro iterations for each subproblem and
        at each master iteration.
        @inputs :
            - sub_name : subproblem name
            - solving_name : time to solve subproblem
            - adding_rows_time : elapsed time to add constraints
            - added_constraints_keys : keys of added constraints
    */
    void AddMicroIterionLog(std::string sub_name,
                            int num_micro_iter,
                            int num_master_iter, 
                            std::string solving_time,
                            std::vector<std::string> added_constraints_keys);

    void AddMicroIterCount(std::string sub_name, int num_micro_iter);

private:
    std::filesystem::path output_root_ ; 
    mpi::communicator* _world;
    bool warm_start_;
    std::map<std::string, std::string> sub_constraints_map_;
    std::ofstream log_file_;
    int log_level_;
};
