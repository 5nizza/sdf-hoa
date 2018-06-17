//
// Created by ayrat on 31/05/18.
//

#pragma once

#include <memory>
#include <iostream>
#include <cstdio>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include "myassert.hpp"


inline std::vector<std::string> split_by_space(const std::string& s)
{
    std::istringstream iss(s);
    std::string token;
    std::vector<std::string> tokens;
    while (iss >> token)
        tokens.push_back(token);
    return tokens;
}


template<typename T>
inline
std::string
join(const std::string& sep, const std::vector<T>& elements)
{
    std::stringstream ss;
    uint i = 1;
    for (const auto& e: elements) {
        ss << e;
        if (i != elements.size())
            ss << sep;
        i++;
    }
    return ss.str();
}


//https://stackoverflow.com/a/116220/801203
inline std::string readfile(const std::string &file_name)
{
    std::fstream in(file_name);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}


//https://stackoverflow.com/a/3418285/801203
inline std::string substitute(const std::string& str,
                       const std::string& from, const std::string& to)
{   // substitutes the first occurrence
    std::string result(str);

    std::size_t start_pos = result.find(from);
    if(start_pos == std::string::npos)
        return result;

    result.replace(start_pos, from.length(), to);
    return result;
}


//https://stackoverflow.com/a/3418285/801203
inline std::string substituteAll(const std::string& str, const std::string& from, const std::string& to)
{
    std::string result(str);
    MASSERT(!from.empty(), "`from` is empty: '" << from << "', '" << to << "'");

    std::size_t start_pos = 0;
    while((start_pos = result.find(from, start_pos)) != std::string::npos) {
        result.replace(start_pos, from.length(), to);
        start_pos += to.length();  // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
    return result;
}


//https://stackoverflow.com/a/26221725/801203
template<typename ... Args>
inline
std::string string_format(const std::string& format, Args ... args)
{
    size_t size = (size_t)snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    std::unique_ptr<char[]> buf( new char[ size ] );
    snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}


template<typename T>
inline
bool contains(const std::vector<T>& elements, const T& elem)
{
    return std::find(elements.begin(), elements.end(), elem) != elements.end();
}


/* Execute the command, return <rc, stdout, stderr>. */
std::tuple<int, std::string, std::string> execute(const char* cmd);
std::tuple<int, std::string, std::string> execute(const std::string& cmd);


/**
 * Similar to python's range.
 * @return [min, min+1, ..., max_excluded)
 */
template <typename T>
inline
std::vector<T> range(const T& min, const T& max_excluded)  // T to be able to use with int and uint, etc.
{
    std::vector<T> numbers;
    for (auto i = min; i < max_excluded; ++i)
        numbers.push_back(i);
    return numbers;
}
