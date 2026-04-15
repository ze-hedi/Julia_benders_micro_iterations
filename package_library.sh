#!/bin/bash
# Script to create a self-contained package of libmyoutput.so with all dependencies

set -e

PACKAGE_NAME="libmyoutput_package"
PACKAGE_DIR="./${PACKAGE_NAME}"

echo "Creating self-contained package: ${PACKAGE_NAME}"

# Clean and create package directory
rm -rf "${PACKAGE_DIR}"
mkdir -p "${PACKAGE_DIR}/lib"
mkdir -p "${PACKAGE_DIR}/include"

# Recompile libmyoutput.so with correct rpath for package
echo "Recompiling libmyoutput.so with package-friendly rpath..."
g++ -shared -fPIC micro_iters.cpp \
    -I./libmylib/include \
    -I/usr/include/julia \
    -I/usr/lib/x86_64-linux-gnu/openmpi/include \
    -I/usr/lib/x86_64-linux-gnu/openmpi/include/openmpi \
    -L./libmylib/lib \
    -L/usr/lib/x86_64-linux-gnu/openmpi/lib \
    -lmylib -ljulia \
    -lboost_mpi -lboost_serialization -lmpi_cxx -lmpi \
    -Wl,-rpath,'$ORIGIN' \
    -o "${PACKAGE_DIR}/lib/libmyoutput.so"

# Copy libmylib dependencies
echo "Copying libmylib dependencies..."
cp -r libmylib/lib/* "${PACKAGE_DIR}/lib/"

# Copy headers if they exist
if [ -d "libmylib/include" ]; then
    echo "Copying headers..."
    cp -r libmylib/include/* "${PACKAGE_DIR}/include/"
fi

# Copy MyLib.h if it exists
if [ -f "MyLib.h" ]; then
    cp MyLib.h "${PACKAGE_DIR}/include/"
fi

# Copy artifacts directory for Julia dependencies
if [ -d "libmylib/share/julia/artifacts" ]; then
    echo "Copying Julia artifacts..."
    mkdir -p "${PACKAGE_DIR}/share/julia"
    cp -r libmylib/share/julia/artifacts "${PACKAGE_DIR}/share/julia/"
fi

# Create a usage README
cat > "${PACKAGE_DIR}/README.md" << 'EOF'
# libmyoutput Package

This is a self-contained package with all dependencies.

## Directory Structure
```
libmyoutput_package/
├── lib/                  # All shared libraries
│   ├── libmyoutput.so   # Main library
│   ├── libmylib.so      # MyLib dependency
│   └── julia/           # Julia runtime libraries
├── include/             # Header files
└── share/
    └── julia/
        └── artifacts/   # Julia binary artifacts (JLL packages)
```

## Usage in Your Project

### Option 1: Using LD_LIBRARY_PATH (easiest for development)
```bash
export LD_LIBRARY_PATH=/path/to/libmyoutput_package/lib:$LD_LIBRARY_PATH
# Run your application
./your_app
```

### Option 2: Compile with rpath (recommended for distribution)
```bash
g++ your_app.cpp \
    -I/path/to/libmyoutput_package/include \
    -L/path/to/libmyoutput_package/lib \
    -lmyoutput \
    -Wl,-rpath,/path/to/libmyoutput_package/lib \
    -o your_app
```

### Option 3: Relative rpath (for portable deployments)
If you want to keep the library next to your executable:
```bash
# Directory structure:
# your_project/
# ├── your_app
# └── lib/  (copy libmyoutput_package/lib/* here)

g++ your_app.cpp \
    -I/path/to/libmyoutput_package/include \
    -L/path/to/libmyoutput_package/lib \
    -lmyoutput \
    -Wl,-rpath,'$ORIGIN/lib' \
    -o your_app
```

## Dependencies Included
- libmylib.so
- Julia runtime libraries (libjulia.so, libjulia-internal.so, etc.)
- All Julia support libraries (libgcc_s, libunwind, libz, libatomic, libstdc++, etc.)
- Julia artifacts (binary dependencies for packages like Libiconv_jll, etc.)

## Verify Dependencies
To check that all dependencies are included:
```bash
cd lib
ldd libmyoutput.so
```

All libraries should show paths relative to this package (no "not found" errors).
EOF

echo ""
echo "Package created successfully: ${PACKAGE_DIR}/"
echo ""
echo "Package size:"
du -sh "${PACKAGE_DIR}"
echo ""
echo "To use in another repository, copy the entire '${PACKAGE_NAME}' directory."
echo "See ${PACKAGE_DIR}/README.md for usage instructions."
