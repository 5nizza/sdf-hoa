#pragma once

#include <memory>
#include <iostream>
#include <cstdio>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <cctype>
#include <locale>
#include <unordered_set>
#include "my_assert.hpp"


// TODO: use Boost

namespace sdf
{


inline
std::string readfile(const std::string& file_name)
{
    //https://stackoverflow.com/a/116220/801203
    std::fstream in(file_name);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}


inline
std::vector<std::string> split_by_space(const std::string& s)
{
    std::istringstream iss(s);
    std::string token;
    std::vector<std::string> tokens;
    while (iss >> token)
        tokens.push_back(token);
    return tokens;
}

template<typename TC>
inline
std::string join(const std::string& sep, const TC& elements)
{
    std::stringstream ss;
    uint i = 1;
    for (const auto& e: elements)
    {
        ss << e;
        if (i != elements.size())
            ss << sep;
        i++;
    }
    return ss.str();
}

template<typename TC, typename ToStr>
inline
std::string join(const std::string& sep, const TC& elements, ToStr convert_to_str)
{
    std::stringstream ss;
    uint i = 1;
    for (const auto& e: elements)
    {
        ss << convert_to_str(e);
        if (i != elements.size())
            ss << sep;
        i++;
    }
    return ss.str();
}


//https://stackoverflow.com/a/3418285/801203
inline
std::string substitute(const std::string& str,
                       const std::string& from, const std::string& to)
{   // substitutes the first occurrence
    std::string result(str);

    std::size_t start_pos = result.find(from);
    if (start_pos == std::string::npos)
        return result;

    result.replace(start_pos, from.length(), to);
    return result;
}


inline
std::string substituteAll(const std::string& str,
                          const std::string& from, const std::string& to)
{
    // https://stackoverflow.com/a/3418285/801203
    std::string result(str);
    MASSERT(!from.empty(), "`from` is empty: '" << from << "', '" << to << "'");

    std::size_t start_pos = 0;
    while ((start_pos = result.find(from, start_pos)) != std::string::npos)
    {
        result.replace(start_pos, from.length(), to);
        start_pos += to.length();  // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
    return result;
}


inline
std::string trim_spaces(const std::string& arg)
{
    // python's strip()
    std::string result = arg;

    result.erase(result.begin(), std::find_if(result.begin(), result.end(), [](int ch) {
        return !std::isspace(ch);
    }));

    result.erase(std::find_if(result.rbegin(), result.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), result.end());

    return result;
}


inline
std::string lower(const std::string& what)
{
    // https://stackoverflow.com/a/313990/801203
    std::string result = what;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

template<typename T>
inline
std::string to_string(const T& obj)
{
    std::stringstream ss;
    ss << obj;
    return ss.str();
}

template<typename ... Args>
inline
std::string string_format(const std::string& format, Args ... args)
{
    // https://stackoverflow.com/a/26221725/801203
    size_t size = (size_t) snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

template<typename T>
inline
bool contains(const std::vector<T>& elements, const T& elem)
{
    return std::find(elements.begin(), elements.end(), elem) != elements.end();
}

template<typename T>
inline
bool contains(const std::set<T>& elements, const T& elem)
{
    return elements.find(elem) != elements.end();
}


/* Execute the command, return <rc, stdout, stderr>. */
std::tuple<int, std::string, std::string> execute(const char *cmd);
std::tuple<int, std::string, std::string> execute(const std::string &cmd);

/**
 * Similar to python's range.
 * @return [min, min+1, ..., max_excluded)
 */
template<typename T>
inline
std::vector<T> range(const T &min, const T &max_excluded)  // T to be able to use with int and uint, etc.
{
    std::vector<T> numbers;
    for (auto i = min; i < max_excluded; ++i)
        numbers.push_back(i);
    return numbers;
}

template<typename E,typename Container>
E pop_first(Container& container)
{
    auto it = container.begin();
    auto result = *it;  // save before it gets invalidated by `erase`
    container.erase(it);
    return result;
}

inline
std::string create_tmp_folder()
{
    std::string path_tmp_str = "/tmp/XXXXXX";
    std::string folder = mkdtemp((char*)path_tmp_str.c_str());  // (char*) is a hack but path_tmp_str lives long enough, so OK
    if (folder.empty())
        throw std::string("failure to create tmp folder");
    return folder;
}


template<typename E, typename Container>
std::set<E>
a_minus_b(const std::set<E>& a, const Container& b)
{
    std::set<E> result = a;
    for (const auto& e : b)
        result.erase(e);
    return result;
}

template<typename E, typename Container>
std::unordered_set<E>
a_minus_b(const std::unordered_set<E>& a, const Container& b)
{
    std::unordered_set<E> result = a;
    for (const auto& e : b)
        result.erase(e);
    return result;
}

template<typename E>
std::vector<E>
a_minus_b(const std::vector<E>& a, const std::unordered_set<E>& b)
{
    std::vector<E> result;
    for (const auto& e : a)
        if (b.count(e) == 0)
            result.push_back(e);
    return result;
}

template<typename Container>
Container
a_intersection_b(const Container& a, const Container& b)
{
    Container result;
    for (const auto& x : a)
        if (b.find(x) != b.end())
            result.insert(x);
    return result;
}

template<typename T>
std::vector<T>
a_union_b(const std::vector<T>& a, const std::vector<T>& b)
{
    std::vector<T> result = a;  // copy
    result.insert(result.end(), b.begin(), b.end());
    return result;
}

template<typename Container>
Container
a_union_b(const Container& a, const Container& b)
{
    Container result = a;  // copy
    result.insert(b.begin(), b.end());
    return result;
}

template<typename Container>
Container
a_union_b(const Container& a, const Container& b, const Container& c)
{
    Container result = a;  // copy
    result.insert(b.begin(), b.end());
    result.insert(c.begin(), c.end());
    return result;
}


//template<typename E, typename IteratorStart, typename IteratorEnd, typename UnaryPred>
//std::vector<E> filter(IteratorStart it1, IteratorEnd it2, UnaryPred pred)
//{
//    std::vector<E> result;
//    std::copy_if(it1, it2, std::back_inserter(result), pred);
//    return result;
//}

template<typename E, typename Container, typename UnaryPred>
std::vector<E> filter(const Container& cont, UnaryPred pred)
{
    std::vector<E> result;
    std::copy_if(cont.begin(), cont.end(), std::back_inserter(result), pred);
    return result;
}

template<typename E, typename Container>
std::vector<std::vector<E>> all_subsets(const Container& elements_, bool skip_empty=false)
{
    std::vector<E> elements(elements_.begin(), elements_.end());  // for indexed access
    std::vector<std::vector<E>> result;

    for (uint mask = skip_empty? 1:0; mask < 1u<<elements.size(); ++mask)
    {
        std::vector<E> subset;
        for (uint idx = 0; idx < elements.size(); ++idx)
        {
            if ((1<<idx & mask) != 0)
                subset.push_back(elements[idx]);
        }
        result.push_back(subset);
    }

    return result;
}


// source: modern c++ programming cookbook, page 254
template<typename OrderedContainer, typename ElementHasher>
size_t hash_ordered(const OrderedContainer& container, ElementHasher hasher)
{
    if (container.empty())
        return 0;

    size_t hash_value = 17;

    hash_value = 31*hash_value + std::hash<size_t>()(container.size());
    for (const auto& e : container)
        hash_value = 31*hash_value + hasher(e);
    return hash_value;
}


// This function XOR's the hash values of the elements
// (hence the order is not important)
template<typename Container, typename ElementHasher>
size_t xor_hash(const Container& container, ElementHasher hasher)
{
    auto hash_value = std::hash<size_t>()(container.size());

    if (container.empty())
        return hash_value;

    for (const auto& e : container)
        hash_value ^= hasher(e);

    return hash_value;
}


template<typename T1, typename T2, typename H1, typename H2>
size_t do_hash_pair(const std::pair<T1,T2>& x, H1 h1, H2 h2)
{
    size_t hash_value = 17;
    hash_value = 31*hash_value + h1(x.first);
    hash_value = 31*hash_value + h2(x.second);
    return hash_value;
}


template<typename T, typename U, typename H1=std::hash<T>, typename H2=std::hash<U>>
struct pair_hash
{
    size_t operator()(const std::pair<T, U>& x) const
    {
        size_t hash_value = 17;
        hash_value = 31*hash_value + H1()(x.first);
        hash_value = 31*hash_value + H2()(x.second);
        return hash_value;
    }
};

//struct pair_hash
//{
//    template<typename T, typename U>
//    size_t operator()(const std::pair<T, U>& x) const
//    {
//        size_t hash_value = 17;
//        hash_value = 31*hash_value + std::hash<T>()(x.first);
//        hash_value = 31*hash_value + std::hash<U>()(x.second);
//        return hash_value;
//    }
//};


template<typename T>
std::vector<T> to_vector(const std::set<T>& c)
{
    return std::vector<T>(c.begin(), c.end());
}

template<typename T>
std::vector<T> to_vector(const std::unordered_set<T>& c)
{
    return std::vector<T>(c.begin(), c.end());
}

template<typename Container>
std::vector<typename Container::key_type> keys(const Container& container)
{
    std::vector<typename Container::key_type> result;
    for (const auto& [k,v] : container)
        result.push_back(k);
    return result;
}

template<typename Container>
std::unordered_set<typename Container::key_type> keysSet(const Container& container)
{
    std::unordered_set<typename Container::key_type> result;
    for (const auto& [k,v] : container)
        result.insert(k);
    return result;
}



}  // namespace sdf
