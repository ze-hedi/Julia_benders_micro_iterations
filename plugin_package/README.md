# micro_iters_plugin Package

This is a self-contained package with all dependencies.

## Directory Structure
```
libmyoutput_package/
├── lib/                  # All shared libraries
│   ├── micro_iters_plugin.so   # Main library
│   ├── libGridModelisation.so      # MyLib dependency
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
- libGridModelisation.so
- Julia runtime libraries (libjulia.so, libjulia-internal.so, etc.)
- All Julia support libraries (libgcc_s, libunwind, libz, libatomic, libstdc++, etc.)
- Julia artifacts (binary dependencies for packages like Libiconv_jll, etc.)

## Verify Dependencies
To check that all dependencies are included:
```bash
cd lib
ldd micro_iters_plugin.so
```

All libraries should show paths relative to this package (no "not found" errors).
