/** This file should be included only into cpp files.
 * Otherwise, you might get name clashes.
 */

#pragma once

#include <iostream>
#include <sstream>
#include <stdexcept>


#define MASSERT(condition, message)                                                     \
    do {if(!(condition))                                                                \
    {                                                                                   \
        std::cerr << __FILE__ << " (" << __LINE__ << ") : " << message << std::endl;    \
        std::stringstream __ss;                                                         \
        __ss << message;                                                                \
        throw std::logic_error(__ss.str());                                             \
        abort();                                                                        \
    }} while (0)

#define UNREACHABLE()                                                               \
do {                                                                                    \
    {                                                                                   \
        std::cerr << __FILE__ << " (" << __LINE__ << ") : " << std::endl;               \
        throw std::logic_error("unreachable code reached");                             \
        abort();                                                                        \
    }} while (0)
