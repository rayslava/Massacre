#pragma once

#include "FileSystem.h"
#include "Debug.h"
#include <bits/stl_map.h>
#include <boost/any.hpp>
#include <boost/lexical_cast.hpp>
#include <string>
#include <map>
#include <iostream>

namespace mcr {

class Config {
    
    static std::map<std::string, std::string> configuration;

private:
    FileSystem* m_fs;
    bool m_ownFs;
    const static int default_value = 0;   
    
struct opbrack_impl {
    const std::string& str;
 
    opbrack_impl(std::string const& str) : str(str) {}
 
    template <class T>
    operator T() const {
            debug("Looking for %s in config", str.c_str());
            if (configuration.find(str) != configuration.end()) {
                debug("Found!");
                return boost::lexical_cast<T>(configuration.find(str)->second);
            }
            else {
                debug("Not found!");
                return boost::lexical_cast<T>(0.0f);            // TODO: Think what to do in this case
            }
    }
};
 
public:
    Config() {};
    ~Config()
    {
        if (m_ownFs)
            delete m_fs;
    }

    void load(const char* filename, FileSystem* fs = nullptr);
    
    opbrack_impl operator[](const std::string& str) {
        return opbrack_impl(str);
    };
};
}