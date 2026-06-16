#include <fstream>
#include <sstream>
#include "micro_iterations_logger.h"

MicroIterationsLog::MicroIterationsLog(
  const std::filesystem::path& output_root,
  bool warm_start,
  mpi::communicator* world,
  int log_level)
{
    warm_start_ = warm_start;
    _world = world;
    log_level_ = log_level;
    std::filesystem::path micro_iterations_log_path = std::filesystem::path(output_root_)
                                                      / std::filesystem::path(
                                                        "micro_iterations_proc_"
                                                        + std::to_string(world->rank()) + ".log");

    log_file_.open(micro_iterations_log_path.c_str());
    if (log_file_.is_open()) 
    {
    }

    output_root_ = output_root ; 

    if (world->rank() == 0)
    {
        log_file_
          << "************************** MICRO ITERS config ************************** \n\n";
        if (warm_start_)
        {
            log_file_ << "warm_start=1\n\n";
        }
        else
        {
            log_file_ << "warm_start=0\n\n";
        }

        std::filesystem::path added_constraints_repo_path = std::filesystem::path(
                                                              output_root_)
                                                            / "added_constraints";
        if (!std::filesystem::exists(added_constraints_repo_path))
        {
            std::filesystem::create_directories(added_constraints_repo_path);
        }
        else
        {
            std::cout << "added constraints folder already exist  !!!" << std::endl;
        }
    }
}

void MicroIterationsLog::AddMasterIterationLog(int num_iter, std::string elapsed_time)
{
    log_file_ <<"master iteration "<<num_iter<<" elapsed time : "<<elapsed_time<<" ms" <<std::endl ; 
}

void MicroIterationsLog::AddMicroIterionLog(std::string sub_name,
                                            int num_micro_iter, 
                                            int num_master_iter, 
                                            std::string solving_time,
                                            std::vector<std::string> added_constraints_keys)
                                            
{
    log_file_ << sub_name << " ; " << num_master_iter << " ; " << num_micro_iter << " ; " << solving_time << "\n";
}

void MicroIterationsLog::AddMicroIterCount(std::string sub_name, int num_micro_iter)
{
    log_file_ << sub_name << " ; " << num_micro_iter << "\n";
}
