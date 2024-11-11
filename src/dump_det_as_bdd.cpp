#include "dump_det_as_bdd.hpp"
#include "my_assert.hpp"

#include "dddmp.h"


using namespace sdf;
using namespace std;


void dump_determinization_problem_to_bdd_file(const Cudd& cudd,
                                              const std::set<uint>& inputs,
                                              const std::set<uint>& outputs,
                                              const std::set<uint>& states,
                                              const BDD& init,
                                              const BDD& error,
                                              const BDD& win_region,
                                              const std::unordered_map<uint,BDD>& pre_by_state)
{

//    auto res = Dddmp_cuddBddArrayStore(cudd.getManager(), ddname, nroots, nodes, ...);
//    MASSERT(res == DDDMP_SUCCESS, "");
}
