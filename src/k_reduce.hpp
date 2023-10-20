#pragma once

#define BDD spotBDD
    #include <spot/twa/twagraph.hh>
#undef BDD


namespace sdf
{

/**
 *
 * @param aut   co-Buchi automaton
 * @param max_k maximal number of visits to rejecting states of the same SCC (3 means the 4th visit is fatal)
 * @return      safety automaton
 */
spot::twa_graph_ptr k_reduce(const spot::twa_graph_ptr& aut, uint max_k);

}
