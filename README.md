# sdf-hoa

The synthesis tool takes as input a TLSF file
and outputs `REALIZABLE` or `UNREALIZABLE` (thus, it is a realizability checker).
The theory behind the tool is described in
[Schewe and Finkbeiner](https://www.react.uni-saarland.de/publications/atva07.pdf) and
[Kupferman](http://www.cse.huji.ac.il/~ornak/publications/lics06c.pdf).

## Dependencies
They should be placed into the folder `third_parties`.
From more details, consult `CMakeLists.txt`, where you can find the exact folder names that `sdf-hoa` expects.

- modified aiger-1.9.4, get it from [https://github.com/5nizza/aisy/tree/master/aiger_swig](https://github.com/5nizza/aisy/tree/master/aiger_swig)
- cudd-3.0.0
- spot-2.5.3
- spdlog
- [args](https://github.com/Taywee/args): it should be placed into `third_parties` folder (see `CMakeLists.txt`)
- [googletest](https://github.com/google/googletest) version 1.10.0 (see `CMakeLists.txt`):
  you will need to download the source code (together with its own `CMakeLists.txt`),
  then it will be built together with me.

## Build
To build, install dependencies into folder `third_parties` (except for `googletest` which is automatically built by `sdf`).
Then:

- create folder `build`
- `cd build`
- `cmake ..`
- `make`

The resulting binaries should be placed in folder `build/src/`.
You can run them with `-help` argument.

Probably the above steps won't work out of the box,
so feel free to contact my author.
