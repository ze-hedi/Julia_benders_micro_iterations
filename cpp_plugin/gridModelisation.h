#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <nlohmann/json.hpp>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <cassert>
#include <set>
#include <tuple>
#include <cmath>
#include <type_traits>
#include <ranges>
#include "iostream"
#include <boost/mpi.hpp>
#include <boost/serialization/map.hpp>                                                                                                                                                                    
#include <boost/serialization/vector.hpp>                                                                                                                                                                 
#include <boost/serialization/string.hpp>                                                                                                                                                                 
#include <boost/serialization/utility.hpp>

namespace boost { namespace serialization {
template<class Archive, typename... Args>
void serialize(Archive& ar, std::tuple<Args...>& t, const unsigned int) {
    std::apply([&ar](auto&... args) { ((ar & args), ...); }, t);
}
}}

using json = nlohmann::json;
namespace mpi = boost::mpi ;

// ---- NamedMatrix ----
template <typename MatrixType, typename RowKey = std::string, typename ColKey = std::string>
struct NamedMatrix {
    MatrixType mat;
    std::map<RowKey, int> row_index;
    std::map<ColKey, int> col_index;
};

// ---- Binary readers ----


std::map<std::pair<std::string,std::string>, double> load_pair_keyed_dict(const std::string& path)
{
    std::ifstream f(path) ;
    json j = json::parse(f) ;

    std::map<std::pair<std::string,std::string>, double> dict ;
    for (auto& [k, v] : j.items())
    {
        // keys look like: ("str1", "str2")
        auto p1 = k.find('"') ;
        auto p2 = k.find('"', p1 + 1) ;
        auto p3 = k.find('"', p2 + 1) ;
        auto p4 = k.find('"', p3 + 1) ;
        std::string first = k.substr(p1 + 1, p2 - p1 - 1) ;
        std::string second = k.substr(p3 + 1, p4 - p3 - 1) ;
        dict[{first, second}] = v.get<double>() ;
    }
    return dict ;
}

std::map<std::string, std::vector<std::string>> load_dict_of_vectors(const std::string& path)
{
    std::ifstream f(path) ;
    json j = json::parse(f) ;

    std::map<std::string, std::vector<std::string>> dict ;
    for (auto& [k, v] : j.items())
    {
        dict[k] = v.get<std::vector<std::string>>() ;
    }
    return dict ;
}

template<typename key=std::string, typename value=std::string>
std::map<key,value> load_dict(const std::string& path)
{
    std::ifstream f(path) ;
    json j = json::parse(f) ;

    std::map<key,value> dict ;
    for (auto& [k,v] : j.items())
    {
        dict[k] = v ;
    }
    return dict ;
}

template <typename Key>
std::map<Key, int> load_names(const std::string& path) {
    std::ifstream f(path);
    json j = json::parse(f);

    std::map<Key, int> m;
    for (auto& [k, v] : j.items()) {
        Key key;
        if constexpr (std::is_same_v<Key, int>) {
            key = std::stoi(k);   // JSON keys are always strings
        } else {
            key = k;
        }
        m[key] = v.template get<int>() - 1;  // Julia 1-based -> C++ 0-based
    }
    return m;
}

NamedMatrix<Eigen::MatrixXd, int, int>
load_dense_named(const std::string& prefix) {
    NamedMatrix<Eigen::MatrixXd, int, int> nm;

    // Read binary blob
    std::ifstream f(prefix + ".bin", std::ios::binary);


    int64_t rows, cols;
    f.read(reinterpret_cast<char*>(&rows), 8);
    f.read(reinterpret_cast<char*>(&cols), 8);


    // Julia is column-major, Eigen default is column-major → direct copy
    nm.mat.resize(rows, cols);
    f.read(reinterpret_cast<char*>(nm.mat.data()), rows * cols * sizeof(double));

    nm.row_index = load_names<int>( prefix + "_rownames.json");
    nm.col_index = load_names<int>( prefix + "_colnames.json");
    return nm;
}


template <typename RowKey = int, typename ColKey = int>
NamedMatrix<Eigen::SparseMatrix<double>, RowKey, ColKey>
load_sparse_named(const std::string& prefix) {
    NamedMatrix<Eigen::SparseMatrix<double>, RowKey, ColKey> nm;

    std::ifstream f(prefix + ".coo", std::ios::binary);
    int64_t rows, cols, nnz;
    f.read(reinterpret_cast<char*>(&rows), 8);
    f.read(reinterpret_cast<char*>(&cols), 8);
    f.read(reinterpret_cast<char*>(&nnz), 8);

    std::vector<int64_t> I(nnz), J(nnz);
    std::vector<double> V(nnz);

    f.read(reinterpret_cast<char*>(I.data()), nnz * 8);
    f.read(reinterpret_cast<char*>(J.data()), nnz * 8);
    f.read(reinterpret_cast<char*>(V.data()), nnz * sizeof(double));

    // Build from triplets
    std::vector<Eigen::Triplet<double>> triplets(nnz);
    for (int64_t k = 0; k < nnz; ++k) {
        triplets[k] = {static_cast<int>(I[k]), static_cast<int>(J[k]), V[k]};
    }
    nm.mat.resize(rows, cols);
    nm.mat.setFromTriplets(triplets.begin(), triplets.end());

    nm.row_index = load_names<RowKey>(prefix + "_rownames.json");
    nm.col_index = load_names<ColKey>(prefix + "_colnames.json");
    return nm;
}


std::vector<std::string> load_branches_from_txt(const std::string& path) {
    std::vector<std::string> branches;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) branches.push_back(line);
    }
    return branches;
}

class Plugin
{

    public :
    Plugin(mpi::communicator* world)  {

        data_path_ = "./plugin_inputs/cpp_structures" ;
        world_ = world ;
        B_inv_ = load_dense_named(data_path_ + "/B_inv") ;
        Ab_  = load_sparse_named<std::string,std::string>(data_path_ + "/Ab") ;
        Yl_ =  load_sparse_named<std::string,std::string>(data_path_+"/Yl") ;
        A_hvdc_ = load_sparse_named<std::string,std::string>(data_path_+"/A_hvdc") ;
        branches_dict_ = load_dict(data_path_ + "/branches_to_candidates_dict.json") ;
        n_side1_dict_ = load_dict<std::string,int>(data_path_ + "/n_side1_dict.json" ) ;
        n_side2_dict_ = load_dict<std::string,int>(data_path_ + "/n_side2_dict.json") ;
        max_flows_N_K_ = load_pair_keyed_dict(data_path_ + "/max_flows_N_K.json") ;
        max_flows_N_ = load_pair_keyed_dict(data_path_ + "/max_flows_N.json") ;
        dict_incident_outage_AC_branches_ = load_dict_of_vectors(data_path_ + "/dict_incident_outage_AC_branches.json") ;
        dict_incident_HVDC_branches_ = load_dict_of_vectors(data_path_ + "/dict_incident_HVDC_branches.json") ;
    }

    void get_invested_branches(const std::map<std::string,int>& z_dict, std::vector<std::string>& invested_branches,
                                std::vector<std::string>& non_invested_branches)
    {
        invested_branches.clear();
        non_invested_branches.clear();
        for (const auto& [branch, candidate] : branches_dict_) {
            if (z_dict.at(candidate) == 1) invested_branches.push_back(branch);
            else non_invested_branches.push_back(branch);
        }
    }


    NamedMatrix<Eigen::MatrixXd,std::string,int> update_PTDF_after_lines_removal(std::vector<std::string>&non_invested_branches)
    {
        auto k = non_invested_branches.size();
        auto n = B_inv_.mat.rows() ;

        Eigen::MatrixXd A_k = Eigen::MatrixXd::Zero(n,k) ;

        for (int j = 0; j < k; ++j) {
            const auto& br = non_invested_branches[j];
            int row_idx = Ab_.row_index.at(br);
            Eigen::VectorXd a = Eigen::VectorXd(Ab_.mat.row(row_idx)) ;

            int row_idx_yl = Yl_.row_index.at(br) ;
            int col_idx_yl = Yl_.col_index.at(br) ;

            double yl = Yl_.mat.coeff(row_idx_yl, col_idx_yl) ;

            A_k.col(j) = std::sqrt(yl) * a ;

        }

        Eigen::MatrixXd I = Eigen::MatrixXd::Identity(k, k) ;
        Eigen::MatrixXd BinvA = B_inv_.mat * A_k ;
        Eigen::MatrixXd M = I - A_k.transpose() * BinvA ;
        Eigen::MatrixXd M_inv = M.inverse() ;

        decltype(B_inv_) Bnew_inv ;

        Bnew_inv.mat = B_inv_.mat + BinvA * M_inv * BinvA.transpose();
        Bnew_inv.row_index = B_inv_.row_index;
        Bnew_inv.col_index = B_inv_.col_index;

        std::vector<std::string> remaining_branches_Ab ;
        for (const auto&  [name,idx] : Ab_.row_index) {
            if (std::find(non_invested_branches.begin(), non_invested_branches.end(), name)
                            == non_invested_branches.end()) {
                remaining_branches_Ab.push_back(name) ;
            }
        }

        std::map<int, int> old_to_new_row_Ab;
        for (int i = 0; i < remaining_branches_Ab.size(); ++i) {
            old_to_new_row_Ab[Ab_.row_index.at(remaining_branches_Ab[i])] = i;
        }

        std::vector<Eigen::Triplet<double>> triplets_Ab ;
        for (int col = 0; col < Ab_.mat.cols(); ++col) {
            for (Eigen::SparseMatrix<double>::InnerIterator it(Ab_.mat, col); it; ++it) {
                auto found = old_to_new_row_Ab.find(it.row());
                if (found != old_to_new_row_Ab.end()) {
                    triplets_Ab.emplace_back(found->second, col, it.value());
                }
            }
        }

        Eigen::SparseMatrix<double> Ab_new(remaining_branches_Ab.size(),Ab_.mat.cols()) ;
        Ab_new.setFromTriplets(triplets_Ab.begin(), triplets_Ab.end()) ;

        std::vector<std::string> remaining_banches_Yl ;
        for (const auto& [name,idx] : Yl_.row_index ) {
            if (std::find(non_invested_branches.begin(), non_invested_branches.end(),name)
                            == non_invested_branches.end() ) {
                remaining_banches_Yl.push_back(name) ;
            }
        }

        std::map<int, int> old_to_new_row_Yl;
        std::set<int> kept_yl_cols;
        std::map<int, int> old_to_new_col_Yl;
        for (int i = 0; i < remaining_banches_Yl.size(); ++i) {
            old_to_new_row_Yl[Yl_.row_index.at(remaining_banches_Yl[i])] = i;
            int old_col = Yl_.col_index.at(remaining_banches_Yl[i]);
            kept_yl_cols.insert(old_col);
            old_to_new_col_Yl[old_col] = i;
        }

        std::vector<Eigen::Triplet<double>> triplets_Yl ;
        for (int col = 0; col < Yl_.mat.cols(); ++col) {
            if (!kept_yl_cols.count(col)) continue;
            for (Eigen::SparseMatrix<double>::InnerIterator it(Yl_.mat, col); it; ++it) {
                auto found = old_to_new_row_Yl.find(it.row());
                if (found != old_to_new_row_Yl.end()) {
                    triplets_Yl.emplace_back(found->second, old_to_new_col_Yl[col], it.value());
                }
            }
        }


        NamedMatrix<Eigen::SparseMatrix<double>,std::string,std::string> Yl_new ;
        Yl_new.mat.resize(remaining_banches_Yl.size(), remaining_banches_Yl.size()) ;
        for (int i = 0; i < remaining_banches_Yl.size(); ++i)
        {
            Yl_new.row_index[remaining_banches_Yl[i]] = i;
        }
        Yl_new.mat.setFromTriplets(triplets_Yl.begin(), triplets_Yl.end()) ;

        NamedMatrix<Eigen::MatrixXd,std::string,int> PTDF_new  ;
        PTDF_new.mat = Yl_new.mat * Ab_new * Bnew_inv.mat  ;
        PTDF_new.row_index = Yl_new.row_index ;
        PTDF_new.col_index = Bnew_inv.col_index;

        return PTDF_new ;
    }

    NamedMatrix<Eigen::MatrixXd,std::string,std::string> update_HVDC_sensi_after_lines_removal(NamedMatrix<Eigen::MatrixXd,std::string,int> & PTDF_new)
    {
        NamedMatrix<Eigen::MatrixXd,std::string,std::string> HVDC_sensitivity_matrix_new ;

        HVDC_sensitivity_matrix_new.mat = PTDF_new.mat*A_hvdc_.mat;
        HVDC_sensitivity_matrix_new.col_index = A_hvdc_.col_index ;
        HVDC_sensitivity_matrix_new.row_index = PTDF_new.row_index ;
        return HVDC_sensitivity_matrix_new ;
    }


    std::map<std::tuple<std::string,std::string,std::string>, double>
    compute_incident_factors_after_lines_removal(
        const std::vector<std::string>& all_monitored_branches,
        const std::vector<std::string>& branches_invested,
        const NamedMatrix<Eigen::MatrixXd,std::string,int>& PTDF_new)
    {
        std::map<std::tuple<std::string,std::string,std::string>, double> dict_incident_factors ;

        // Build PTDF lookup: (node, branch) -> value
        std::map<std::pair<int,std::string>, double> PTDF_new_dict ;
        for (const auto& [branch, row_idx] : PTDF_new.row_index) {
            // Only keep monitored branches
            if (std::find(all_monitored_branches.begin(), all_monitored_branches.end(), branch)
                    == all_monitored_branches.end()) continue ;
            for (const auto& [node, col_idx] : PTDF_new.col_index) {
                double val = PTDF_new.mat(row_idx, col_idx) ;
                if (val != 0.0) {
                    PTDF_new_dict[{node, branch}] = val ;
                }
            }
        }

        // Iterate over all incidents
        for (const auto& [incident_id, outage_AC_branches] : dict_incident_outage_AC_branches_) {

            int nb_outage = outage_AC_branches.size() ;
            if (nb_outage == 0) continue ;

            // Get side1/side2 node indexes and outage branch row indexes in PTDF
            std::vector<int> side1_cols(nb_outage), side2_cols(nb_outage), outage_rows(nb_outage) ;
            for (int i = 0; i < nb_outage; ++i) {
                const auto& br = outage_AC_branches[i] ;
                side1_cols[i] = PTDF_new.col_index.at(n_side1_dict_.at(br)) ;
                side2_cols[i] = PTDF_new.col_index.at(n_side2_dict_.at(br)) ;
                outage_rows[i] = PTDF_new.row_index.at(br) ;
            }

            // Extract PTDF sub-matrices for outage branches at side1/side2 nodes
            Eigen::MatrixXd PTDF_side1(nb_outage, nb_outage) ;
            Eigen::MatrixXd PTDF_side2(nb_outage, nb_outage) ;
            for (int i = 0; i < nb_outage; ++i) {
                for (int j = 0; j < nb_outage; ++j) {
                    PTDF_side1(i, j) = PTDF_new.mat(outage_rows[i], side1_cols[j]) ;
                    PTDF_side2(i, j) = PTDF_new.mat(outage_rows[i], side2_cols[j]) ;
                }
            }

            // FPM = inv(I - (PTDF_side1 - PTDF_side2))
            Eigen::MatrixXd I_mat = Eigen::MatrixXd::Identity(nb_outage, nb_outage) ;
            Eigen::MatrixXd FPM = (I_mat - (PTDF_side1 - PTDF_side2)).inverse() ;

            // Build name-to-local-index map for FPM
            std::map<std::string, int> outage_idx ;
            for (int i = 0; i < nb_outage; ++i) {
                outage_idx[outage_AC_branches[i]] = i ;
            }

            // Compute incident factors
            for (const auto& monitored_line : all_monitored_branches) {
                for (const auto& outage_line : outage_AC_branches) {
                    double incident_factor = 0.0 ;

                    for (const auto& outage_line_bis : outage_AC_branches) {
                        int node1 = n_side1_dict_.at(outage_line_bis) ;
                        int node2 = n_side2_dict_.at(outage_line_bis) ;

                        auto it1 = PTDF_new_dict.find({node1, monitored_line}) ;
                        double PTDF_node1 = (it1 != PTDF_new_dict.end()) ? it1->second : 0.0 ;

                        auto it2 = PTDF_new_dict.find({node2, monitored_line}) ;
                        double PTDF_node2 = (it2 != PTDF_new_dict.end()) ? it2->second : 0.0 ;

                        incident_factor += FPM(outage_idx[outage_line_bis], outage_idx[outage_line])
                                           * (PTDF_node1 - PTDF_node2) ;
                    }

                    dict_incident_factors[{monitored_line, incident_id, outage_line}] = incident_factor ;
                }
            }
        }

        // Threshold filtering
        double threshold_factors = 1e-5 ;
        for (auto& [k, v] : dict_incident_factors) {
            if (std::abs(v) < threshold_factors) v = 0.0 ;
        }

        return dict_incident_factors ;
    }

    void compute_sensis_after_lines_removal(std::vector<std::string>& non_invested_branches,NamedMatrix<Eigen::MatrixXd,std::string,int>& PTDF_new, NamedMatrix<Eigen::MatrixXd,std::string,std::string>& HVDC_sensitivity_matrix_new)
    {
        PTDF_new = update_PTDF_after_lines_removal(non_invested_branches) ;
        HVDC_sensitivity_matrix_new = update_HVDC_sensi_after_lines_removal(PTDF_new) ;

        double threshold = 1e-5 ;

        PTDF_new.mat  = (PTDF_new.mat.array().abs() >= threshold).select(PTDF_new.mat,0.) ;
        HVDC_sensitivity_matrix_new.mat = (HVDC_sensitivity_matrix_new.mat.array().abs()>=threshold).select(HVDC_sensitivity_matrix_new.mat,0.) ;
    }


    template<typename RowKey, typename ColKey>
    std::map<std::pair<ColKey,RowKey>,double>
    helper_convert_sensitivity_array_to_dict(NamedMatrix<Eigen::MatrixXd,RowKey,ColKey>& named_matrix)
    {
        auto row_labels_view = std::views::keys(named_matrix.row_index) ;
        std::vector<RowKey> row_labels(row_labels_view.begin(),row_labels_view.end()) ;
        auto col_labels_view = std::views::keys(named_matrix.col_index) ;
        std::vector<ColKey> col_labels(col_labels_view.begin(),col_labels_view.end()) ;
        std::map<std::pair<ColKey,RowKey>,double> sensi_dict ;
        for (int i=0; i<row_labels.size(); i++)
        {
            for (int j=0; j<col_labels.size(); j++)
            {
                sensi_dict[std::make_pair(col_labels[j],row_labels[i] )] = named_matrix.mat(i,j) ;
            }
        }
        return sensi_dict ;
    }



    void compute_factors_for_micro_iterations(std::map<std::string,int> z_dict)
    {
        std::vector<std::string> invested_branches, non_invested_branches ;
        get_invested_branches(z_dict,invested_branches,non_invested_branches) ;

        // Get unique monitored branches from max_flows_N and max_flows_N_K (first element of pair keys)
        std::set<std::string> monitored_set ;
        for (const auto& [key, val] : max_flows_N_) {
            monitored_set.insert(key.first) ;
        }
        for (const auto& [key, val] : max_flows_N_K_) {
            monitored_set.insert(key.first) ;
        }

        // Remove non-invested branches
        for (const auto& br : non_invested_branches) {
            monitored_set.erase(br) ;
        }
        all_monitored_branches_.clear() ; 
        all_monitored_branches_.insert(all_monitored_branches_.end(), monitored_set.begin(), monitored_set.end()) ;

        // Get unique branches involved in incidents
        std::set<std::string> branches_with_incidents_set ;
        for (const auto& [incident, branches] : dict_incident_outage_AC_branches_) {
            for (const auto& br : branches) {
                branches_with_incidents_set.insert(br) ;
            }
        }
        std::vector<std::string> branches_with_incidents(branches_with_incidents_set.begin(), branches_with_incidents_set.end()) ;

        NamedMatrix<Eigen::MatrixXd,std::string,int> PTDF_new ;
        NamedMatrix<Eigen::MatrixXd,std::string,std::string> HVDC_sensitivity_matrix_new ;

        compute_sensis_after_lines_removal(non_invested_branches,PTDF_new,HVDC_sensitivity_matrix_new) ;

        // Union of all_monitored_branches and branches_with_incidents, deduplicated
        std::set<std::string> branches_to_keep_set(all_monitored_branches_.begin(), all_monitored_branches_.end()) ;
        branches_to_keep_set.insert(branches_with_incidents.begin(), branches_with_incidents.end()) ;
        std::vector<std::string> branches_to_keep_in_sensi_dicts(branches_to_keep_set.begin(), branches_to_keep_set.end()) ;

        // Filter HVDC rows to intersect(HVDC row names, branches_to_keep)
        NamedMatrix<Eigen::MatrixXd,std::string,std::string> HVDC_filtered ;
        std::vector<std::string> filtered_branches ;
        for (const auto& [name, idx] : HVDC_sensitivity_matrix_new.row_index) {
            if (branches_to_keep_set.count(name)) filtered_branches.push_back(name) ;
        }
        HVDC_filtered.mat.resize(filtered_branches.size(), HVDC_sensitivity_matrix_new.mat.cols()) ;
        for (int i = 0; i < filtered_branches.size(); ++i) {
            int old_row = HVDC_sensitivity_matrix_new.row_index.at(filtered_branches[i]) ;
            HVDC_filtered.mat.row(i) = HVDC_sensitivity_matrix_new.mat.row(old_row) ;
            HVDC_filtered.row_index[filtered_branches[i]] = i ;
        }
        HVDC_filtered.col_index = HVDC_sensitivity_matrix_new.col_index ;

        // Convert HVDC matrix to dict
        HVDC_new_dict_ = helper_convert_sensitivity_array_to_dict(HVDC_filtered) ;

        // Compute incident factors
        dict_incident_factors_ = compute_incident_factors_after_lines_removal(all_monitored_branches_, invested_branches, PTDF_new) ;  
    }



    void broadcast_factors() 
    {
        mpi::broadcast(*world_,dict_incident_factors_,0) ; 
        mpi::broadcast(*world_,HVDC_new_dict_,0) ; 
        mpi::broadcast(*world_,all_monitored_branches_,0) ;
    }


    std::map<std::string, double>
    get_overflows_N(
        const std::map<std::pair<std::string,std::string>, double>& max_flows_N,
        const std::vector<std::string>& all_monitored_branches,
        const std::string& v,
        const std::map<std::string, double>& F_N_values,
        double tol_N)
    {
        std::map<std::string, double> dict_results_overflow_N ;

        for (const auto& monitored : all_monitored_branches) {
            double flow = std::abs(F_N_values.at(monitored)) ;
            double max_flow = std::abs(max_flows_N.at({monitored, v})) ;
            if (flow > max_flow * tol_N) {
                dict_results_overflow_N[monitored] = flow - max_flow ;
            }
        }

        return dict_results_overflow_N ;
    }


    std::map<std::pair<std::string,std::string>, double>
    get_overflows_N_K(
        const std::map<std::pair<std::string,std::string>, double>& max_flows_N_K,
        const std::vector<std::string>& all_monitored_branches,
        const std::map<std::string, std::vector<std::string>>& dict_incident_outage_AC_branches,
        const std::map<std::string, std::vector<std::string>>& dict_incident_HVDC_branches,
        const std::map<std::tuple<std::string,std::string,std::string>, double>& dict_incident_factors,
        const std::map<std::pair<std::string,std::string>, double>& HVDC_new_dict,
        const std::string& v,
        const std::map<std::string, double>& F_N_values,
        double tol_N_K)
    {
        std::map<std::pair<std::string,std::string>, double> dict_results_overflow_N_K ;

        // Compute N-K overflows: iterate over each incident
        for (const auto& [incident, outage_AC_branches] : dict_incident_outage_AC_branches) {

            const auto& outage_hvdcs = dict_incident_HVDC_branches.at(incident) ;

            // Compute N-K overflows for monitored lines + candidates invested in
            for (const auto& monitored : all_monitored_branches) {
                // Skip if monitored branch is in outage
                if (std::find(outage_AC_branches.begin(), outage_AC_branches.end(), monitored)
                        != outage_AC_branches.end()) continue ;

                double F_N_K_loadflow = F_N_values.at(monitored) ;

                // + sum(F_N(outage_AC) * incident_factor(monitored, incident, outage_AC))
                for (const auto& outage_AC_branch : outage_AC_branches) {
                    auto it = dict_incident_factors.find({monitored, incident, outage_AC_branch}) ;
                    if (it != dict_incident_factors.end()) {
                        F_N_K_loadflow += F_N_values.at(outage_AC_branch) * it->second ;
                    }
                }

                // - sum(F_N(outage_hvdc) * HVDC_sensi(outage_hvdc, monitored))
                for (const auto& outage_hvdc : outage_hvdcs) {
                    auto it = HVDC_new_dict.find({outage_hvdc, monitored}) ;
                    if (it != HVDC_new_dict.end()) {
                        F_N_K_loadflow -= F_N_values.at(outage_hvdc) * it->second ;
                    }
                }

                double max_flow = std::abs(max_flows_N_K.at({monitored, v})) ;
                if (std::abs(F_N_K_loadflow) > max_flow * tol_N_K) {
                    dict_results_overflow_N_K[{monitored, incident}] = std::abs(F_N_K_loadflow) - max_flow ;
                }
            }
        }

        return dict_results_overflow_N_K ;
    }


    std::vector<std::string>
    sort_results_and_return_constraints(
        const std::map<std::string, double>& dict_results_overflow_N,
        const std::map<std::pair<std::string,std::string>, double>& dict_results_overflow_N_K,
        int max_constraints_per_micro_it,
        bool add_N_constraint_first)
    {
        std::vector<std::string> constraints_to_add ;
        std::vector<std::string> N_constraints_micro_it ;
        std::vector<std::string> N_K_constraints_micro_it ;

        // Sort N-K overflows by value descending
        std::vector<std::pair<std::pair<std::string,std::string>, double>> sorted_results_overflow_N_K(
            dict_results_overflow_N_K.begin(), dict_results_overflow_N_K.end()) ;
        std::sort(sorted_results_overflow_N_K.begin(), sorted_results_overflow_N_K.end(),
            [](const auto& a, const auto& b) { return a.second > b.second ; }) ;

        // Add N constraints
        for (const auto& [monitored, val] : dict_results_overflow_N) {
            if ((int)(N_constraints_micro_it.size() + N_K_constraints_micro_it.size()) >= max_constraints_per_micro_it)
                break ;
            constraints_to_add.push_back("branch_" + monitored) ;
            N_constraints_micro_it.push_back(monitored) ;
        }

        // Add N-K constraints
        for (const auto& [key, val] : sorted_results_overflow_N_K) {
            if ((int)(N_constraints_micro_it.size() + N_K_constraints_micro_it.size()) >= max_constraints_per_micro_it)
                break ;
            const auto& monitored = key.first ;
            const auto& incident = key.second ;

            bool already_in_N_K = std::find(N_K_constraints_micro_it.begin(), N_K_constraints_micro_it.end(), monitored)
                                    != N_K_constraints_micro_it.end() ;
            if (!already_in_N_K) {
                bool in_N = std::find(N_constraints_micro_it.begin(), N_constraints_micro_it.end(), monitored)
                                != N_constraints_micro_it.end() ;
                if (!(add_N_constraint_first && in_N)) {
                    constraints_to_add.push_back("branch_" + monitored + "_inc_" + incident) ;
                    constraints_to_add.push_back("inc_" + incident) ;
                    N_K_constraints_micro_it.push_back(monitored) ;
                }
            }
        }

        return constraints_to_add ;
    }



    //to do here 
    std::vector<std::string>
    return_constraints_for_micro_iteration(
        const std::string& sub_problem ,
        const std::map<std::string, double>& F_N_values,
        double tol_N=1e-5,
        double tol_N_K=1e-5,
        int max_constraints_per_micro_it = 200,
        bool add_N_constraint_first=false)
    {
        auto dict_results_overflow_N = get_overflows_N(max_flows_N_, all_monitored_branches_, sub_problem, F_N_values, tol_N) ;

        auto dict_results_overflow_N_K = get_overflows_N_K(max_flows_N_K_, all_monitored_branches_,
            dict_incident_outage_AC_branches_, dict_incident_HVDC_branches_,
            dict_incident_factors_, HVDC_new_dict_, sub_problem, F_N_values, tol_N_K) ;

        return sort_results_and_return_constraints(dict_results_overflow_N, dict_results_overflow_N_K,
            max_constraints_per_micro_it, add_N_constraint_first) ;
    }


    const std::map<std::pair<std::string,std::string>, double>& get_max_flows_N() const { return max_flows_N_ ; }
    const std::map<std::pair<std::string,std::string>, double>& get_max_flows_N_K() const { return max_flows_N_K_ ; }
    const std::map<std::string, std::vector<std::string>>& get_dict_incident_outage_AC_branches() const { return dict_incident_outage_AC_branches_ ; }
    const std::map<std::string, std::vector<std::string>>& get_dict_incident_HVDC_branches() const { return dict_incident_HVDC_branches_ ; }

    private :
    mpi::communicator* world_ ; 
    std::string data_path_ ;
    NamedMatrix<Eigen::MatrixXd,int,int> B_inv_ ;
    NamedMatrix<Eigen::SparseMatrix<double>,std::string,std::string> Ab_ ;
    NamedMatrix<Eigen::SparseMatrix<double>,std::string,std::string> Yl_ ;
    NamedMatrix<Eigen::SparseMatrix<double>,std::string,std::string> A_hvdc_ ;
    std::map<std::string,std::string> branches_dict_ ;
    std::map<std::string,int> n_side1_dict_ ;
    std::map<std::string,int> n_side2_dict_ ;
    std::map<std::pair<std::string,std::string>,double> max_flows_N_K_ ;
    std::map<std::pair<std::string,std::string>,double> max_flows_N_ ;
    std::map<std::string, std::vector<std::string>> dict_incident_outage_AC_branches_ ;
    std::map<std::string, std::vector<std::string>> dict_incident_HVDC_branches_ ;
    std::vector<std::string> all_monitored_branches_ ; 
    std::map<std::pair<std::string,std::string>,double> HVDC_new_dict_  ; 
    std::map<std::tuple<std::string,std::string,std::string>, double> dict_incident_factors_  ;  
} ;
