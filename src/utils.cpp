//
// Created by ayrat on 31/05/18.
//

#include "utils.hpp"

#include <string>
#include <iostream>

#include <pstream.h>
#include <tuple>


using namespace std;


tuple<int, string, string> ak_utils::execute(const string& cmd)
{
    return ak_utils::execute(cmd.c_str());
}


tuple<int, string, string> ak_utils::execute(const char* cmd)
{   // TODO:AK: what happens when cmd crashes?
    redi::ipstream proc(cmd, redi::pstreams::pstdout | redi::pstreams::pstderr);
    string line;

    stringstream out;
    while (getline(proc.out(), line))
        out << line << '\n';

    stringstream err;
    while (getline(proc.err(), line))
        err << line << '\n';

    proc.close();
    if (proc.rdbuf()->exited())
        return tuple<int,string, string>(proc.rdbuf()->status(), out.str(), err.str());
}

