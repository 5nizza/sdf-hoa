# sdf-hoa

The synthesis tool takes as input a TLSF file
and outputs `REALIZABLE` or `UNREALIZABLE`, and an AIGER model.
A brief description is available in the SYNTCOMP report
[https://arxiv.org/pdf/2206.00251.pdf](https://arxiv.org/pdf/2206.00251.pdf),
the tool re-invents the ideas of Ruediger Ehlers of the [symbolic bounded synthesis](https://ruediger-ehlers.de/papers/fmsd2012.pdf).


## Dependencies
Dependencies should be placed into folder `third_parties`.
I use the versions below but probably everything works with others as well,
but make sure to modify the paths mentioned in `CmakeLists.txt`.

- modified aiger-1.9.4, get it from [https://github.com/5nizza/aisy/tree/master/aiger_swig](https://github.com/5nizza/aisy/tree/master/aiger_swig)
- cudd-3.0.0
- spot-2.11.6
- spdlog
- [pstreams](http://pstreams.sourceforge.net/), version 1.0.3
- [args](https://github.com/Taywee/args): version 6.4.6
- [googletest](https://github.com/google/googletest) version 1.14.0\
  You need to download the source code together with its own `CMakeLists.txt`.

Here is how `third_parties` look on my machine after downloading/installation of all dependencies:
```
aiger-1.9.4
args-6.4.6
cudd-3.0.0
googletest-release-1.14.0
pstreams-1.0.3
spdlog/spdlog
spot-2.11.6
spot-install-prefix
```
(you should have the folder `spdlog` with same-named folder `spdlog` inside)

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

🐾
