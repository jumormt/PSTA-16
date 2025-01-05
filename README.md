
# 1.Basic Usage

## Dependencies

- SVF: https://github.com/jumormt/SVF-xiao/tree/fse/

## Build

```shell
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug/Release -DSVF_DIR=/path/to/svf -DLLVM_DIR=/path/to/llvm -DZ3_DIR=/path/to/z3 -DCMAKE_INSTALL_PREFIX=/path/to/install ..
cmake --build . -j 16
cmake --build . --target install
```

The arguments `SVF_DIR, LLVM_DIR, Z3_DIR` can be removed if they are configured in environment variables.

## RUN

detect memory leak

```shell
psta -leak /path/to/bc
```

## Test

```sh
cd build
ctest -j8
```

## RUN



# References

> [[1].Cheng X, Ren J, Sui Y. Fast Graph Simplification for Path-Sensitive Typestate Analysis through Tempo-Spatial Multi-Point Slicing[J]. Proceedings of the ACM on Software Engineering, 2024, 1(FSE): 494-516.](https://dl.acm.org/doi/pdf/10.1145/3643749)

> [[2].Manuvir Das, Sorin Lerner, and Mark Seigle. 2002. ESP: Path-Sensitive Program Verification in Polynomial Time. In Proceedings of the ACM SIGPLAN 2002 conference on Programming language design and implementation (PLDI ’02). ACM.](https://doi.org/10.1145/512529.512538)