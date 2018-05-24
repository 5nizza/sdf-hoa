//
// Created by ayrat on 24/05/18.
//

#define BDD spotBDD
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/sccinfo.hh>
#undef BDD

#include "myassert.hpp"


/*
 * When `uniform` is set, we reset the counter on crossing the border between SCCs.
 * When it is not set, k limits the total number of visits to bad states.
*/
spot::twa_graph_ptr k_reduce(spot::twa_graph_ptr atm, uint k, bool uniform=true) {
    MASSERT(k >= 0, "k must be positive: " << k);

//    finSCC_by_node = build_state_to_final_scc(atm);

//    dead_node = Node('dead');
//    dead_node.add_transition(LABEL_TRUE, {(dead_node, True)});
//    dead_node.k = 0;

//    new_by_old_k = dict()       # type: Dict[(Node, int), Node]
//
//    old_by_new = dict()      # type:
//    Dict[Node, Node]
//
//    nodes_to_process = set() # type:
//    Set[Node]
//    for
//    n
//    in
//    atm.init_nodes:
//    new_n = _get_add_node(n, k)
//    old_by_new[new_n] = n
//    nodes_to_process.add(new_n)
//
//    processed_nodes = set()  # type:
//    Set[Node]
//    processed_nodes.add(dead_node)
//    while
//        nodes_to_process:
//        new_src = nodes_to_process.pop()
//    processed_nodes.add(new_src)
//    old_src = old_by_new[new_src]
//    for
//    lbl, node_flag_pairs
//    in
//    old_by_new[new_src].transitions.items():  # type:
//    (Label, Set[(Node, bool)])
//    for
//    old_dst, is_fin
//    in
//    node_flag_pairs:
//    if is_final_sink(old_dst):
//    new_dst = dead_node
//    else:
//    new_dst_k = new_src.k - is_fin
//    if (not uniform or _within_same_finSCC(old_src, old_dst, finSCC_by_node))\
//                                else
//    k
//            new_dst = _get_add_node(old_dst, new_dst_k)
//# For "into dead" transitions (lbl, dead) it is possible
//# that it is already present, so we check
//    if lbl
//    not in
//    new_src.transitions or (new_dst, False)
//    not in
//    new_src.transitions[lbl]:
//    new_src.add_transition(lbl, {(new_dst, False)})
//    else:
//    assert
//    new_dst == dead_node, "I know only the case of repetitions of transitions into dead"
//    old_by_new[new_dst] = old_dst
//    if new_dst
//    not in
//    processed_nodes:
//    nodes_to_process.add(new_dst)
//
//    return Automaton({_get_add_node(next(iter(atm.init_nodes)), k)},
//                     processed_nodes)
}
