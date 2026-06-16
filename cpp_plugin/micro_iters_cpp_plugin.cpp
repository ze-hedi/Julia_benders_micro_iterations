#include <map>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include "gridModelisation.h"
#include "micro_iterations_logger.h"

#include <boost/mpi.hpp>
#include <boost/tokenizer.hpp>

namespace mpi = boost::mpi;

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


std::shared_ptr<Plugin> get_plugin(std::string data_path = "", mpi::communicator* world = nullptr)
{
    static auto gridModelisation_plugin = std::make_shared<Plugin>(data_path, world) ;
    return gridModelisation_plugin ;
}

static std::shared_ptr<MicroIterationsLog> micro_iterations_logger;

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


std::map<std::string, std::vector<std::string>>& get_added_constraints_families_per_sub() 
{
  static std::map<std::string, std::vector<std::string>> added_constraints_families_per_sub ; 
  return added_constraints_families_per_sub ; 
}


void clean_added_constraints_families_per_sub()
{
  auto& added_constraints_families_per_sub = get_added_constraints_families_per_sub() ;
  for (auto& [sub_name,constraints] : added_constraints_families_per_sub)
  {
    constraints.clear() ;
  }
}

const std::map<std::string,std::string> get_binary_variables_ids_map(const std::filesystem::path& input_root = ".")
{
  static auto binary_variables_ids = read_binary_variables_ids_map(input_root) ;
  return binary_variables_ids ;
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
    void OnBendersStart(std::vector<std::string> sub_problems, int rank, 
                        std::filesystem::path input_root, std::filesystem::path output_root, 
                        bool warm_start, mpi::communicator* wold, int log_level)

    {


        auto variables_to_follow_dict = get_variables_dictionary(output_root);
        auto constraints_dict = get_constraints_dict(input_root) ;

        auto plugin = get_plugin(input_root, wold) ;

        micro_iterations_logger = std::make_shared<MicroIterationsLog>(output_root, warm_start, wold, log_level) ;
        get_added_constraints_families_per_sub()  ;
    }

    void OnBendersEnd(){
    }
     

    void OnBendersIterationStart()
    {
        clean_added_constraints_families_per_sub(); 
    }

    void OnBendersIterationEnd(){
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
        auto& variables_dict = get_variables_dictionary(input_root);
        auto& added_constraints_families_per_sub = get_added_constraints_families_per_sub();
        auto plugin = get_plugin();
        std::map<std::string, double> F_N_values ;
        for (int i=0; i<variables_indices_vector.size(); ++i)
        {
            F_N_values[variables_dict.at(variables_names_vector[i])] = sub_solution[variables_indices_vector[i]] ;
        }
        auto constraints_families = plugin->return_constraints_for_micro_iteration(sub_name, F_N_values);
        for (const auto& constraint_family : constraints_families)
        {
            if (!check_if_constraints_family_added(sub_name, constraint_family.c_str()))
            {
                added_constraints_families_per_sub[sub_name].push_back(constraint_family) ;
                constraints_to_add_vec.insert(constraints_to_add_vec.end(), constraints_dict[constraint_family].begin(), constraints_dict[constraint_family].end()) ;
            }
        }

        micro_iterations_logger->DumpAddedConstraints(num_micro_iter, num_master_iter, sub_name, constraints_to_add_vec);
        micro_iterations_logger->AddMicroIterionLog(sub_name, num_micro_iter, num_master_iter, solving_time, constraints_to_add_vec);

    }

    void OnBendersMasterResolutionStart()
    {
    }


    void  OnBendersMasterResolutionEnd(
                std::map<std::string, double>& master_out,
                int& num_iter,
                mpi::communicator* world,
                std::map<std::string, std::vector<std::string>>& added_constraintes_per_sub,
                std::filesystem::path input_root)

    {
        auto plugin = get_plugin() ;
        if (world->rank() == 0) {
            auto binary_vars_map = get_binary_variables_ids_map(input_root);
            std::map<std::string, int> z_dict;
            for (const auto& [var_name, var_id] : binary_vars_map) {
                if (master_out.count(var_name)) {
                    z_dict[var_id] = static_cast<int>(master_out[var_name]);
                }
            }
            auto chrono_start = std::chrono::high_resolution_clock::now();
            plugin->compute_factors_for_micro_iterations(z_dict) ;
            auto chrono_end = std::chrono::high_resolution_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(chrono_end - chrono_start).count();
            micro_iterations_logger->AddMasterIterationLog(num_iter, std::to_string(elapsed_ms));
        }
        plugin->broadcast_factors() ; 
    }


    void OnBendersSubResolutionStart()
    {
    }
    void OnBendersSubResolutionEnd(std::string sub_name, int num_micro_iter)
    {
    }



        

}