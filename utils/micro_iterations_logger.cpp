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
    output_root_ = output_root;

    std::filesystem::path micro_iterations_log_path = std::filesystem::path(output_root_)
                                                      / std::filesystem::path(
                                                        "micro_iterations_proc_"
                                                        + std::to_string(world->rank()) + ".log");

    log_file_.open(micro_iterations_log_path.c_str());

    log_file_ << "************************** MICRO ITERS config ************************** \n\n";
    if (warm_start_)
    {
        log_file_ << "warm_start=1\n\n";
    }
    else
    {
        log_file_ << "warm_start=0\n\n";
    }

    if (world->rank() == 0)
    {
        std::filesystem::path added_constraints_repo_path = std::filesystem::path(
                                                              output_root_)
                                                            / "added_constraints";
        if (!std::filesystem::exists(added_constraints_repo_path))
        {
            std::filesystem::create_directories(added_constraints_repo_path);
        }
        else
        {
            for (const auto& entry : std::filesystem::directory_iterator(added_constraints_repo_path))
            {
                std::filesystem::remove(entry.path());
            }
        }
    }

    // Start the background worker thread
    worker_thread_ = std::thread(&MicroIterationsLog::workerLoop, this);
}

MicroIterationsLog::~MicroIterationsLog()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    queue_cv_.notify_one();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void MicroIterationsLog::enqueue(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(std::move(task));
    }
    queue_cv_.notify_one();
}

void MicroIterationsLog::workerLoop()
{
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return stop_ || !task_queue_.empty(); });

            if (stop_ && task_queue_.empty()) {
                break;
            }

            task = std::move(task_queue_.front());
            task_queue_.pop();
        }
        task();
    }
}

void MicroIterationsLog::AddMasterIterationLog(int num_iter, std::string elapsed_time)
{
    enqueue([this, num_iter, elapsed_time = std::move(elapsed_time)] {
        log_file_ << "master iteration " << num_iter << " elapsed time : " << elapsed_time << " ms" << std::endl;
    });
}

void MicroIterationsLog::AddMicroIterionLog(std::string sub_name,
                                            int num_micro_iter,
                                            int num_master_iter,
                                            std::string solving_time,
                                            std::vector<std::string> added_constraints_keys)
{
    enqueue([this, sub_name = std::move(sub_name), num_micro_iter, num_master_iter,
             solving_time = std::move(solving_time),
             added_constraints_keys = std::move(added_constraints_keys)] {
        log_file_ << sub_name << " ; " << num_master_iter << " ; " << num_micro_iter << " ; " << solving_time << "\n";
    });
}

void MicroIterationsLog::DumpAddedConstraints(int num_micro_iter,
                                               int num_master_iter,
                                               std::string sub_name,
                                               const std::vector<std::string>& constraints_to_add)
{
    enqueue([this, num_micro_iter, num_master_iter,
             sub_name = std::move(sub_name),
             constraints_to_add] {
        std::filesystem::path added_constraints_dir = output_root_ / "added_constraints";
        std::string clean_sub_name = sub_name;
        auto last_slash = clean_sub_name.rfind('/');
        if (last_slash != std::string::npos)
            clean_sub_name = clean_sub_name.substr(last_slash + 1);
        auto mps_pos = clean_sub_name.rfind(".mps");
        if (mps_pos != std::string::npos)
            clean_sub_name = clean_sub_name.substr(0, mps_pos);

        std::string filename = "micro_iter_" + std::to_string(num_micro_iter)
                             + "_master_" + std::to_string(num_master_iter)
                             + "_" + clean_sub_name + ".txt";
        std::filesystem::path filepath = added_constraints_dir / filename;

        std::ofstream out(filepath);
        for (const auto& constraint : constraints_to_add) {
            out << constraint << "\n";
        }
    });
}

void MicroIterationsLog::AddMicroIterCount(std::string sub_name, int num_micro_iter)
{
    enqueue([this, sub_name = std::move(sub_name), num_micro_iter] {
        log_file_ << sub_name << " ; " << num_micro_iter << "\n";
    });
}
