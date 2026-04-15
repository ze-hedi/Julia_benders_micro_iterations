# Julia Benders Micro-Iterations Library

A high-performance C++/Julia hybrid library for implementing Benders decomposition with micro-iterations for power grid optimization problems. The library computes network flow constraints (N and N-K security criteria) during iterative optimization processes.

## Table of Contents
- [Overview](#overview)
- [Architecture](#architecture)
- [How It Works](#how-it-works)
- [Building the Library](#building-the-library)
- [Using libmyoutput.so](#using-libmyoutputso)
- [API Reference](#api-reference)
- [Example Usage](#example-usage)
- [Deployment](#deployment)

---

## Overview

This library solves large-scale power grid optimization problems using Benders decomposition with **micro-iterations**. The workflow involves:

1. **Benders Master Problem**: Determines candidate line investments (binary decisions)
2. **Micro-Iterations**: For each subproblem, iteratively adds violated flow constraints until feasibility
3. **Network Computations**: Uses Julia to compute:
   - PTDF matrices (Power Transfer Distribution Factors)
   - HVDC sensitivity matrices
   - Incident factors for N-K security analysis
   - Constraint violations based on flow solutions

### Why Julia + C++?

- **Julia**: High-performance matrix computations (PTDF, sensitivity analysis)
- **C++**: Integration with existing optimization solvers (e.g., CPLEX, Gurobi)
- **Hybrid approach**: Best of both worlds - Julia's speed for numerical computing, C++'s ecosystem for optimization

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    C++ Application Layer                     │
│                     (Benders Algorithm)                      │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ↓
┌─────────────────────────────────────────────────────────────┐
│                   libmyoutput.so (C++)                       │
│                  micro_iters.cpp/h                           │
│   • OnBendersStart()                                         │
│   • OnBendersMasterResolutionStart()                         │
│   • OnBendersMicroIterationEnd()                             │
│   • MPI communication for distributed solving                │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ↓
┌─────────────────────────────────────────────────────────────┐
│                    libmylib.so (Julia)                       │
│                   Compiled from MyLib.jl                     │
│   • jl_load_variables()                                      │
│   • jl_compute_factors_for_microiterations()                 │
│   • jl_return_constraints_for_micro_iteration()              │
│   • PTDF/HVDC/Incident factor computations                   │
└─────────────────────────────────────────────────────────────┘
```

### Component Breakdown

1. **src/MyLib.jl** - Julia module with core algorithms
   - Network matrix computations (PTDF, HVDC sensitivity)
   - Constraint violation detection
   - Serialization/deserialization for MPI communication

2. **micro_iters.cpp/h** - C++ wrapper layer
   - Interfaces with optimization solver
   - Manages Benders iteration lifecycle
   - Handles MPI communication between processes

3. **compile.jl / compile2.jl** - Build scripts
   - Compile Julia code to shared library (.so)
   - Uses PackageCompiler.jl

4. **package_library.sh** - Packaging script
   - Bundles all dependencies into self-contained package
   - Creates portable distribution

---

## How It Works

### Benders Decomposition Workflow

```
1. MASTER PROBLEM
   ├─→ Solve investment decisions (which lines to build)
   └─→ Output: z_dict (candidate_line_id → is_invested)
        │
        ↓
2. COMPUTE NETWORK FACTORS [Julia: jl_compute_factors_for_microiterations]
   ├─→ Update PTDF matrix after line removal
   ├─→ Compute HVDC sensitivity matrix
   └─→ Compute incident factors for N-K security
        │
        ↓ [Broadcast via MPI to all processes]
        │
3. SUBPROBLEMS (one per scenario/region)
   ├─→ Micro-iteration loop:
   │    ├─→ Solve subproblem with current constraints
   │    ├─→ Get flow solution (F_N values)
   │    ├─→ Check violations [Julia: jl_return_constraints_for_micro_iteration]
   │    │    ├─→ N violations (normal conditions)
   │    │    └─→ N-K violations (with incidents/outages)
   │    ├─→ Add violated constraints to model
   │    └─→ Repeat until no violations
   └─→ Return optimality cuts to master
```

### Key Algorithms (in MyLib.jl)

#### 1. PTDF Update After Line Removal
```julia
# When candidate lines are not invested:
# - Remove lines from network topology
# - Update admittance matrix inverse (B_inv)
# - Recompute PTDF using Sherman-Morrison-Woodbury formula
PTDF_new = update_PTDF_after_lines_removal(branches_not_invested)
```

#### 2. Incident Factor Computation
```julia
# For each incident (line outage):
# - Compute fictitious power matrix (FPM)
# - Calculate how monitored line flows change when incident occurs
# - Store as incident factors for fast constraint evaluation
dict_incident_factors[monitored_line, incident_id, outage_line] = factor
```

#### 3. Violation Detection
```julia
# N violations (normal conditions)
overflow_N = max(|F_N| - F_max_N, 0)

# N-K violations (with incident k)
F_NK = F_N + Σ(F_outage * incident_factor) - Σ(F_hvdc * hvdc_sensitivity)
overflow_NK = max(|F_NK| - F_max_NK, 0)
```

---

## Building the Library

### Prerequisites

```bash
# Install Julia 1.11+
wget https://julialang-s3.julialang.org/bin/linux/x64/1.11/julia-1.11.0-linux-x86_64.tar.gz
tar -xzf julia-1.11.0-linux-x86_64.tar.gz
export PATH="$PWD/julia-1.11.0/bin:$PATH"

# Install required packages
julia -e 'using Pkg; Pkg.add(["PackageCompiler", "CSV", "DataFrames", "NamedArrays", "SparseArrays", "MPI"])'

# Install C++ dependencies
sudo apt-get install g++ libboost-mpi-dev libboost-serialization-dev openmpi-bin libopenmpi-dev
```

### Build Process

#### Option 1: Quick Build (Parallel Compilation)
```bash
./build.sh
```

This script:
1. Cleans previous builds (`libmylib/`, `libmylib2/`, `libmyoutput_package/`)
2. Compiles Julia libraries in parallel (`compile.jl` & `compile2.jl`)
3. Packages everything into `libmyoutput_package/`

#### Option 2: Manual Step-by-Step Build

```bash
# Step 1: Compile Julia library
julia compile.jl
# Output: libmylib/ directory with libmylib.so

# Step 2: Build C++ wrapper library
g++ -shared -fPIC micro_iters.cpp \
    -I./libmylib/include \
    -I/usr/include/julia \
    -L./libmylib/lib \
    -lmylib -ljulia \
    -lboost_mpi -lboost_serialization -lmpi_cxx -lmpi \
    -Wl,-rpath,'$ORIGIN' \
    -o libmyoutput.so

# Step 3: Package for distribution
./package_library.sh
```

### Build Output

```
libmyoutput_package/
├── lib/
│   ├── libmyoutput.so          # Main C++ wrapper (your API)
│   ├── libmylib.so             # Compiled Julia library (~459 MB)
│   └── julia/                  # Julia runtime dependencies
│       ├── libjulia.so.1.11
│       ├── libjulia-internal.so.1.11
│       └── ... (support libraries)
├── include/
│   ├── MyLib.h                 # C API for Julia functions
│   └── julia_init.h            # Julia initialization
└── README.md                   # Usage instructions
```

---

## Using libmyoutput.so

### Integration in Your C++ Project

#### 1. Include Headers
```cpp
#include "micro_iters.h"      // Main API
#include "MyLib.h"            // Data structures
```

#### 2. Initialize Julia Runtime
```cpp
extern "C" {
    #include "julia_init.h"
}

int main() {
    // Initialize Julia (call once at startup)
    init_julia(0, NULL);
    
    // Your Benders algorithm here...
    
    // Shutdown Julia (call once at exit)
    shutdown_julia(0);
    return 0;
}
```

#### 3. Compilation

**Method A: Using LD_LIBRARY_PATH (Development)**
```bash
export LD_LIBRARY_PATH=/path/to/libmyoutput_package/lib:$LD_LIBRARY_PATH

g++ your_app.cpp \
    -I/path/to/libmyoutput_package/include \
    -L/path/to/libmyoutput_package/lib \
    -lmyoutput -lboost_mpi -lboost_serialization -lmpi_cxx -lmpi \
    -o your_app
```

**Method B: Using RPATH (Production)**
```bash
g++ your_app.cpp \
    -I/path/to/libmyoutput_package/include \
    -L/path/to/libmyoutput_package/lib \
    -lmyoutput -lboost_mpi -lboost_serialization -lmpi_cxx -lmpi \
    -Wl,-rpath,/path/to/libmyoutput_package/lib \
    -o your_app
```

**Method C: Portable Deployment**
```bash
# Directory structure:
# project/
# ├── bin/
# │   └── your_app
# └── lib/  (copy libmyoutput_package/lib/* here)

g++ your_app.cpp \
    -I/path/to/libmyoutput_package/include \
    -L/path/to/libmyoutput_package/lib \
    -lmyoutput -lboost_mpi -lboost_serialization -lmpi_cxx -lmpi \
    -Wl,-rpath,'$ORIGIN/../lib' \
    -o project/bin/your_app
```

---

## API Reference

### Data Structures

#### SubProblemsIds
```cpp
struct SubProblemsIds {
    char** subProblems_ids;    // Array of subproblem names
    int n_subproblems;         // Number of subproblems
};
```

#### CandidateLineInvestmentStatus
```cpp
struct CandidateLineInvestmentStatus {
    const char* candidate_line_id;  // Line identifier
    int is_invested;                // 0 or 1
};

struct CandidateLineInvestmentStatusList {
    CandidateLineInvestmentStatus* candidates_res;
    int size;
};
```

#### FlowN
```cpp
struct FlowN {
    const char* flow_id;       // Line identifier
    double value;              // Flow value in MW
};

struct FlowNList {
    FlowN* flows;
    int size;
};
```

#### ViolatedFlowConstraints
```cpp
struct ViolatedFlowConstraints {
    const char** constraints;  // Array of constraint names
    int size;                  // Number of violated constraints
};
```

### Main API Functions

#### 1. OnBendersStart
```cpp
void OnBendersStart(SubProblemsIds sub_problem_ids, 
                    int rank,
                    std::filesystem::path& input_root);
```
**Purpose**: Initialize Benders algorithm  
**When to call**: Once at the beginning, before first iteration  
**Parameters**:
- `sub_problem_ids`: List of subproblem identifiers
- `rank`: MPI rank (0 for master, 1+ for workers)
- `input_root`: Directory containing input files (constraints_dictionary.csv, variables_dictionary.csv)

**What it does**:
- Initializes Julia runtime (`init_julia`)
- Loads network data (PTDF matrices, line limits, incident definitions) via `jl_load_variables`
- Reads constraint and variable dictionaries from CSV files

---

#### 2. OnBendersMasterResolutionStart
```cpp
void OnBendersMasterResolutionStart(
    std::map<std::string, double>& master_out,
    int& num_iter,
    mpi::communicator* world,
    std::map<std::string, std::vector<std::string>>& added_constraints_per_sub,
    std::map<std::string, std::string>& binary_variables_ids_map);
```
**Purpose**: Process master problem solution and compute network factors  
**When to call**: After solving master problem, before solving subproblems  
**Parameters**:
- `master_out`: Investment decisions (line_id → investment_value)
- `num_iter`: Current Benders iteration number
- `world`: MPI communicator for parallel execution
- `added_constraints_per_sub`: Tracking of constraints added per subproblem (reset each iteration)
- `binary_variables_ids_map`: Maps solver variable names to CSV line IDs

**What it does**:
- Calls `jl_compute_factors_for_microiterations` (rank 0 only)
  - Computes updated PTDF matrix (removing non-invested lines)
  - Computes HVDC sensitivity matrix
  - Computes incident factors for N-K constraints
- Serializes results and broadcasts to all MPI processes
- Deserializes on worker processes via `jl_deserialize_factors`

---

#### 3. OnBendersMicroIterationEnd
```cpp
void OnBendersMicroIterationEnd(
    std::string sub_name,
    bool& added_rows,
    std::string solving_time,
    std::vector<double> sub_solution,
    std::vector<int>& variables_indices_vector,
    std::vector<std::string>& variables_names_vector,
    std::filesystem::path input_root,
    std::vector<std::string>& constraints_to_add_vec);
```
**Purpose**: Check for constraint violations after solving subproblem  
**When to call**: After each subproblem solve (inside micro-iteration loop)  
**Parameters**:
- `sub_name`: Subproblem identifier
- `added_rows`: OUTPUT - set to true if constraints were added
- `solving_time`: Solver timing information (for logging)
- `sub_solution`: Full solution vector from solver
- `variables_indices_vector`: Indices of flow variables in solution
- `variables_names_vector`: Names of flow variables (from solver)
- `input_root`: Path to constraint dictionary
- `constraints_to_add_vec`: OUTPUT - constraint names to add to model

**What it does**:
- Extracts flow values from solution vector
- Calls `jl_return_constraints_for_micro_iteration`
  - Checks N violations (normal operating conditions)
  - Checks N-K violations (with contingencies/incidents)
- Returns list of constraint families to add (e.g., "branch_LINE123_inc_INC456")
- Expands constraint families to actual MPS/LP constraint names using dictionary
- Tracks which constraint families have been added (avoids duplicates)

---

#### 4. OnBendersIterationEnd
```cpp
void OnBendersIterationEnd();
```
**Purpose**: Cleanup after Benders iteration  
**When to call**: After all subproblems solved, before next master solve  
**What it does**: Calls `jl_call_GC()` to trigger Julia garbage collection

---

#### 5. OnBendersEnd
```cpp
void OnBendersEnd();
```
**Purpose**: Shutdown library  
**When to call**: Once at program exit  
**What it does**: Calls `shutdown_julia(0)` to cleanly exit Julia runtime

---

### Lower-Level Julia C API (called internally)

These functions are defined in `MyLib.h` and called by the wrapper:

```cpp
// Load network data from disk
void jl_load_variables(SubProblemsIds, int rank);

// Compute PTDF/HVDC/incident factors for this iteration
SerializedFactors jl_compute_factors_for_microiterations(
    CandidateLineInvestmentStatusList, int iter);

// Deserialize factors (on worker MPI processes)
void jl_deserialize_factors(SerializedFactors);

// Check for violated flow constraints
ViolatedFlowConstraints jl_return_constraints_for_micro_iteration(
    const char* subproblem_id, FlowNList flows);

// Clean serialization buffers
void jl_clean_buffers();

// Trigger garbage collection
void jl_call_GC();
```

---

## Example Usage

### Complete Benders Algorithm Integration

```cpp
#include "micro_iters.h"
#include <boost/mpi.hpp>

namespace mpi = boost::mpi;

int main(int argc, char** argv) {
    // Initialize MPI
    mpi::environment env(argc, argv);
    mpi::communicator world;
    
    // === 1. INITIALIZATION ===
    std::filesystem::path input_root = "./inputs";
    
    // Define subproblems
    char* sub_ids[] = {(char*)"scenario_1", (char*)"scenario_2", (char*)"scenario_3"};
    SubProblemsIds subproblems;
    subproblems.subProblems_ids = sub_ids;
    subproblems.n_subproblems = 3;
    
    OnBendersStart(subproblems, world.rank(), input_root);
    
    // === 2. BENDERS LOOP ===
    for (int iter = 1; iter <= max_iterations; iter++) {
        
        // === 2a. SOLVE MASTER PROBLEM ===
        std::map<std::string, double> master_solution;
        // ... solve master problem with CPLEX/Gurobi ...
        // master_solution["candidate_line_1"] = 1.0;
        // master_solution["candidate_line_2"] = 0.0;
        
        std::map<std::string, std::string> binary_var_map;
        // binary_var_map["x_invest[candidate_1]"] = "candidate_line_1";
        
        std::map<std::string, std::vector<std::string>> added_constraints_per_sub;
        
        OnBendersMasterResolutionStart(
            master_solution, 
            iter, 
            &world,
            added_constraints_per_sub,
            binary_var_map);
        
        // === 2b. SOLVE SUBPROBLEMS ===
        for (int sub_idx = 0; sub_idx < subproblems.n_subproblems; sub_idx++) {
            std::string sub_name = subproblems.subProblems_ids[sub_idx];
            
            // MICRO-ITERATION LOOP
            bool added_rows = true;
            int micro_iter = 0;
            
            while (added_rows && micro_iter < max_micro_iterations) {
                micro_iter++;
                added_rows = false;
                
                // Solve subproblem
                // ... call CPLEX/Gurobi solver ...
                std::vector<double> solution = /* solver.getSolution() */;
                std::vector<int> flow_var_indices = /* indices of flow variables */;
                std::vector<std::string> flow_var_names = /* names of flow variables */;
                
                // Check for violations
                std::vector<std::string> constraints_to_add;
                OnBendersMicroIterationEnd(
                    sub_name,
                    added_rows,
                    "0.5s",  // solver time
                    solution,
                    flow_var_indices,
                    flow_var_names,
                    input_root,
                    constraints_to_add);
                
                if (added_rows) {
                    // Add constraints to model
                    for (const auto& constraint_name : constraints_to_add) {
                        // ... model.addConstraint(constraint_name) ...
                    }
                }
            }
            
            // Generate optimality cut from subproblem dual values
            // ... add cut to master problem ...
        }
        
        OnBendersIterationEnd();
        
        // Check convergence
        // if (converged) break;
    }
    
    // === 3. CLEANUP ===
    OnBendersEnd();
    
    return 0;
}
```

### Simple Example (from example_usage.cpp)

```cpp
#include "MyLib.h"
#include <iostream>

extern "C" {
    void OnBendersStart(SubProblemsIds, int);
    #include "libmylib/include/julia_init.h"
}

int main() {
    // Initialize Julia
    init_julia(0, NULL);
    
    std::cout << "Example usage of libmyoutput.so" << std::endl;
    
    // Create subproblem IDs
    char* sub_ids[] = {(char*)"subproblem_1", (char*)"subproblem_2"};
    SubProblemsIds ids;
    ids.subProblems_ids = sub_ids;
    ids.n_subproblems = 2;
    
    int rank = 0;
    
    // Call initialization
    std::cout << "Calling OnBendersStart..." << std::endl;
    OnBendersStart(ids, rank);
    std::cout << "OnBendersStart completed!" << std::endl;
    
    // Cleanup
    shutdown_julia(0);
    return 0;
}
```

**Compile and run:**
```bash
cd libmyoutput_package
g++ example_usage.cpp \
    -I./include \
    -L./lib \
    -lmyoutput \
    -Wl,-rpath,'$ORIGIN/lib' \
    -o example_app

./example_app
```

---

## Deployment

### Transferring to Another Repository

#### 1. Copy the Package
```bash
# Copy entire package directory
cp -r libmyoutput_package /path/to/other/repo/

# OR use tar archive
tar -czf libmyoutput_package.tar.gz libmyoutput_package/
scp libmyoutput_package.tar.gz user@remote:/path/

# On remote:
tar -xzf libmyoutput_package.tar.gz
```

#### 2. Verify Dependencies
```bash
cd libmyoutput_package/lib
ldd libmyoutput.so
# Should show no "not found" errors
```

#### 3. Required Input Files

Your application needs these CSV files in the `inputs_julia/` directory:

```
inputs_julia/
├── B_inv.jls                          # Admittance matrix inverse
├── Ab.jls                             # Branch-node incidence matrix
├── Yl.jls                             # Line admittance matrix
├── A_hvdc.jls                         # HVDC incidence matrix
├── branches_to_candidates_dict.jls    # Branch → candidate mapping
├── n_side1_dict.jls                   # Line → from_node mapping
├── n_side2_dict.jls                   # Line → to_node mapping
├── dict_incident_outage_AC_branches.jls   # Incident definitions
├── dict_incident_HVDC_branches.jls         # HVDC incident definitions
├── max_flows_N.jls                    # N-state flow limits
└── max_flows_N_K.jls                  # N-K-state flow limits
```

Plus CSV mappings:
```
inputs/
├── constraints_dictionary.csv    # constraint_family → MPS_constraint_names
└── variables_dictionary.csv      # solver_var_name → csv_id
```

### Package Contents

- **Size**: ~730 MB uncompressed, ~204 MB compressed
- **Platform**: Linux x86_64 (requires rebuild for other platforms)
- **Dependencies included**: All Julia runtime libraries and dependencies

### Important Notes

1. **Keep libraries together** - Do NOT separate `libmyoutput.so` from `libmylib.so` and Julia libraries
2. **Preserve directory structure** - The `lib/julia/` subdirectory is required
3. **Platform-specific** - This is a Linux x86_64 build; rebuild for other platforms
4. **MPI required** - Your system needs OpenMPI or MPICH installed

---

## Performance Considerations

### Compilation Performance

The `compile.jl` file now uses `include_transitive_dependencies=false` to avoid recompiling all dependencies when only `MyLib.jl` changes. This significantly speeds up incremental builds.

**When full rebuild is needed:**
- Adding new `using` statements to MyLib.jl
- Updating packages in Project.toml/Manifest.toml

**When incremental build works:**
- Modifying Julia code in MyLib.jl (functions, algorithms)
- Changing constants or data structures

### Runtime Performance

- **First call overhead**: Julia JIT compilation occurs on first function call (~1-2s)
- **Subsequent calls**: Near-native performance (<1ms for typical network sizes)
- **Memory**: Shared data structures across MPI processes to minimize duplication
- **Serialization**: Binary serialization for fast MPI broadcast of network factors

---

## Troubleshooting

### Library Not Found Errors
```bash
# Check dependencies
ldd libmyoutput_package/lib/libmyoutput.so

# Set LD_LIBRARY_PATH if needed
export LD_LIBRARY_PATH=/path/to/libmyoutput_package/lib:$LD_LIBRARY_PATH
```

### Julia Initialization Errors
```
ERROR: could not load library "libmylib.so"
```
Solution: Ensure `libmylib.so` is in the same directory as `libmyoutput.so`

### MPI Errors
```
ERROR: MPI_Init has not been called
```
Solution: Initialize MPI before calling library functions:
```cpp
mpi::environment env(argc, argv);
```

### Segmentation Faults
- Ensure `init_julia()` is called before any library functions
- Ensure `shutdown_julia()` is called at program exit
- Check that arrays passed to functions have correct sizes

