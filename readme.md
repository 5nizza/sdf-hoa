# jil

[TODO: update]

Dependencies:

- modifed aiger-1.9.4, get it from [https://github.com/5nizza/aisy/tree/master/aiger_swig](https://github.com/5nizza/aisy/tree/master/aiger_swig)
- cudd-3.0.0
- spdlog

To build, install dependencies into folder `<third_parties>`.
Modify `setup.sh` to provide the path to that folder.
Use the standard cmake routine for building:

- `. ./setup.sh`
- create build folder `<build>`
- `cd <build>`
- `cmake ..`
- `make`

