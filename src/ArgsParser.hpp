//
// Created by ayrat on 04/06/17.
// From: https://stackoverflow.com/a/868894/801203
//

#ifndef SDF_ARGSPARSER_H
#define SDF_ARGSPARSER_H


#include <algorithm>


class ArgsParser {
public:
    ArgsParser(int argc, char **argv, int start=1) {
        /// start: from which token shall we start parsing?
        /// by default, we skip the first one (program name),
        /// but if you want to have positional args (which must go first),
        /// then use `start` to skip them
        for (int i = start; i < argc; ++i)
            this->tokens.push_back(std::string(argv[i]));
    }

    const std::string &getCmdOption(const std::string &option) const {
        std::vector<std::string>::const_iterator itr;
        itr = std::find(this->tokens.begin(), this->tokens.end(), option);
        if (itr != this->tokens.end() && ++itr != this->tokens.end()) {
            return *itr;
        }
        static const std::string empty_string("");
        return empty_string;
    }

    bool cmdOptionExists(const std::string &option) const {
        return std::find(this->tokens.begin(), this->tokens.end(), option)
               != this->tokens.end();
    }

private:
    std::vector<std::string> tokens;
};


#endif //SDF_ARGSPARSER_H
