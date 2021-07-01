#pragma once

#include <iostream>


#define MASSERT(condition, message)                                                     \
    do {if(!(condition))                                                                \
    {                                                                                   \
        std::cerr << __FILE__ << " (" << __LINE__ << ") : " << message << std::endl;    \
        abort();                                                                        \
    }}while (0)
