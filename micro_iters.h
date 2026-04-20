#pragma once

#include <map>
#include <string>
#include <vector>
#include <filesystem>

#include <boost/mpi.hpp>

namespace mpi = boost::mpi;

#include "MyLib.h"
extern "C"
{
#include "libGridModelisation/include/julia_init.h"
}

#ifdef __cplusplus
extern "C"
{
#endif

void OnBendersStart(SubProblemsIds, int, std::filesystem::path, std::filesystem::path, 
                       bool, mpi::communicator*, int);
void OnBendersEnd();
void OnBendersIterationStart();
void OnBendersIterationEnd();
void OnBendersMasterResolutionStart(
  std::map<std::string, double>& master_out,
  int& num_iters,
  mpi::communicator* world,
  std::map<std::string, std::vector<std::string>>& added_constraintes_per_sub,
  std::filesystem::path input_root);
void OnBendersMasterResolutionEnd();
void OnBendersMicroIterationStart();
void OnBendersMicroIterationEnd(std::string sub_name,
                                bool& added_rows,
                                std::string solving_time,
                                std::vector<double> sub_solution,
                                std::vector<int>& variables_indices_vector,
                                std::vector<std::string>& variables_names_vector,
                                std::filesystem::path input_root,
                                std::vector<std::string>& constraints_to_add_vec, 
                                int num_master_iter, 
                                int num_micro_iter);
void OnBendersSubResolutionStart();
void OnBendersSubResolutionEnd(std::string sub_name, int num_micro_iter);

#ifdef __cplusplus
}
#endif
