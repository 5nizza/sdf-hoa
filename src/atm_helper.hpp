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

}  // namespace sdf
