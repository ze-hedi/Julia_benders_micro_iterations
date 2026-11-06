// This header file will contain all the necessary objects to handle micro iterations workflow
// logging


#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <nlohmann/json.hpp>

#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/mpi.hpp>
#include "iostream"

#include "micro_iterations_http_sender.h"

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
        Called in the benders master resolution end callback.
        It will set the number of iteration, the master resolution time and the time to compute
        the new PTDF.
        @input
            - num_iter : number of master iteration
            - master_resolution_time : elapsed time of the master resolution (ms)
            - elapsed_time : elapsed time of computing PTDF
    */
    void AddMasterIterationLog(int num_iter,
                               double master_resolution_time,
                               std::string elapsed_time = "");

    /*
        Called in the benders iteration end callback.
        Sends the number of cuts added during the iteration over HTTP only (POST /added_cuts).
        @input
            - n_cuts_added : number of cuts added to the master during the iteration
    */
    void AddCutsLog(int n_cuts_added);

    /*
        Called in the micro iteration end callback.
        It will set the output data we want to dump for micro iterations for each subproblem and
        at each master iteration.
        @inputs :
            - sub_name : subproblem name
            - solving_name : time to solve subproblem
            - adding_rows_time : elapsed time to add constraints
            - added_constraints_keys : keys of added constraints
            - n_saturated_constraints : number of saturated constraints
            - n_all_constraints : total number of constraints
            - n_added_branches_N : branches added in N at this micro iteration (HTTP only)
            - n_added_branches_NK : branches added in N-1 at this micro iteration (HTTP only)
            - n_added_constraints_N : individual constraints behind the N branches (HTTP only)
            - n_added_constraints_NK : individual constraints behind the N-1 branches (HTTP only)
    */
    void AddMicroIterionLog(std::string sub_name,
                            int num_micro_iter,
                            int num_master_iter,
                            std::string solving_time,
                            std::vector<std::string> added_constraints_keys,
                            std::string elapsed_time,
                            int n_saturated_constraints,
                            int n_all_constraints,
                            int n_added_branches_N,
                            int n_added_branches_NK,
                            int n_added_constraints_N,
                            int n_added_constraints_NK);

    void AddMicroIterCount(std::string sub_name, int num_micro_iter);

    void DumpAddedConstraints(int num_micro_iter,
                              int num_master_iter,
                              std::string sub_name,
                              const std::vector<std::string>& constraints_to_add,
                              const std::vector<bool>& constraints_to_add_is_N,
                              size_t start,
                              size_t end,
                              const std::vector<std::string>& active_constraints = {});

    // HTTP: tell the server the Benders run is over (POST /end)
    void NotifyEnd();

    ~MicroIterationsLog();

    // non-copyable, non-movable (owns a thread)
    MicroIterationsLog(const MicroIterationsLog&) = delete;
    MicroIterationsLog& operator=(const MicroIterationsLog&) = delete;

private:
    void enqueue(std::function<void()> task);
    void workerLoop();
    void enqueueHttp(std::string path, std::string body);
    void httpWorkerLoop();

    std::filesystem::path output_root_ ;
    mpi::communicator* _world;
    bool warm_start_;
    std::map<std::string, std::string> sub_constraints_map_;
    std::ofstream log_file_;
    int log_level_;

    // async logging
    std::thread worker_thread_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<std::function<void()>> task_queue_;
    bool stop_ = false;

    // async http sending (POST /micro_iter, /master_times, /added_constraints and /added_cuts on localhost:3123), own thread so a
    // slow or absent server never delays file logging. Queue items are {path, body}.
    MicroIterationsHttpSender http_sender_{"127.0.0.1", 3123};
    std::thread http_thread_;
    std::mutex http_mutex_;
    std::condition_variable http_cv_;
    std::queue<std::pair<std::string, std::string>> http_queue_;
    bool http_stop_ = false;
};
