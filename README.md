
# 1.Basic Usage

## 1.1.Dependencies

- SVF: https://github.com/jumormt/SVF-xiao/tree/fse/

## 1.2.Build

```shell
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug/Release -DSVF_DIR=/path/to/svf -DLLVM_DIR=/path/to/llvm -DZ3_DIR=/path/to/z3 -DCMAKE_INSTALL_PREFIX=/path/to/install ..
cmake --build . -j 16
cmake --build . --target install
```

The arguments `SVF_DIR, LLVM_DIR, Z3_DIR` can be removed if they are configured in environment variables. 

For LLVM, when building it, its recommended to add option `-DLLVM_LINK_LLVM_DYLIB=ON` to build dynamic lib to make it less likely to encounter error like duplicated definition of symbol `llvm::opt...`.

For SVF, its recommended to replace the line `#define STATIC_OBJECT malloc(10)` in extapi.c with following contents to reduce the false positive memory leak report.

```cpp
#include <alloca.h>

#define STATIC_OBJECT alloca(10)
```

## 1.3.RUN

```shell
# detect memory leak
psta -leak /path/to/bc
# detect double free
psta -df /path/to/bc
# detect use after free
pst -uaf /path/to/bc

# detect memory leak while not use graph simplification for psta, not recommended
psta -base -leak /path/to/bc
```

Given following program:

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Student {
    int num;
    float score;
    void *data;
} Student;

int main() {
    Student* s1 = (Student*)malloc(sizeof(Student));
    s1->num = 10;
    s1->score = 100;
    s1->data = malloc(10);

    int num = s1->num;
    float score = s1->score;
    void* data = s1->data;

    free(s1);
    return 0;
}
```

Using command `clang -emit-llvm -S -g -fno-discard-value-names -Xclang -no-opaque-pointers test.c` to compile and use before command to run leak detector, following problem will be reported:

```
${SVF statistics}
         NeverFree : memory allocation at : ({ "ln": 14, "cl": 16, "fl": "test.c
" })

*********leak***************
################ (program : /home/cbj/projects/experiments/testcases/test4/test)
###############
Cxt Alias                     1
OTF Alias                     0
Cxt Limit                     3
Multi Slicing                 0
Enable Spatial Slicing        1
Enable Temporal Slicing       1
Enable Isolated Summary       0
Snk Limit                     10
Max Z3 Size                   30
Max PI Size                   100
Max Step In Wrapper           10
Spatial Layer Num             0
-------------------------------------------------------
SrcNum                        2
AbsState Domain Size          4
Var Avg Num                   0
Loc Avg Num                   0
Var Addr Avg Num              1
Loc Addr Avg Num              0
Var AddrSet Avg Size          0
Loc AddrSet Avg Size          0
Info Map Avg Size             6
Summary Map Avg Size          1
Graph Avg Node Num            7
Graph Avg Edge Num            6
Tracking Branch Avg Num       0
ICFG Node Num                 50
ICFG Edge Num                 40
Branch Num                    0
Callsites Avg Num             0
Spatial Avg Num               6
Bug Num                       1
-------------------------------------------------------
InitSrc                       0s
InitAbsTransferFunc           0s
Solve                         0.001s
Compact Graph Time            0s
WrapICFGTime                  0s
Collecting Call Time          0s
Tracking Branch Time          0s
TotalTime                     0.005s
Memory usage: 102836KB
#######################################################
```

## 1.4.Test

```sh
cd build
ctest -j8
```


# 2.Options

In [main function](tools/psta.cpp), we add options for `-model-consts=true -model-arrays=true -pre-field-sensitive=false -ff-eq-base -field-limit=16`,
indicating we treat each constant as independent object, and model each array item, also treating the first field of a memory object as identical to the object.

| option | meaning | default |
| ---- | ---- | ---- |
| `-leak` | Whether to detect memory leak | `false` |
| `-df` | Whether to detect double free | `false` |
| `-uaf` | Whether to detect use after free | `false` |
| `-base` | Use base typestate analysis, not to use graph simplification proposed in reference [1] | `false` |
| `-path-sensitive` | use path sensitive type state analysis | `true` |
| `-spatial` | enable SMS when slicing, must be enabled with `-base=false` | `true` |
| `-temporal` | enable TMS when slicing, must be enabled with `-base=false` | `true` |


# 3.References

> [[1].Cheng X, Ren J, Sui Y. Fast Graph Simplification for Path-Sensitive Typestate Analysis through Tempo-Spatial Multi-Point Slicing[J]. Proceedings of the ACM on Software Engineering, 2024, 1(FSE): 494-516.](https://dl.acm.org/doi/pdf/10.1145/3643749)

> [[2].Manuvir Das, Sorin Lerner, and Mark Seigle. 2002. ESP: Path-Sensitive Program Verification in Polynomial Time. In Proceedings of the ACM SIGPLAN 2002 conference on Programming language design and implementation (PLDI ’02). ACM.](https://doi.org/10.1145/512529.512538)