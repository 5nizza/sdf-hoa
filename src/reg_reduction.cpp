#include "reg_reduction.hpp"

#include "ehoa_parser.hpp"
#include "utils.hpp"

#define BDD spotBDD
    #include <spot/twaalgos/dot.hh>
#undef BDD


using namespace std;
using namespace sdf;


#define DEBUG(message) {spdlog::get("console")->debug() << message;}
#define INF(message) {spdlog::get("console")->info() << message;}


void sdf::reduce(spot::twa_graph_ptr reg_ucw, uint nof_sys_regs,
                 spot::twa_graph_ptr& classical_ucw,
                 set<spot::formula>& sysTst,
                 set<spot::formula>& sysAsgn,
                 set<spot::formula>& sysR)
{

}
