# THIS BRANCH

This branch contains the code to generate QCIR benchmarks.
The benchmarks generated are available at
https://github.com/5nizza/relation-determinization


Below is the content of the original readme file for the synthesis tool `sdf`.


# sdf-hoa

The synthesis tool takes as input a TLSF or extended HOA file
and outputs `REALIZABLE` or `UNREALIZABLE`, and an AIGER model.
A brief description is available in the SYNTCOMP report
[https://arxiv.org/pdf/2206.00251.pdf](https://arxiv.org/pdf/2206.00251.pdf);
the tool re-invents the ideas of Ruediger Ehlers of the [symbolic bounded synthesis](https://ruediger-ehlers.de/papers/fmsd2012.pdf).


## Dependencies
Dependencies should be placed into folder `third_parties`.
I use the versions below but probably everything works with others as well,
but make sure to modify the paths mentioned in `CmakeLists.txt`.

- modified aiger-1.9.4, get it from [https://github.com/5nizza/aisy/tree/master/aiger_swig](https://github.com/5nizza/aisy/tree/master/aiger_swig)
- cudd-3.0.0
- spot-2.11.6
- spdlog-1.12.0
- [pstreams](http://pstreams.sourceforge.net/), version 1.0.3
- [args](https://github.com/Taywee/args): version 6.4.6
- [googletest](https://github.com/google/googletest) version 1.14.0\
  You need to download the source code together with its own `CMakeLists.txt`.
- It assumes [`syfco`](https://github.com/gaperez64/syfco) is in your path and callable.

Here is how `third_parties` look on my machine after downloading/installation of all dependencies:
```
aiger-1.9.4
args-6.4.6
cudd-3.0.0
googletest-release-1.14.0
pstreams-1.0.3
spdlog-1.12.0
spot-2.11.6
spot-install-prefix
```

## Build
After downloading all the dependencies, building and installing spot, and building cudd, do:

- `mkdir build`
- `cd build`
- `cmake ..` (or `cmake .. -DCMAKE_BUILD_TYPE=Debug` for version with debug symbols, default is Release)
- `make` (or `make VERBOSE=1` if you want to see compilation flags)

The resulting binaries will be placed in folder `build/bin/`.
You can run them with `-help` argument.

Note: there are tests, but they require having TLSF-AIGER model checker.
I use IIMC, `combine_aiger`, and this [script](https://gist.github.com/5nizza/14488e6fce0a29d297a38daefc95a1a8).
See also `tests/tests_synt.cpp` for details.

## SyntComp

In synthesis competition 2023, there were benchmarks causing SPOT to throw the error message:
```
Too many acceptance sets used.  The limit is 32.
```
To ease the problem, increase the limit when compiling SPOT:
```
./configure --enable-max-accsets=128 --prefix /home/art/software/sdf-hoa-master/third_parties/spot-install-prefix --disable-devel
make
make install
```
The above limit of `128` results in much less number of such errors.

🐾
