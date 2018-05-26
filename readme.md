# sdf-hoa

The synthesis tool takes as input:

- automaton in HOA format
- separation of signals into inputs and outputs

and outputs `REALIZABLE` or `UNREALIZABLE` (thus, it is a realizability checker).
The theory behind the tool is described in
[Schewe and Finkbeiner](https://www.react.uni-saarland.de/publications/atva07.pdf) and
[Kupferman](http://www.cse.huji.ac.il/~ornak/publications/lics06c.pdf).
This tool is used by tool `kid_hoa.py` submitted to SYNTCOMP'18.
But actually, `kid_hoa` adds nothing and does only boring stuff.

Dependencies:

- modifed aiger-1.9.4, get it from [https://github.com/5nizza/aisy/tree/master/aiger_swig](https://github.com/5nizza/aisy/tree/master/aiger_swig)
- cudd-3.0.0
- spot-2.5.3
- spdlog

To build, install dependencies into folder `third_parties`.
Then:

- create folder `build`
- `cd build`
- `cmake ..`
- `make`

The resulting binaries should be placed in folder `build/src/`.
You can run them with `-help` argument.

Probably the above steps won't work out of the box,
so feel free to contact me.

