#ifndef MYLIB_H
#define MYLIB_H

#include <cstdint>
#include <map>
#include <vector>
#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

#ifdef __cplusplus
extern "C" {
#endif

struct CandidateLineInvestmentStatus
{
    const char* candidate_line_id;
    int is_invested;
};

struct CandidateLineInvestmentStatusList
{
    CandidateLineInvestmentStatus* candidates_res;
    int size;
};



struct SerializedObject
{
    uint8_t* bytes_ptr;
    int bytes_length;
};

struct SubProblemsIds 
{
    char** subProblems_ids ; 
    int n_subproblems ; 
} ; 

struct SerializedFactors
{
    SerializedObject HVDC_dict_serialized;
    SerializedObject dict_incident_factors_serialized;
    SerializedObject all_monitored_branches_serialized;
};

struct FlowN
{
    const char* flow_id;
    double value;
};

struct FlowNList
{
    FlowN* flows;
    int size;
};

struct ViolatedFlowConstraints
{
    const char** constraints;
    int size;
};

void jl_load_variables(SubProblemsIds,int) ; 
void jl_test_output() ; 
void jl_call_GC() ; 
int jl_gc_enable(int on); 
SerializedFactors jl_compute_factors_for_microiterations(CandidateLineInvestmentStatusList,int) ; 
void jl_clean_buffers() ; 
void jl_deserialize_factors(SerializedFactors) ; 
ViolatedFlowConstraints jl_return_constraints_for_micro_iteration(const char*,FlowNList) ; 



#ifdef __cplusplus
}
#endif

// C++ only structures (cannot be in extern "C" block)
struct SerializedBuffers 
{
    std::vector<uint8_t>  HVDC_dict_serialized_buff;
    std::vector<uint8_t> dict_incident_factors_serialized_buff;
    std::vector<uint8_t> all_monitored_branches_serialized_buff;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        ar & HVDC_dict_serialized_buff;
        ar & dict_incident_factors_serialized_buff;
        ar & all_monitored_branches_serialized_buff;
    }
};

#endif // MYLIB_H