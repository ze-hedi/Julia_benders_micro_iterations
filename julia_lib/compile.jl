using PackageCompiler

create_library(
    ".",
    joinpath("..", "build", "julia");
    lib_name="GridModelisation",
    precompile_execution_file="src/MyLib.jl",
    include_lazy_artifacts=true,
    include_transitive_dependencies=false
)