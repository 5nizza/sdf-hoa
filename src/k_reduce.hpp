#pragma once

#define BDD spotBDD
#include <spot/twa/twagraph.hh>
#undef BDD

spot::twa_graph_ptr k_reduce(const spot::twa_graph_ptr &aut, uint max_k);
