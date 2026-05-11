#include "micro_iters.h"
#include "micro_iterations_logger.h"


#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

#include <boost/mpi.hpp>
#include <boost/tokenizer.hpp>

namespace mpi = boost::mpi;

std::shared_ptr<MicroIterationsLog> build_micro_iterations_logger(  const std::filesystem::path& output_root,
  bool warm_start,
  mpi::communicator* world,
  int log_level) 
{

  auto micro_iterations_logger = std::make_shared<MicroIterationsLog>(
    output_root, 
    warm_start, 
    world, 
    log_level
  ) ; 

  return  micro_iterations_logger ; 

}



std::shared_ptr<MicroIterationsLog> get_micro_iterations_logger(  const std::filesystem::path& output_root="",
  bool warm_start = false,
  mpi::communicator* world = nullptr,
  int log_level= 1) 
{ 
  static auto micro_iters_logger = build_micro_iterations_logger(output_root, warm_start, world, log_level) ; 
  return micro_iters_logger;
}

std::map<std::string,std::vector<std::string>> read_constraints_dict(std::filesystem::path& input_root)
{

    std::map<std::string,std::vector<std::string>> result ; 
    std::filesystem::path constraints_csv_path = input_root/"plugin_inputs" / "constraints_dictionary.csv";
    std::ifstream constraints_csv_stream(constraints_csv_path.c_str());

    if (constraints_csv_stream.is_open())
    {
        std::string line;
        typedef boost::tokenizer<boost::escaped_list_separator<char>> Tokenizer;

        while (std::getline(constraints_csv_stream, line))
        {
            Tokenizer tok(line);
            std::vector<std::string> tokens(tok.begin(), tok.end());
            std::string key = tokens[0];
            std::vector<std::string> values;
            if (tokens.size() > 1)
            {
                values.assign(tokens.begin() + 1, tokens.end());
            }

            result[key] = values;
        }
    }
    else
    {
        std::cerr << "unable to open : " << constraints_csv_path.c_str() << std::endl;
        exit(EXIT_FAILURE);
    }
    return result ; 
  }

std::map<std::string,std::vector<std::string>>  get_constraints_dict(std::filesystem::path& input_root)
{
  static auto constraints_dict = read_constraints_dict(input_root); 
  return constraints_dict ; 
}


std::map<std::string, std::string> read_variables_dictionary(
  const std::filesystem::path& input_root)
{
    std::filesystem::path variables_dictionary_path = input_root/"plugin_inputs" / "variables_dictionary.csv";
    std::ifstream variables_dict(variables_dictionary_path.c_str());
    std::map<std::string, std::string> variables_to_follow;
    if (variables_dict.is_open())
    {
        std::string row;
        typedef boost::tokenizer<boost::escaped_list_separator<char>> Tokenizer;
        while (std::getline(variables_dict, row))
        {
            Tokenizer tok(row);
            std::vector<std::string> tokens(tok.begin(), tok.end());
            variables_to_follow[tokens[1]] = tokens[0];
        }
    }
    else
    {
        std::cerr << "unable to open : " << variables_dictionary_path.c_str() << std::endl;
        exit(EXIT_FAILURE);
    }
    return variables_to_follow;
}




const std::map<std::string, std::string>& get_variables_dictionary(
  const std::filesystem::path& input_root = ".")
{

    static auto variables_to_follow_dict = read_variables_dictionary(input_root);
    return variables_to_follow_dict;
}


std::map<std::string,std::string> read_binary_variables_ids_map(const std::filesystem::path& input_root = ".")
{


    std::map<std::string,std::string> binary_variables_ids_map_ ;  
    // Reading investement dictionary
    std::filesystem::path investment_dictionary_path = input_root /"plugin_inputs"/ "investment_dictionary.csv";
    std::ifstream investment_dict_path(investment_dictionary_path.c_str());

    if (investment_dict_path.is_open())
    {
        std::string row;
        typedef boost::tokenizer<boost::escaped_list_separator<char>> Tokenizer;

        while (std::getline(investment_dict_path, row))
        {
            Tokenizer tok(row);
            std::vector<std::string> tokens(tok.begin(), tok.end());
            binary_variables_ids_map_[tokens[1]] = tokens[0];
        }
    }
    else
    {
        std::cerr << "unable to open : " << investment_dictionary_path.c_str() << std::endl;
        exit(EXIT_FAILURE);
    }
    return binary_variables_ids_map_ ; 
}

const std::map<std::string,std::string> get_binary_variables_ids_map(const std::filesystem::path& input_root = ".")
{
  static auto binary_variables_ids = read_binary_variables_ids_map(input_root) ; 
  return binary_variables_ids ; 
}



std::map<std::string, std::vector<std::string>>& get_added_constraints_families_per_sub() 
{
  static std::map<std::string, std::vector<std::string>> added_constraints_families_per_sub ; 
  return added_constraints_families_per_sub ; 
}


void clean_added_constraints_families_per_sub() 
{
  auto& added_constraints_families_per_sub = get_added_constraints_families_per_sub() ; 
  for (auto& [sub_name,_] : added_constraints_families_per_sub) 
  {
    added_constraints_families_per_sub.clear() ; 
  }
}

bool check_if_constraints_family_added(std::string sub_name,const char* violated_constraints_family) 
{
  auto& added_constraints_families_per_sub = get_added_constraints_families_per_sub() ; 
  auto constraint_family_iter = std::find(added_constraints_families_per_sub[sub_name].begin(), added_constraints_families_per_sub[sub_name].end(),violated_constraints_family); 
  if (constraint_family_iter != added_constraints_families_per_sub[sub_name].end())
    return true ; 
  else 
    return false ; 
}

extern "C"
{
void OnBendersStart(SubProblemsIds sub_problem_ids, int rank,std::filesystem::path input_root, std::filesystem::path output_root, 
                  bool warm_start, mpi::communicator* world, int log_level )
{
    init_julia(0, NULL);
    static std::map<std::string, std::vector<std::string>> added_constraints_families_per_sub ; 
    jl_load_variables(sub_problem_ids, rank);


    auto variables_to_follow_dict = get_variables_dictionary(output_root);
    auto constraints_dict = get_constraints_dict(input_root) ; 

    auto micro_iteration_logger = get_micro_iterations_logger(output_root, warm_start, world,log_level) ; 

}

void OnBendersIterationStart()
{
  clean_added_constraints_families_per_sub() ; 
  jl_gc_enable(0) ; 
}

void OnBendersIterationEnd()
{
  jl_gc_enable(1) , 
  jl_call_GC();
}

void OnBendersEnd()
{
    shutdown_julia(0);
}

void OnBendersMicroIterationStart()
{

}

void OnBendersMicroIterationEnd(std::string sub_name,
                                bool& added_rows,
                                std::string solving_time,
                                std::vector<double> sub_solution,
                                std::vector<int>& variables_indices_vector, 
                                std::vector<std::string>& variables_names_vector,
                                std::filesystem::path input_root, 
                                std::vector<std::string>& constraints_to_add_vec,
                                int num_master_iter, 
                                int num_micro_iter)
{

    auto constraints_dict = get_constraints_dict(input_root)  ; 
    std::vector<FlowN> flows_to_follow;
    flows_to_follow.reserve(variables_indices_vector.size());
    auto  variables_dict = get_variables_dictionary(input_root) ; 
 
    for (int i=0; i<variables_indices_vector.size(); i++) 
    {
      auto value = sub_solution[variables_indices_vector[i]] ; 
      FlowN flow;
      flow.flow_id = variables_dict[variables_names_vector[i]].c_str();
      flow.value = value;

      flows_to_follow.push_back(flow) ; 
    }

    FlowNList N_flows;
    N_flows.flows = flows_to_follow.data();
    N_flows.size = flows_to_follow.size(); 

    auto t1 = std::chrono::high_resolution_clock::now();

    ViolatedFlowConstraints constraints_to_add = jl_return_constraints_for_micro_iteration(
                                                sub_name.c_str(),
                                                N_flows);

    added_rows = constraints_to_add.size ; 
    std::vector<std::string> added_constraints_keys_at_micro_iteration ; 
    auto& added_constraints_families_per_sub = get_added_constraints_families_per_sub() ; 
    for (int i=0; i<constraints_to_add.size; i++) 
    {
      if (!check_if_constraints_family_added(sub_name,constraints_to_add.constraints[i]))
      {
        added_constraints_families_per_sub[sub_name].push_back(constraints_to_add.constraints[i]) ;
        added_constraints_keys_at_micro_iteration.push_back(constraints_to_add.constraints[i]) ; 
        constraints_to_add_vec.insert(constraints_to_add_vec.end(), constraints_dict[constraints_to_add.constraints[i]].begin(), constraints_dict[constraints_to_add.constraints[i]].end()) ; 
      } 
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    auto micro_iter_logger = get_micro_iterations_logger() ; 
    micro_iter_logger->AddMicroIterionLog(sub_name,num_micro_iter,num_master_iter,solving_time,added_constraints_keys_at_micro_iteration) ; 

}

void  OnBendersMasterResolutionEnd(
  std::map<std::string, double>& master_out,
  int& num_iter,
  mpi::communicator* world,
  std::map<std::string, std::vector<std::string>>& added_constraintes_per_sub,
  std::filesystem::path input_root)

{

    auto binary_variables_ids_map = get_binary_variables_ids_map(input_root) ;
    
    std::cout<<"binary_variables_ids_map size "<<binary_variables_ids_map.size()<<std::endl ; 

    for (auto& [sub, _]: added_constraintes_per_sub)
    {
        added_constraintes_per_sub[sub] = std::vector<std::string>();
    }

    std::vector<CandidateLineInvestmentStatus> candidates_iter_res;
    candidates_iter_res.reserve(master_out.size());
    for (auto& [line, value]: master_out)
    {
        auto id_in_csv = binary_variables_ids_map[line].c_str();
        candidates_iter_res.push_back(CandidateLineInvestmentStatus{id_in_csv, value});
    }
    std::cout<<"master out size "<<master_out.size()<<std::endl ;
    CandidateLineInvestmentStatusList master_benders_input = CandidateLineInvestmentStatusList{
      candidates_iter_res.data(),
      master_out.size()};



    auto t1 = std::chrono::high_resolution_clock::now();
    // SerializedFactors_mpi serialized_factors_mpi ;
    std::vector<uint8_t> HVDC_dict_serialized_buff;
    std::vector<uint8_t> dict_incident_factors_serialized_buff;
    std::vector<uint8_t> all_monitored_branches_serialized_buff;
    SerializedBuffers serialized_buffs;

    if (world->rank() == 0)
    {
        SerializedFactors serialized_factors = jl_compute_factors_for_microiterations(
          master_benders_input,
          num_iter);

        auto n_bytes_hvdc = serialized_factors.HVDC_dict_serialized.bytes_length ; 
        // serialized_buffs.HVDC_dict_serialized_buff.resize(
        //   serialized_factors.HVDC_dict_serialized.bytes_length);
        
        serialized_buffs.HVDC_dict_serialized_buff.assign(serialized_factors.HVDC_dict_serialized.bytes_ptr,serialized_factors.HVDC_dict_serialized.bytes_ptr + serialized_factors.HVDC_dict_serialized.bytes_length) ; 
        serialized_buffs.dict_incident_factors_serialized_buff.assign(serialized_factors.dict_incident_factors_serialized.bytes_ptr,serialized_factors.dict_incident_factors_serialized.bytes_ptr + serialized_factors.dict_incident_factors_serialized.bytes_length) ; 
        serialized_buffs.all_monitored_branches_serialized_buff.assign(serialized_factors.all_monitored_branches_serialized.bytes_ptr,serialized_factors.all_monitored_branches_serialized.bytes_ptr + serialized_factors.all_monitored_branches_serialized.bytes_length) ; 

        // serialized_buffs.dict_incident_factors_serialized_buff.assign()
        // std::memcpy(serialized_buffs.HVDC_dict_serialized_buff.data(),
        //             serialized_factors.HVDC_dict_serialized.bytes_ptr,
        //             serialized_factors.HVDC_dict_serialized.bytes_length * sizeof(uint8_t));

        // serialized_buffs.dict_incident_factors_serialized_buff.resize(
        //   serialized_factors.dict_incident_factors_serialized.bytes_length);
        // std::memcpy(serialized_buffs.dict_incident_factors_serialized_buff.data(),
        //             serialized_factors.dict_incident_factors_serialized.bytes_ptr,
        //             serialized_factors.dict_incident_factors_serialized.bytes_length
        //               * sizeof(uint8_t));

        // serialized_buffs.all_monitored_branches_serialized_buff.resize(
        //   serialized_factors.all_monitored_branches_serialized.bytes_length);
        // std::memcpy(serialized_buffs.all_monitored_branches_serialized_buff.data(),
        //             serialized_factors.all_monitored_branches_serialized.bytes_ptr,
        //             serialized_factors.all_monitored_branches_serialized.bytes_length
        //               * sizeof(uint8_t));
        jl_clean_buffers();
    }

    mpi::broadcast(*world, serialized_buffs, 0);

    if (world->rank() != 0)
    {
        SerializedFactors serialized_factors{
          SerializedObject{serialized_buffs.HVDC_dict_serialized_buff.data(),
                           serialized_buffs.HVDC_dict_serialized_buff.size()},
          SerializedObject{serialized_buffs.dict_incident_factors_serialized_buff.data(),
                           serialized_buffs.dict_incident_factors_serialized_buff.size()},
          SerializedObject{serialized_buffs.all_monitored_branches_serialized_buff.data(),
                           serialized_buffs.all_monitored_branches_serialized_buff.size()},
        };
        jl_deserialize_factors(serialized_factors);
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    auto elapsed_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1)
                                  .count();

    auto micro_iterations_logger = get_micro_iterations_logger()  ;
    micro_iterations_logger->AddMasterIterationLog(num_iter,std::to_string(elapsed_microseconds)) ; 

}

void OnBendersMasterResolutionStart()
{
}

void OnBendersSubResolutionStart()
{
}

void OnBendersSubResolutionEnd(std::string sub_name, int num_micro_iter)
{
}
}
