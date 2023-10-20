#pragma once

#define BDD spotBDD
    #include <spot/twa/twagraph.hh>
    #include <spot/twaalgos/sccinfo.hh>
#undef BDD

namespace sdf
{

inline bool is_acc_sink(const spot::twa_graph_ptr& aut, uint state)
{
    if (!aut->state_is_accepting(state))
        return false;

    for (auto& t: aut->out(state))
        if (t.dst == t.src && t.cond == bdd_true())
            return true;

    return false;
}

inline void merge_atm(spot::twa_graph_ptr& atm)
{
    LOOP:
    auto nof_states_old = atm->num_states();
    auto nof_edges_old = atm->num_edges();
    atm->merge_states();
    atm->merge_edges();
    if (atm->num_states() < nof_states_old ||
        atm->num_edges() < nof_edges_old)
        goto LOOP;
}

}  // namespace sdf
