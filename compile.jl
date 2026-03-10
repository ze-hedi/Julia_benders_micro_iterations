using PackageCompiler

create_library(
    ".",
    "libmylib";
    lib_name="mylib",
    precompile_execution_file="src/MyLib.jl",  # Changed to init.jl
    include_lazy_artifacts=false
    
)