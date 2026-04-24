#!/bin/bash

set -e  # Exit on error

echo "=== Cleaning up directories ==="
rm -rf libmylib/  libmyoutput_package/

echo "=== Compiling julia ==="
julia compile.jl 

# Wait for both compilations to complete
echo "compile.jl completed"


echo "=== Packaging library ==="
./package_library.sh

echo "=== Build complete ==="
