//
// Created by ayrat on 24/05/18.
//

#include <unordered_map>
#include <unordered_set>


#define BDD spotBDD
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/sccinfo.hh>
#undef BDD


#include "myassert.hpp"


using namespace std;


struct kState {
    uint state;
    uint k;

    kState(uint state_, uint k_) : state(state_), k(k_) {}

    bool operator==(const kState &rhs) const {
        return state == rhs.state && k == rhs.k;
    }

    bool operator!=(const kState &rhs) const {
        return !(rhs == *this);
    }

    kState& operator=(const kState& other) = default;
};


namespace std {
    template <>
    struct hash<kState> {
        size_t operator()(const kState &x) const {
            return x.state ^ x.k;
        }
    };
}


struct pair_hash {
    template <typename T, typename U>
    size_t operator()(const pair<T, U> &x) const {
        return hash<T>()(x.first) ^ hash<U>()(x.second);
    }
};


bool is_acc_sink(const spot::twa_graph_ptr &aut, uint state) {
    if (!aut->state_is_accepting(state))
        return false;

    for (auto &t: aut->out(state))
        if (t.dst == t.src && t.cond == bdd_true())
            return true;

    return false;
}


spot::twa_graph_ptr k_reduce(const spot::twa_graph_ptr &aut, uint max_k) {
    // `max_k` is the number of times an accepting state can be exited.

    MASSERT(max_k > 0, "max_k must be positive: " << max_k);
    MASSERT(aut->is_sba() == spot::trival::yes_value, "currently, only SBA is supported");

    auto k_aut = make_twa_graph(aut->get_dict());
    k_aut->set_buchi();

    spot::scc_info scc_info(aut);

    // `state` is a state of `aut`, `kstate` is a state of `k_aut`
    unordered_map<pair<uint, uint>, kState, pair_hash> kstate_by_state_k;
    unordered_map<kState, uint> state_by_kstate;
    unordered_set<kState> kstates_to_process;
    unordered_set<kState> processed_kstates;

    auto init_kstate = kState(k_aut->new_state(), max_k);
    k_aut->set_init_state(init_kstate.state);

    kstate_by_state_k.emplace(make_pair(aut->get_init_state_number(), max_k),
                              init_kstate);
    state_by_kstate.emplace(init_kstate,
                            aut->get_init_state_number());

    auto acc_ksink = kState(k_aut->new_state(), 0);
    k_aut->new_acc_edge(acc_ksink.state, acc_ksink.state, bdd_true());

    kstates_to_process.insert(init_kstate);
    processed_kstates.insert(acc_ksink);

    while (!kstates_to_process.empty()) {
        auto src_kstate = *kstates_to_process.begin();
        kstates_to_process.erase(src_kstate);
        processed_kstates.insert(src_kstate);

        MASSERT(src_kstate.k >= 1, "k < 1 is impossible: " << src_kstate.k);

        for (auto &t: aut->out(state_by_kstate.find(src_kstate)->second)) {
            kState dst_kstate(666, 666);  // uninitialized

            uint dst_k = scc_info.scc_of(t.src) == scc_info.scc_of(t.dst)
                         ?
                         src_kstate.k - aut->state_is_accepting(t.src)
                         : max_k;
            if (dst_k == 0 || is_acc_sink(aut, t.dst))
                dst_kstate = acc_ksink;
            else {
                auto dst_k_pair = make_pair(t.dst, dst_k);
                if (kstate_by_state_k.find(dst_k_pair) == kstate_by_state_k.end())
                    kstate_by_state_k.emplace(dst_k_pair, kState(k_aut->new_state(), dst_k));
                dst_kstate = kstate_by_state_k.find(dst_k_pair)->second;
            }
            k_aut->new_edge(src_kstate.state, dst_kstate.state, t.cond);
            if (processed_kstates.find(dst_kstate) == processed_kstates.end())
                kstates_to_process.insert(dst_kstate);
        }
    }

    k_aut->prop_terminal(spot::trival::yes_value);
    k_aut->prop_state_acc(spot::trival::yes_value);
    return k_aut;
}
