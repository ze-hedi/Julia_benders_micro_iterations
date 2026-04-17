module MyLib

    using Serialization
    using CSV
    using NamedArrays
    using SparseArrays
    using DataFrames
    using LinearAlgebra
    
    
    struct CandidateLineInvestmentStatus
        candidate_line_id::Cstring
        is_invested::Cint
    end

    struct CandidateLineInvestmentStatusList 
        candidates_res::Ptr{CandidateLineInvestmentStatus}
        size::Cint
    end

    struct SubProblemsIds
        subProblems_ids::Ptr{Cstring} 
        n_subproblems::Cint
    end 

    struct FlowN 
        line_id::Cstring
        value::Cdouble
    end

    struct FlowNList 
        flows::Ptr{FlowN} 
        size::Cint
    end

    struct ViolatedFlowConstraints 
        constraints::Ptr{Cstring} 
        size::Cint
    end

    struct SerializedObject 
        bytes_ptr::Ptr{UInt8} 
        bytes_length::Cint
    end 

    struct SerializedFactors 
        HVDC_dict_serialized::SerializedObject 
        dict_incident_factors_serialized::SerializedObject 
        all_monitored_branches_serialized::SerializedObject
    end 

    
    function build_dict_from_master_iter_result(input::CandidateLineInvestmentStatusList) 
        
        dict = Dict{String, Int}()
        sizehint!(dict, input.size)
    
        candidates = unsafe_wrap(Array, input.candidates_res, input.size)

   
    
        for candidate in candidates
            line_id = unsafe_string(candidate.candidate_line_id)
            dict[line_id] = candidate.is_invested
        end
        
        return dict
        
    end 



    function serialize_obj(obj_2_serialize) 

        buf = IOBuffer() 
        serialize(buf,obj_2_serialize) 

        bytes = take!(buf)
        n = length(bytes)
        ptr = pointer(bytes)

        return ptr, n 
    end 


    function deserialize_obj(bytes_ptr, bytes_len)
        
        bytes_deserialize = unsafe_wrap(Array,bytes_ptr,bytes_len,own=false) 
        buf_deserialize = IOBuffer(bytes_deserialize)
        deserialized_obj = deserialize(buf_deserialize)
        
        return deserialized_obj 
    end 


    



    Base.@ccallable function jl_load_variables(subproblems_ids::SubProblemsIds,rank::Cint)::Cvoid


        path_input_julia = "./plugin_inputs/inputs_julia"
        global vars_dict = Dict{String, Any}() 
        

        dict_subproblems = Dict{String,Vector{String}}() 
        sizehint!(dict_subproblems,subproblems_ids.n_subproblems)

    
        for i in 1:subproblems_ids.n_subproblems 
            key = unsafe_string(unsafe_load(subproblems_ids.subProblems_ids,i))
            dict_subproblems[key] = String[]
        end
        vars_dict["inc_by_sub"]  = dict_subproblems

        if (rank==0)
            
            B_inv = deserialize("$(path_input_julia)/B_inv.jls")
            vars_dict["B_inv"] = B_inv

            Ab = deserialize("$(path_input_julia)/Ab.jls")
            vars_dict["Ab"] = Ab 
       
            Yl = deserialize("$(path_input_julia)/Yl.jls")
            vars_dict["Yl"] = Yl 

            
            A_hvdc = deserialize("$(path_input_julia)/A_hvdc.jls")
            vars_dict["A_hvdc"] = A_hvdc


            branches_to_candidates_dict = deserialize("$(path_input_julia)/branches_to_candidates_dict.jls")
            vars_dict["branches_to_candidates_dict"]  = branches_to_candidates_dict


            n_side1_dict = deserialize("$(path_input_julia)/n_side1_dict.jls")
            vars_dict["n_side1_dict"] = n_side1_dict    


            n_side2_dict = deserialize("$(path_input_julia)/n_side2_dict.jls")
            vars_dict["n_side2_dict"] = n_side2_dict

            
        end 

        
        
       
        dict_incident_outage_AC_branches = deserialize("$(path_input_julia)/dict_incident_outage_AC_branches.jls")
        vars_dict["dict_incident_outage_AC_branches"] = dict_incident_outage_AC_branches

        dict_incident_HVDC_branches = deserialize("$(path_input_julia)/dict_incident_HVDC_branches.jls")
        vars_dict["dict_incident_HVDC_branches"] = dict_incident_HVDC_branches

       
        max_flows_N = deserialize("$(path_input_julia)/max_flows_N.jls")
        vars_dict["max_flows_N"] = max_flows_N
     
        max_flows_N_K = deserialize("$(path_input_julia)/max_flows_N_K.jls")
        vars_dict["max_flows_N_K"] = max_flows_N_K
     

        vars_dict["HVDC_new_dict"] = nothing 
        vars_dict["dict_incident_factors"] = nothing 
        vars_dict["all_monitored_branches"] = nothing 


        return nothing 

    end

####################################################################################################################################
#################################### START CODE NEEDED AT BENDERS ITERATION ######################################################## 
####################################################################################################################################

    function csv_to_Dict(path)
        
        df = CSV.read(path, DataFrame, header = false,stringtype=String)
        @assert ncol(df) ≥ 2 "Le CSV doit contenir au moins deux colonnes"
        
        return Dict(df[!, 1] .=> df[!, 2])
    end
    

    function get_invested_branches(z_dict::Dict)

        branches_invested = collect(k for (k,v) in vars_dict["branches_to_candidates_dict"] if z_dict[v]== 1)   
        branches_not_invested = collect(k for (k,v) in vars_dict["branches_to_candidates_dict"] if z_dict[v]== 0)

        return branches_invested, branches_not_invested
    end


    function update_HVDC_sensi_after_lines_removal(PTDF_new)
        
        HVDC_sensitivity_matrix_new = PTDF_new * vars_dict["A_hvdc"] 

        return HVDC_sensitivity_matrix_new
    end

    function compute_sensis_after_lines_removal( branches_not_invested)

        # Compute matrix (PTDF and HVDC sensis)
        PTDF_new = update_PTDF_after_lines_removal( branches_not_invested)
        HVDC_sensi_new = update_HVDC_sensi_after_lines_removal(PTDF_new)

        # Pruning 
        threshold_ptdf = 1e-5
        PTDF_new[abs.(PTDF_new) .< threshold_ptdf] .= 0.

        threshold_hvdc_sensi = 1e-5
        HVDC_sensi_new[abs.(HVDC_sensi_new) .< threshold_hvdc_sensi] .= 0.0
    
        return PTDF_new, HVDC_sensi_new
    end

    function update_PTDF_after_lines_removal(branches_not_invested)

        k = length(branches_not_invested) # number of deleted lines on the network 
        n = size(vars_dict["B_inv"], 1) # number of nodes without slack nodes


        # Construire A_k = [√b_l * a_l_sans_slack]
        A_k = zeros(n, k)
        for (j, br) in enumerate(branches_not_invested)
            # vecteur d'incidence complet
            a = vars_dict["Ab"][br, :] |> collect
            # suppression du slack
            y_l = vars_dict["Yl"][br, br]
            A_k[:, j] = sqrt(y_l) * a
        end

        # Calcul de la matrice intermédiaire M qu'il faut ensuite inverser
        M = I - A_k' * (vars_dict["B_inv"] * A_k)
        M_inv = inv(Matrix(M))  # si îlot possible : utiliser pinv(M)

        # Mise à jour de B_invdf_3
        BinvA = vars_dict["B_inv"] * A_k
        Bnew_inv = vars_dict["B_inv"] + BinvA * M_inv * (BinvA')

        # Mettre à jour Ab et Yl (suppression des lignes retirées)
        Ab_new = vars_dict["Ab"][setdiff(names(vars_dict["Ab"], 1), branches_not_invested), :]
        Yl_new = vars_dict["Yl"][setdiff(names(vars_dict["Yl"], 1), branches_not_invested), setdiff(names(vars_dict["Yl"], 2), branches_not_invested)]

        # Nouvelle PTDF
        PTDF_new = Yl_new * Ab_new * Bnew_inv

        return PTDF_new
    end


    function helper_convert_sensitivity_array_to_dict(named_array::NamedArray{Float64, 2})

        row_labels = names(named_array, 1)
        col_labels = names(named_array, 2)
        sensi_dict = Dict{Tuple{eltype(col_labels), eltype(row_labels)}, eltype(named_array)}(
            (col_labels[j], row_labels[i]) => named_array[i, j]
            for i in eachindex(row_labels), j in eachindex(col_labels)
        )

        return sensi_dict
    end


    function compute_incident_factors_after_lines_removal(all_monitored_branches, branches_invested, PTDF_new)
        
        ## PRE TREATMENTS ##

        #Initialise a dictionnary which will contain all the FPM matrices
        #It will contain NamedArrays, or nothing if the incident contains a branch breaking synchronicity
        dict_incident_factors = Dict{Tuple{String,String,String}, Float64}()

        PTDF_matrix = Array(PTDF_new)

        PTDF_new_dict = helper_convert_sensitivity_array_to_dict(PTDF_new[intersect(names(PTDF_new, 1), all_monitored_branches), :])

        ## MAIN LOOP ##

        #Iterate over all incidents to compute the incident factors

        for incident_id in keys(vars_dict["dict_incident_outage_AC_branches"])

            #Get the involved AC branches
            outage_AC_branches = vars_dict["dict_incident_outage_AC_branches"][incident_id]

            #Get the origin (side 1) and extremity (side 2) nodes of the AC outage branches
            side1_nodes = [vars_dict["n_side1_dict"][branch] for branch in outage_AC_branches]
            side2_nodes = [vars_dict["n_side2_dict"][branch] for branch in outage_AC_branches]
            
            #Extract the part of the PTDF corresponding to the side 1 nodes and outaged branches
            side1_nodes_indexes = map(node -> findfirst(isequal(node), names(PTDF_new, 2)), side1_nodes)
            outage_AC_branches_indexes = map(branch -> findfirst(isequal(branch), names(PTDF_new, 1)), outage_AC_branches)
            PTDF_side1_nodes_outage_lines = PTDF_matrix[outage_AC_branches_indexes, side1_nodes_indexes]
            
            #Extract the part of the PTDF corresponding to the side 2 nodes and outaged branches
            side2_nodes_indexes = map(node -> findfirst(isequal(node), names(PTDF_new, 2)), side2_nodes)
            PTDF_side2_nodes_outage_lines = PTDF_matrix[outage_AC_branches_indexes, side2_nodes_indexes]

            
            #Create corresponding identity matrix (used below to compute the FPM)
            nb_outage_AC_branches = length(outage_AC_branches)
            I_matrix = Matrix{Float64}(I, nb_outage_AC_branches, nb_outage_AC_branches)
            
            #Compute the FPM (fictitious powers matrix), such that fictitious_powers_vector = FPM * flow_outage_lines

            #We first compute FPM^-1 , which is the matrix such that flow_outage_lines = FPM^-1 * fictitious_powers_vector
            FPM_inv_matrix = I_matrix - (Matrix(PTDF_side1_nodes_outage_lines) - Matrix(PTDF_side2_nodes_outage_lines))
            FPM_matrix = inv(FPM_inv_matrix)
            FPM = NamedArray(FPM_matrix, (outage_AC_branches, outage_AC_branches), ("branch_fictitious_power","outage_branch_id"))

            for monitored_line in all_monitored_branches, outage_line in outage_AC_branches
                incident_factor = 0

                for outage_line_bis in outage_AC_branches
                    node1_outage = vars_dict["n_side1_dict"][outage_line_bis]
                    node2_outage = vars_dict["n_side2_dict"][outage_line_bis]

                    PTDF_node1 = PTDF_new_dict[node1_outage,monitored_line]
                    PTDF_node2 = PTDF_new_dict[node2_outage,monitored_line]

                    incident_factor += FPM[outage_line_bis,outage_line]*(PTDF_node1 - PTDF_node2)
                end

                dict_incident_factors[monitored_line,incident_id,outage_line] = incident_factor
            end
        end

        # Data filtering 
        threshold_factors = 1e-5

        dict_incident_factors = Dict(
            k => (abs(v) >= threshold_factors ? v : 0)
            for (k, v) in dict_incident_factors
        )

        
        #Do not return anything (in place modification)
        return dict_incident_factors

    end


    Base.@ccallable function jl_call_GC()::Cvoid
        GC.gc(true)
    end 

    Base.@ccallable function jl_compute_factors_for_microiterations(candidates::CandidateLineInvestmentStatusList,num_iter::Cint)::SerializedFactors 

        t1 = time_ns()
        z_dict = build_dict_from_master_iter_result(candidates)
        branches_invested, branches_not_invested = get_invested_branches(z_dict) 

        # Get vector of monitored lines and incidents lines
        monitored_N = unique(first.(keys(vars_dict["max_flows_N"])))
        monitored_N_K = unique(first.(keys(vars_dict["max_flows_N_K"])))
        all_monitored_branches = setdiff(monitored_N ∪ monitored_N_K,branches_not_invested)
       
        branches_with_incidents = unique(vcat(values(vars_dict["dict_incident_outage_AC_branches"])...))

        # Compute PTDF and HVDC sensi matrix
        PTDF_new , HVDC_new = compute_sensis_after_lines_removal( branches_not_invested)


        # Write HVDC matrix into a dictionnary
        branches_to_keep_in_sensi_dicts = collect(Set([all_monitored_branches; branches_with_incidents])) #to get rid of duplicates
        
        HVDC_new_dict = helper_convert_sensitivity_array_to_dict(HVDC_new[intersect(names(HVDC_new, 1), branches_to_keep_in_sensi_dicts), :])


        dict_incident_factors = compute_incident_factors_after_lines_removal(all_monitored_branches, branches_invested, PTDF_new)
        t2 = time_ns() 

        elapsed_microseconds = (t2 - t1) / 1000 

        vars_dict["HVDC_new_dict"] = HVDC_new_dict 
        HVDC_new_dict_serialized_ptr, HVDC_new_dict_bytes_len  = serialize_obj(HVDC_new_dict) 
        HVDC_new_dict_serialized_serialized_obj = SerializedObject(HVDC_new_dict_serialized_ptr,HVDC_new_dict_bytes_len)    

        vars_dict["dict_incident_factors"] = dict_incident_factors
        dict_incident_factors_serialized_ptr, dict_incident_factors_bytes_len  = serialize_obj(dict_incident_factors) 
        dict_incident_factors_serialized_obj = SerializedObject(dict_incident_factors_serialized_ptr,dict_incident_factors_bytes_len) 

        vars_dict["all_monitored_branches"] = all_monitored_branches 
        all_monitored_branches_serialized_ptr, all_monitored_branches_bytes_len  = serialize_obj(all_monitored_branches) 
        all_monitored_branches_serialized_obj = SerializedObject(all_monitored_branches_serialized_ptr,all_monitored_branches_bytes_len) 

        new_msg = "elapsed time to compute factors for microiterations $(elapsed_microseconds)"   
        
        vars_dict["serialized_factors"] = SerializedFactors(HVDC_new_dict_serialized_serialized_obj,dict_incident_factors_serialized_obj, all_monitored_branches_serialized_obj)

        return vars_dict["serialized_factors"]
    end 



    Base.@ccallable function jl_clean_buffers()::Cvoid 
        vars_dict["serialized_factors"] = nothing 
        
        return nothing 
    end 

    
    ####################################################################################################################################
    #################################### END CODE NEEDED AT BENDERS ITERATION ######################################################## 
    ####################################################################################################################################
    
    
    
    ####################################################################################################################################
    #################################### START CODE NEEDED AT MICRO ITERATION  ######################################################## 
    ####################################################################################################################################
    function get_overflows_N(v,F_N_values,N_constraints_added)

        #Initialize overflows dictionnaries
        dict_results_overflow_N = Dict{String, Float64}() #Branche monitorée

        # Compute N overflows
        for monitored in vars_dict["all_monitored_branches"] # on ne regarde pas les branches déjà rajouter en N précédemment

            overflow = max((abs(F_N_values[monitored])) - abs(vars_dict["max_flows_N"][monitored,v]),0)
            if overflow > 0 
                dict_results_overflow_N[monitored] = overflow
            end
        end
        return dict_results_overflow_N
    end


    function get_overflows_N_K( v, F_N_values)

        #Initialize overflows dictionnaries
        dict_results_overflow_N_K= Dict{Tuple{String, String}, Float64}() # (branche monitorée,incident)

        incidents_vector = keys(vars_dict["dict_incident_outage_AC_branches"])

        # Compute N-K overflows : iterative process on each incident
        for incident in incidents_vector

            ## Retrieve incident information # 
            outage_AC_branches = vars_dict["dict_incident_outage_AC_branches"][incident]
            outage_hvdcs       = vars_dict["dict_incident_HVDC_branches"][incident]

            # Compute N-K overflows for monitored lines + candidates invested in
            for monitored in vars_dict["all_monitored_branches"]
                if !(monitored in outage_AC_branches)
                  F_N_k_loadflow = F_N_values[monitored] + sum((F_N_values[outage_AC_branch] * vars_dict["dict_incident_factors"][monitored,incident,outage_AC_branch] for outage_AC_branch in outage_AC_branches);init=0.0) - sum((F_N_values[outage_hvdc] * vars_dict["HVDC_new_dict"][outage_hvdc, monitored] for outage_hvdc in outage_hvdcs); init= 0.0)
                    overflow = max((abs(F_N_k_loadflow) - abs(vars_dict["max_flows_N_K"][monitored, v])),0) 
                    if overflow > 0 
                        dict_results_overflow_N_K[monitored,incident] = overflow
                    end 
                end
            end
        end

        results_overflow_N_K = sort(collect(dict_results_overflow_N_K), by=x->x[2], rev=true)
        
        return results_overflow_N_K
    end

    
    function sort_results_and_return_constraints(dict_results_overflow_N,results_overflow_N_K) 
        
        constraints_to_add = [] # Vector which will be sent to C++
        N_constraints_micro_it = [] # Vector used to track N_constraints added during this micro-iteration
        N_K_constraints_micro_it = [] # Vector used to track N_K_constraints added during this micro-iteration

        
        for monitored in keys(dict_results_overflow_N)
            append!(constraints_to_add,["branch_$(monitored)"])
            append!(N_constraints_micro_it,[monitored])
        end

        for overflow in results_overflow_N_K
            monitored = overflow[1][1]  #overflow de la forme (monitored,incident) => value
            if !(monitored in N_K_constraints_micro_it) # on ajoute d'abord la contrainte en N et seulement un incident par lignes surveillées
                incident = overflow[1][2] #on ajoute le plus gros incident (max threat)
                append!(constraints_to_add,["branch_$(monitored)_inc_$(incident)"])
                append!(constraints_to_add,["inc_$(incident)"])
                append!(N_K_constraints_micro_it,[monitored])
            end
        end
        return constraints_to_add, N_constraints_micro_it,N_K_constraints_micro_it
    end

    Base.@ccallable function jl_deserialize_factors(serialized_factors::SerializedFactors)::Cvoid 

        vars_dict["HVDC_new_dict"] = deserialize_obj(serialized_factors.HVDC_dict_serialized.bytes_ptr,serialized_factors.HVDC_dict_serialized.bytes_length)
        

        vars_dict["dict_incident_factors"] = deserialize_obj(serialized_factors.dict_incident_factors_serialized.bytes_ptr, serialized_factors.dict_incident_factors_serialized.bytes_length)

    
        vars_dict["all_monitored_branches"] = deserialize_obj(serialized_factors.all_monitored_branches_serialized.bytes_ptr, serialized_factors.all_monitored_branches_serialized.bytes_length)        

        return nothing 
    end
    
    Base.@ccallable function jl_return_constraints_for_micro_iteration(subproblem_id::Ptr{UInt8},flow_list::FlowNList)::ViolatedFlowConstraints
    

        F_N_values = Dict(unsafe_string(unsafe_load(flow_list.flows, i).line_id) => unsafe_load(flow_list.flows, i).value 
            for i in 1:flow_list.size)

            
        dict_results_overflow_N = get_overflows_N(unsafe_string(subproblem_id),F_N_values,vars_dict["inc_by_sub"][unsafe_string(subproblem_id)])

        results_overflow_N_K = get_overflows_N_K(unsafe_string(subproblem_id), F_N_values)

        constraints_to_add, N_constraints_micro_it, N_K_constraints_micro_it = sort_results_and_return_constraints(dict_results_overflow_N,results_overflow_N_K)


        append!(vars_dict["inc_by_sub"][unsafe_string(subproblem_id)],N_constraints_micro_it)
        append!(vars_dict["inc_by_sub"][unsafe_string(subproblem_id)],N_K_constraints_micro_it)

        c_string_constraints = [pointer(constraint) for constraint in constraints_to_add] 
        constraints_ptr = pointer(c_string_constraints)
        return ViolatedFlowConstraints(constraints_ptr,Cint(length(constraints_to_add))) 
    end 

    
    
    ####################################################################################################################################
    #################################### END CODE NEEDED AT MICRO ITERATION  ######################################################## 
    ####################################################################################################################################
    

end  