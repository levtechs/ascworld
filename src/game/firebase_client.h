#pragma once
#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

// Simple Firebase REST client using curl via popen
class FirebaseClient {
public:
    static void setBaseUrl(const std::string& url);

    static nlohmann::json get(const std::string& path);
    static bool put(const std::string& path, const nlohmann::json& data);
    static bool patch(const std::string& path, const nlohmann::json& data);
    static bool post(const std::string& path, const nlohmann::json& data);
    static bool remove(const std::string& path);

private:
    static std::string executeCurl(const std::string& method, const std::string& path, const std::string& data = "");
    static std::string s_baseUrl;
};
