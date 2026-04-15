#!/bin/bash

set -e  # Exit on error

echo "=== Cleaning up directories ==="
rm -rf libmylib/ libmylib2/ libmyoutput_package/

echo "=== Compiling in parallel ==="
julia compile.jl &
PID1=$!
julia compile2.jl &
PID2=$!

# Wait for both compilations to complete
wait $PID1
echo "compile.jl completed"
wait $PID2
echo "compile2.jl completed"

echo "=== Packaging library ==="
./package_library.sh

echo "=== Build complete ==="
