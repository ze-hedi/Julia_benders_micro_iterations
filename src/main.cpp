#include "gridModelisation.h"
#include "iostream"

int main()
{
    std::string files_input = "./cpp_data_strucutre" ;
    mpi::communicator world ;
    auto plugin = Plugin(files_input, &world) ;
    std::cout << "Plugin loaded" << std::endl ;

    // // Load z_dict from JSON
    // auto z_dict = load_dict<std::string, int>("z_dict.json") ;
    // std::cout << "Loaded z_dict with " << z_dict.size() << " entries" << std::endl ;

    // plugin.compute_factors_for_micro_iterations(z_dict) ;
    // std::cout << "compute_factors_for_micro_iterations done" << std::endl ;

    return 0 ;
}  // // Load z_dict from JSON
    // auto z_dict = load_dict<std::string, int>("z_dict.json") ;
    // std::cout << "Loaded z_dict with " << z_dict.size() << " entries" << std::endl ;

    // plugin.compute_factors_for_micro_iterations(z_dict) ;
    // std::cout << "compute_factors_for_micro_iterations done" << std::endl ;
