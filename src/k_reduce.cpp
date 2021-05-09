#include <unordered_map>


#define BDD spotBDD
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/sccinfo.hh>
#undef BDD


#include "my_assert.hpp"


using namespace std;


struct kState {
    uint state;
    uint k;      // "how many times can I exit an accepting state"; 0 means that the next "bad" exit leads to the sink.

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


spot::twa_graph_ptr k_reduce(const spot::twa_graph_ptr &aut, uint max_nof_visits)
{
    MASSERT(aut->is_sba().is_true(), "currently, only SBA is supported");

    auto k_aut = make_twa_graph(aut->get_dict());
    k_aut->set_buchi();
    k_aut->copy_ap_of(aut);

    spot::scc_info scc_info(aut);

    // `state` is a state of `aut`, `kstate` is a state of `k_aut`
    unordered_map<pair<uint, uint>, kState, pair_hash> kstate_by_state_k;
    vector<pair<kState, uint>> kstate_state_to_process;

    auto init_kstate = kState(k_aut->new_state(), max_nof_visits);
    k_aut->set_init_state(init_kstate.state);

    kstate_by_state_k.emplace(make_pair(aut->get_init_state_number(), max_nof_visits),
                              init_kstate);

    auto acc_ksink = kState(k_aut->new_state(), (uint)-1);
    k_aut->new_acc_edge(acc_ksink.state, acc_ksink.state, bdd_true());

    kstate_state_to_process.emplace_back(init_kstate, aut->get_init_state_number());

    while (!kstate_state_to_process.empty())
    {
        kState src_kstate(666, 666);
        uint src_state;
        tie(src_kstate, src_state) = kstate_state_to_process.back();
        kstate_state_to_process.erase(kstate_state_to_process.end()-1);

        for (const auto &t: aut->out(src_state))
        {
            int dst_k = scc_info.scc_of(t.src) == scc_info.scc_of(t.dst)
                        ?
                        (int)src_kstate.k - aut->state_is_accepting(t.src)
                        : (int)max_nof_visits;

            if (dst_k == -1 || is_acc_sink(aut, t.dst))
            {
                k_aut->new_edge(src_kstate.state, acc_ksink.state, t.cond);
                continue;
            }

            auto pair_dst_k = make_pair(t.dst, dst_k);
            auto dst_kstateIt = kstate_by_state_k.find(pair_dst_k);
            if (dst_kstateIt == kstate_by_state_k.end())
            {
                auto dst_kstate = kState(k_aut->new_state(), (uint)dst_k);
                tie(dst_kstateIt, ignore) = kstate_by_state_k.emplace(pair_dst_k, dst_kstate);
                kstate_state_to_process.emplace_back(dst_kstate, t.dst);
            }
            k_aut->new_edge(src_kstate.state, dst_kstateIt->second.state, t.cond);
        }
    }

    k_aut->prop_terminal(true);
    k_aut->prop_state_acc(true);

    return k_aut;
}
