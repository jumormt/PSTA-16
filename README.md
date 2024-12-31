## Dependencies

- SVF: https://github.com/jumormt/SVF-xiao/tree/fse/

## Build

```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug/Release -DSVF_DIR=/path/to/svf -DLLVM_DIR=/path/to/llvm -DZ3_DIR=/path/to/z3 ..
make -j8
```

## Test

```sh
cd build
ctest -j8
```