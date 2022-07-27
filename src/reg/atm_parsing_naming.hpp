#pragma once

#include "my_assert.hpp"
#include "reg_utils.hpp"


#define BDD spotBDD
    #include <spot/twa/twagraph.hh>
    #include <spot/tl/formula.hh>
#undef BDD


// TODO: hide all unnecesarry details


namespace sdf
{


static const std::string TST_CMP_TOKENS[] = {"=","≠","<",">","≤","≥"};  // NOLINT

static const std::string  IN = "in";   // NOLINT
static const std::string OUT = "out";  // NOLINT
static const std::string  RS = "rs";   // NOLINT
static const std::string   R = "r";    // NOLINT

/**
 * A test atom has the form:
 *   atom := in◇rX | out◇rX | in◇out
 * where ◇ is one of =,<,>,≠,≤,≥
 */
inline
bool is_tst(const std::string& ap_name)
{
    for (const auto& tok : TST_CMP_TOKENS)  // NOLINT
        if (ap_name.find(tok) != std::string::npos)  // we have to use `find` because some of the tokens consist of multiple chars
            return true;
    return false;
}


/** Assignments have form in↓rX | out↓rX */
inline
bool is_atm_asgn(const std::string& name)
{
    return name.find("↓") != std::string::npos;
}


// ASSUMPTION: one data input and one data output
// APs naming convention for system tst/asgn/R

inline spot::formula ctor_tst_inp_smaller_r(const std::string& r) { return spot::formula::ap(IN + "<" + r); }
inline spot::formula ctor_tst_inp_equal_r(const std::string& r)   { return spot::formula::ap(IN + "=" + r); }
inline spot::formula ctor_sys_asgn_r(const std::string& r)        { return spot::formula::ap(IN + "↓" + r); }
inline spot::formula ctor_sys_out_r(const std::string& r)         { return spot::formula::ap("↑" + r); }

inline std::string ctor_atm_reg(uint num) { return R+std::to_string(num); }
inline std::string ctor_sys_reg(uint num) { return RS+std::to_string(num); }

inline bool is_input_name(const std::string& name)   { return name.length() >= 2 and name.substr(0,2) == IN; }
inline bool is_output_name(const std::string& name)  { return name.length() >= 3 and name.substr(0,3) == OUT; }
inline bool is_reg_name(const std::string& name)     { return name.length() >  0 and name[0] == 'r'; }
inline bool is_sys_reg_name(const std::string& name) { return name.length() >= RS.length() and name.substr(0,RS.length()) == RS; }
inline bool is_atm_reg_name(const std::string& name) { return is_reg_name(name) && !is_sys_reg_name(name); }


/** Extract t1,t2,◇ from  t1◇t2 */
inline
std::tuple<std::string, std::string, std::string>
parse_tst(const std::string& action_name)
{
    MASSERT(is_tst(action_name), action_name);

    uint index_cmp = 0;
    size_t pos_cmp;
    for (; index_cmp < sizeof TST_CMP_TOKENS; ++index_cmp)
        if ((pos_cmp = action_name.find(TST_CMP_TOKENS[index_cmp])) != std::string::npos)
            break;

    auto cmp = TST_CMP_TOKENS[index_cmp];
    auto t1 = trim_spaces(action_name.substr(0, pos_cmp));
    auto t2 = trim_spaces(action_name.substr(pos_cmp+cmp.length(), std::string::npos));

    return {t1,t2,cmp};
}

/** Returns pair (variable_name, register) */
inline
std::pair<std::string, std::string>
parse_asgn_atom(const std::string& asgn_atom)
{
    // Examples: "in↓r1", "out ↓ r2"
    auto trimmed_action_name = trim_spaces(asgn_atom);
    auto i = trimmed_action_name.find("↓");
    auto var_name = trim_spaces(trimmed_action_name.substr(0, i));
    auto reg = trim_spaces(trimmed_action_name.substr(i + std::string("↓").length(), std::string::npos));
    MASSERT(reg[0] == 'r', reg);
    return {var_name, reg};
}

/** string names for system registers */             // TODO: do we really need to expose this?
inline
std::unordered_set<std::string>
construct_sysR(uint nof_sys_regs)
{
    std::unordered_set<std::string> result;
    for (uint i = 1; i<= nof_sys_regs; ++i)
        result.insert(RS + std::to_string(i));
    return result;
}

/** extract string names of automaton registers */   // TODO: do we really need to expose this?
inline
std::unordered_set<std::string>
extract_atmR(const spot::twa_graph_ptr& reg_atm)
{
    std::unordered_set<std::string> result;
    for (const auto& ap : reg_atm->ap())
    {
        const auto& ap_name = ap.ap_name();

        if (!is_tst(ap_name) and !is_atm_asgn(ap_name))
            continue;

        if (is_tst(ap_name))
        {
            auto [t1,t2,_] = parse_tst(ap_name);
            if (is_reg_name(t1))
                result.insert(t1);
            if (is_reg_name(t2))
                result.insert(t2);
        }
        else  // is_atm_asgn(ap_name)
            result.insert(parse_asgn_atom(ap_name).second);
    }
    return result;
}


} // namespace sdf