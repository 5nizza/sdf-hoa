//
// Created by ayrat on 24/05/16.
//

#pragma once

#include <iostream>


#define MASSERT(condition, message)                                                     \
{                                                                                       \
    if(!(condition))                                                                    \
    {                                                                                   \
        std::cerr << __FILE__ << " (" << __LINE__ << ") : " << message << std::endl;    \
        abort();                                                                        \
    }                                                                                   \
}
