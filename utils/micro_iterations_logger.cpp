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
            std::filesystem::remove_all(added_constraints_repo_path);
            std::filesystem::create_directories(added_constraints_repo_path);
        }
    }

    // Start the background worker thread
    worker_thread_ = std::thread(&MicroIterationsLog::workerLoop, this);
    http_thread_ = std::thread(&MicroIterationsLog::httpWorkerLoop, this);
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

    {
        std::lock_guard<std::mutex> lock(http_mutex_);
        http_stop_ = true;
    }
    http_cv_.notify_one();
    if (http_thread_.joinable()) {
        http_thread_.join();
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

void MicroIterationsLog::enqueueHttp(std::string path, std::string body)
{
    {
        std::lock_guard<std::mutex> lock(http_mutex_);
        http_queue_.emplace(std::move(path), std::move(body));
    }
    http_cv_.notify_one();
}

void MicroIterationsLog::httpWorkerLoop()
{
    while (true) {
        std::pair<std::string, std::string> request;
        {
            std::unique_lock<std::mutex> lock(http_mutex_);
            http_cv_.wait(lock, [this] { return http_stop_ || !http_queue_.empty(); });

            if (http_stop_ && http_queue_.empty()) {
                break;
            }

            request = std::move(http_queue_.front());
            http_queue_.pop();
        }
        // failures are reported on stderr by the sender; never block or abort the solver
        http_sender_.Send(request.first, request.second);
    }
}

void MicroIterationsLog::AddMasterIterationLog(int num_iter,
                                               double master_resolution_time,
                                               std::string elapsed_time)
{
    // every rank calls this with the same broadcast values: only rank 0 notifies the frontend
    if (_world->rank() == 0)
    {
        nlohmann::json payload = {{"num_master_iter", num_iter},
                                  {"elapsed_time", elapsed_time.empty() ? 0.0 : std::stod(elapsed_time)},
                                  {"master_resolution_time", master_resolution_time * 1000.0}};
        enqueueHttp("/master_times", payload.dump());
    }

    enqueue([this, num_iter, master_resolution_time, elapsed_time = std::move(elapsed_time)] {
        log_file_ << "master iteration " << num_iter << " elapsed time : " << elapsed_time << " ms"
                  << " ; master resolution time : " << master_resolution_time << " s" << std::endl;
    });
}

void MicroIterationsLog::AddMicroIterionLog(std::string sub_name,
                                            int num_micro_iter,
                                            int num_master_iter,
                                            std::string solving_time,
                                            std::vector<std::string> added_constraints_keys,
                                            std::string elapsed_time)
{
    nlohmann::json payload = {{"rank", _world->rank()},
                              {"sub_name", sub_name},
                              {"num_master_iter", num_master_iter},
                              {"num_micro_iter", num_micro_iter},
                              {"nb_added_constraints", added_constraints_keys.size()},
                              {"solving_time", solving_time},
                              {"elapsed_time", elapsed_time}};
    enqueueHttp("/micro_iter", payload.dump());

    enqueue([this, sub_name = std::move(sub_name), num_micro_iter, num_master_iter,
             solving_time = std::move(solving_time),
             added_constraints_keys = std::move(added_constraints_keys),
             elapsed_time = std::move(elapsed_time)] {
        log_file_ << sub_name << " ; " << num_master_iter << " ; " << num_micro_iter << " ; " <<added_constraints_keys.size()<< " ; " << solving_time <<" ms"<< " ; " << elapsed_time << " ms\n";
    });
}

void MicroIterationsLog::DumpAddedConstraints(int num_micro_iter,
                                               int num_master_iter,
                                               std::string sub_name,
                                               const std::vector<std::string>& constraints_to_add,
                                               const std::vector<bool>& constraints_to_add_is_N,
                                               size_t start,
                                               size_t end)
{
    nlohmann::json constraints = nlohmann::json::array();
    for (size_t i = start; i < end; ++i) {
        constraints.push_back({{"name", constraints_to_add[i]},
                               {"is_N", static_cast<bool>(constraints_to_add_is_N[i])}});
    }
    nlohmann::json payload = {{"rank", _world->rank()},
                              {"sub_name", sub_name},
                              {"num_master_iter", num_master_iter},
                              {"num_micro_iter", num_micro_iter},
                              {"constraints", constraints}};
    enqueueHttp("/added_constraints", payload.dump());

    enqueue([this, num_micro_iter, num_master_iter,
             sub_name = std::move(sub_name),
             constraints_to_add,constraints_to_add_is_N, start, end] {
        std::filesystem::path added_constraints_dir = output_root_ / "added_constraints";
        std::string clean_sub_name = sub_name;
        auto last_slash = clean_sub_name.rfind('/');
        if (last_slash != std::string::npos)
            clean_sub_name = clean_sub_name.substr(last_slash + 1);
        auto mps_pos = clean_sub_name.rfind(".mps");
        if (mps_pos != std::string::npos)
            clean_sub_name = clean_sub_name.substr(0, mps_pos);

        std::filesystem::path sub_dir = added_constraints_dir
                                      / ("master_" + std::to_string(num_master_iter))
                                      / clean_sub_name;
        if (!std::filesystem::exists(sub_dir))
            std::filesystem::create_directories(sub_dir);

        std::string filename = "micro_iter_" + std::to_string(num_micro_iter) + ".txt";
        std::filesystem::path filepath = sub_dir / filename;

        std::ofstream out(filepath);
        for (size_t i = start; i < end; ++i) {
            out << constraints_to_add[i] << " " << (constraints_to_add_is_N[i] ? 1 : 0) << "\n";
        }
    });
}

void MicroIterationsLog::AddMicroIterCount(std::string sub_name, int num_micro_iter)
{
    enqueue([this, sub_name = std::move(sub_name), num_micro_iter] {
        log_file_ << sub_name << " ; " << num_micro_iter << "\n";
    });
}
