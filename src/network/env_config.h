#pragma once
#include <string>
#include <unordered_map>
#include <fstream>
#include <algorithm>

class EnvConfig {
public:
    // Load from a .env file. Tries multiple locations.
    // Returns false if file not found in any location.
    static bool load(const std::string& filepath = ".env") {
        // Try the given path first (usually CWD/.env)
        std::ifstream file(filepath);

        // Fallback: try ~/.ascii3d/.env
        if (!file.is_open()) {
            const char* home = std::getenv("HOME");
            if (home) {
                file.open(std::string(home) + "/.ascii3d/.env");
            }
        }

        if (!file.is_open()) return false;
        
        std::string line;
        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;
            
            auto eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;
            
            std::string key = line.substr(0, eqPos);
            std::string value = line.substr(eqPos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t\r\n"));
            key.erase(key.find_last_not_of(" \t\r\n") + 1);
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            
            // Remove quotes if present
            if (value.size() >= 2) {
                if ((value.front() == '"' && value.back() == '"') ||
                    (value.front() == '\'' && value.back() == '\'')) {
                    value = value.substr(1, value.size() - 2);
                }
            }
            
            s_values[key] = value;
        }
        return true;
    }
    
    // Get a value by key, returns empty string if not found
    static std::string get(const std::string& key, const std::string& defaultValue = "") {
        auto it = s_values.find(key);
        if (it != s_values.end()) return it->second;
        
        // Fall back to environment variable
        const char* env = std::getenv(key.c_str());
        if (env) return std::string(env);
        
        return defaultValue;
    }
    
private:
    static inline std::unordered_map<std::string, std::string> s_values;
};
