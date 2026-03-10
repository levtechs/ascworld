#include "game/firebase_client.h"
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <array>

std::string FirebaseClient::s_baseUrl = "https://ascii3d-default-rtdb.firebaseio.com/";

void FirebaseClient::setBaseUrl(const std::string& url) {
    s_baseUrl = url;
    if (!s_baseUrl.empty() && s_baseUrl.back() != '/') {
        s_baseUrl += '/';
    }
}

nlohmann::json FirebaseClient::get(const std::string& path) {
    std::string res = executeCurl("GET", path);
    try {
        return nlohmann::json::parse(res);
    } catch (...) {
        return nlohmann::json();
    }
}

bool FirebaseClient::put(const std::string& path, const nlohmann::json& data) {
    std::string res = executeCurl("PUT", path, data.dump());
    return !res.empty() && res != "null";
}

bool FirebaseClient::patch(const std::string& path, const nlohmann::json& data) {
    std::string res = executeCurl("PATCH", path, data.dump());
    return !res.empty() && res != "null";
}

bool FirebaseClient::post(const std::string& path, const nlohmann::json& data) {
    std::string res = executeCurl("POST", path, data.dump());
    return !res.empty() && res != "null";
}

bool FirebaseClient::remove(const std::string& path) {
    std::string res = executeCurl("DELETE", path);
    return true; // DELETE usually returns null or empty on success
}

std::string FirebaseClient::executeCurl(const std::string& method, const std::string& path, const std::string& data) {
    std::string url = s_baseUrl + path + ".json";
    std::string cmd = "curl -s --connect-timeout 2 --max-time 3 -X " + method + " \"" + url + "\"";
    if (!data.empty()) {
        // Simple escaping for JSON data in shell
        std::string escapedData = data;
        size_t pos = 0;
        while ((pos = escapedData.find("\"", pos)) != std::string::npos) {
            escapedData.replace(pos, 1, "\\\"");
            pos += 2;
        }
        cmd += " -d \"" + escapedData + "\"";
    }

    std::array<char, 128> buffer;
    std::string result;
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return "";
    while (fgets(buffer.data(), buffer.size(), fp) != nullptr) {
        result += buffer.data();
    }
    pclose(fp);
    return result;
}
