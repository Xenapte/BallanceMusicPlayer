#pragma once
#include <string>
#include <map>
#include <fstream>
#include <filesystem>
#include "Common.hpp"

class SecondaryConfig {
public:
    static void Save(const std::string& filepath, const std::map<std::string, std::string>& data) {
        std::ofstream file(Utf8ToPath(filepath));
        if (!file.is_open()) return;
        for (const auto& [key, val] : data) {
            file << key << "=" << val << "\n";
        }
    }

    static std::map<std::string, std::string> Load(const std::string& filepath) {
        std::map<std::string, std::string> data;
        std::ifstream file(Utf8ToPath(filepath));
        if (!file.is_open()) return data;
        std::string line;
        while (std::getline(file, line)) {
            // Trim inline comments starting with '#'
            size_t commentPos = line.find('#');
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }
            
            size_t pos = line.find('=');
            if (pos == std::string::npos) continue;
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            
            // Trim whitespace/carriage returns
            key.erase(0, key.find_first_not_of(" \t\r\n"));
            key.erase(key.find_last_not_of(" \t\r\n") + 1);
            val.erase(0, val.find_first_not_of(" \t\r\n"));
            val.erase(val.find_last_not_of(" \t\r\n") + 1);
            
            if (!key.empty()) {
                data[key] = val;
            }
        }
        return data;
    }
};
