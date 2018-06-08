#ifndef SDF_LTL_PARSER_H
#define SDF_LTL_PARSER_H


#include <string>
#include <tuple>

#define BDD spotBDD
#include <spot/tl/formula.hh>
#undef BDD


/*
 * Convert `ltl_file_name` as TLSF (if the name ends with `.tlsf`),
 * and return <formula, inputs, outputs>.
 */
std::tuple<spot::formula, std::vector<spot::formula>, std::vector<spot::formula>>
parse_tlsf(const std::string& tlsf_file_name);


#endif //SDF_LTL_PARSER_H
