using PackageCompiler

create_library(
    ".",
    "libmylib2";
    lib_name="mylib2",
    precompile_execution_file="src/MyLib.jl",
    include_lazy_artifacts=true,
    include_transitive_dependencies=false
)