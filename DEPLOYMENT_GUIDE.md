# Deployment Guide for libmyoutput.so

## Summary

Your library `libmyoutput.so` has been packaged with all its dependencies into a self-contained distribution.

## Package Contents

### Files Created:
- `libmyoutput_package/` - Complete self-contained directory (730 MB)
- `libmyoutput_package.tar.gz` - Compressed archive (204 MB) for easy transfer

### Package Structure:
```
libmyoutput_package/
├── lib/
│   ├── libmyoutput.so          # Your library
│   ├── libmylib.so             # Main dependency (459 MB)
│   ├── libjulia.so.1.11        # Julia runtime
│   └── julia/                  # Julia support libraries
│       ├── libjulia-internal.so.1.11
│       ├── libgcc_s.so.1
│       ├── libunwind.so.8
│       ├── libz.so.1
│       ├── libatomic.so.1
│       ├── libstdc++.so.6
│       └── ... (other Julia libraries)
├── include/
│   ├── MyLib.h                 # Main header
│   └── julia_init.h            # Julia initialization header
├── example_usage.cpp           # Example code
└── README.md                   # Usage instructions
```

## Why You Need All These Files

`libmyoutput.so` depends on:
1. **libmylib.so** - Your custom library
2. **Julia runtime** - libjulia.so and libjulia-internal.so
3. **Julia support libraries** - Various system libraries that Julia needs

Without these dependencies, you'll get "library not found" errors at runtime.

## How to Deploy to Another Repository

### Step 1: Transfer the package
```bash
# Option A: Copy the directory
cp -r libmyoutput_package /path/to/other/repo/

# Option B: Use the compressed archive
scp libmyoutput_package.tar.gz user@remote:/path/
# Then on remote:
tar -xzf libmyoutput_package.tar.gz
```

### Step 2: Use in your project

#### Method 1: Quick test with LD_LIBRARY_PATH
```bash
export LD_LIBRARY_PATH=/path/to/libmyoutput_package/lib:$LD_LIBRARY_PATH
g++ your_app.cpp -I/path/to/libmyoutput_package/include -L/path/to/libmyoutput_package/lib -lmyoutput -o your_app
./your_app
```

#### Method 2: Compile with absolute rpath (recommended)
```bash
g++ your_app.cpp \
    -I/path/to/libmyoutput_package/include \
    -L/path/to/libmyoutput_package/lib \
    -lmyoutput \
    -Wl,-rpath,/path/to/libmyoutput_package/lib \
    -o your_app
    
./your_app  # No need to set LD_LIBRARY_PATH
```

#### Method 3: Portable deployment with relative rpath
If you want your application to be portable:
```bash
# Create this structure:
your_project/
├── bin/
│   └── your_app
└── lib/  # Copy all files from libmyoutput_package/lib/ here

# Compile with relative rpath:
g++ your_app.cpp \
    -I/path/to/libmyoutput_package/include \
    -L/path/to/libmyoutput_package/lib \
    -lmyoutput \
    -Wl,-rpath,'$ORIGIN/../lib' \
    -o your_app

# Move executable to bin/
mv your_app your_project/bin/

# Now you can move the entire your_project/ folder anywhere
```

## Verification

To verify all dependencies are satisfied:
```bash
cd libmyoutput_package/lib
ldd libmyoutput.so
```

All paths should be resolved (no "not found" errors).

## Size Considerations

- **Uncompressed**: 730 MB
- **Compressed**: 204 MB
- Most of the size comes from `libmylib.so` (459 MB) and Julia runtime libraries

## Important Notes

1. **Do NOT separate the libraries** - They must stay together as packaged
2. **Preserve directory structure** - The `lib/julia/` subdirectory is required
3. **Include headers** - If other developers need to compile against your library, they need the `include/` directory
4. **Platform specific** - This package is for Linux x86_64. You'll need to rebuild for other platforms.

## Quick Start Example

See `libmyoutput_package/example_usage.cpp` for a complete working example.

Compile and run:
```bash
cd libmyoutput_package
g++ example_usage.cpp -I./include -L./lib -lmyoutput -Wl,-rpath,'$ORIGIN/lib' -o example_app
./example_app
```
