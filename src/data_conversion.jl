using JSON, DelimitedFiles, SparseArrays
using Serialization
using NamedArrays


path_out = "./cpp_structures"
path_input_julia = "./inputs_julia"

mkpath(path_out)

# Dense matrix → binary (row-major double array + dimensions)
function write_dense_named(path, nm)
    m = Matrix(nm)  # underlying dense matrix
    open("$(path).bin", "w") do f
        write(f, Int64(size(m,1)))
        write(f, Int64(size(m,2)))
        write(f, m)  # column-major Float64 blob
    end
    # row/col names as JSON: {name: index, ...}
    open("$(path)_rownames.json", "w") do f
        println("dirnames : $(nm.dicts[1])")
        JSON.print(f, nm.dicts[1])
    end
    open("$(path)_colnames.json", "w") do f
        JSON.print(f, nm.dicts[2])
    end
end



# Sparse matrix → COO format (row, col, val triples + dimensions)
function write_sparse_named(path, nm)
    s = sparse(nm.array)
    I, J, V = findnz(s)
    open("$(path).coo", "w") do f
        write(f, Int64(size(s,1)))
        write(f, Int64(size(s,2)))
        write(f, Int64(length(V)))
        write(f, Vector{Int64}(I .- 1))  # 0-based for C++
        write(f, Vector{Int64}(J .- 1))
        write(f, Vector{Float64}(V))       # ensure Float64 for C++
    end
    open("$(path)_rownames.json", "w") do f
        JSON.print(f, nm.dicts[1])
    end
    open("$(path)_colnames.json", "w") do f
        JSON.print(f, nm.dicts[2])
    end
end

# Dicts → JSON
function write_dict(path, d)
    open("$(path).json", "w") do f
        JSON.print(f, d)
    end
end




B_inv = deserialize("$(path_input_julia)/B_inv.jls")
Ab = deserialize("$(path_input_julia)/Ab.jls")
Yl = deserialize("$(path_input_julia)/Yl.jls")
branches_to_candidates_dict = deserialize("$(path_input_julia)/branches_to_candidates_dict.jls")
A_hvdc = deserialize("$(path_input_julia)/A_hvdc.jls")
n_side1_dict = deserialize("$(path_input_julia)/n_side1_dict.jls")
n_side2_dict = deserialize("$(path_input_julia)/n_side2_dict.jls")

write_dense_named("$(path_out)/B_inv",B_inv)
write_sparse_named("$(path_out)/Ab", Ab)
write_sparse_named("$(path_out)/Yl", Yl)
write_sparse_named("$(path_out)/A_hvdc", A_hvdc)
write_dict("$(path_out)/branches_to_candidates_dict", branches_to_candidates_dict)
write_dict("$(path_out)/n_side1_dict", n_side1_dict)
write_dict("$(path_out)/n_side2_dict", n_side2_dict)

max_flows_N = deserialize("$(path_input_julia)/max_flows_N.jls")
write_dict("$(path_out)/max_flows_N", max_flows_N)

max_flows_N_K = deserialize("$(path_input_julia)/max_flows_N_K.jls")
write_dict("$(path_out)/max_flows_N_K", max_flows_N_K)

dict_incident_outage_AC_branches = deserialize("$(path_input_julia)/dict_incident_outage_AC_branches.jls")
write_dict("$(path_out)/dict_incident_outage_AC_branches", dict_incident_outage_AC_branches)

dict_incident_HVDC_branches = deserialize("$(path_input_julia)/dict_incident_HVDC_branches.jls")
write_dict("$(path_out)/dict_incident_HVDC_branches", dict_incident_HVDC_branches)

