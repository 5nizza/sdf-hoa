#pragma once

#include <iostream>
#include <sstream>
#include <stdexcept>


#define MASSERT(condition, message)                                                     \
    do {if(!(condition))                                                                \
    {                                                                                   \
        std::cerr << __FILE__ << " (" << __LINE__ << ") : " << message << std::endl;    \
        std::stringstream ss;                                                           \
        ss << message;                                                                  \
        throw std::logic_error(ss.str());                                               \
        abort();                                                                        \
    }}while (0)
