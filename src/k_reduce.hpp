#ifndef SDF_K_REDUCE_H
#define SDF_K_REDUCE_H


#define BDD spotBDD
#include <spot/twa/twagraph.hh>
#undef BDD

spot::twa_graph_ptr k_reduce(const spot::twa_graph_ptr &aut, uint max_k);

#endif // SDF_K_REDUCE_H
