#include "game/game_session.h"
#include "game/firebase_client.h"
#include <rtc/rtc.hpp>
#include <random>
#include <chrono>
#include <iostream>
#include <csignal>
#include <sys/stat.h>
#include <cerrno>

extern void installSignalHandlers();

// Recursively create directories (like mkdir -p)
static bool mkdirRecursive(const std::string& path) {
    if (path.empty()) return false;
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return true;
    size_t pos = path.rfind('/');
    if (pos != std::string::npos && pos > 0) {
        mkdirRecursive(path.substr(0, pos));
    }
    return mkdir(path.c_str(), 0755) == 0 || (errno == EEXIST);
}

static std::string getOrGenerateUUID(const std::string& configDir) {
    std::string path = configDir + "/client_id";
    FILE* f = fopen(path.c_str(), "r");
    if (f) {
        char buf[64];
        if (fgets(buf, sizeof(buf), f)) {
            fclose(f);
            std::string s(buf);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
            if (!s.empty()) return s;
        }
        fclose(f);
    }
    std::string uuid;
    static const char hex[] = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 15);
    for (int i = 0; i < 32; i++) {
        uuid += hex[dist(gen)];
        if (i == 7 || i == 11 || i == 15 || i == 19) uuid += '-';
    }
    mkdirRecursive(configDir);
    f = fopen(path.c_str(), "w");
    if (f) {
        fprintf(f, "%s\n", uuid.c_str());
        fclose(f);
    }
    return uuid;
}

static std::string loadUsername(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return "";
    char buf[64];
    if (fgets(buf, sizeof(buf), f)) {
        fclose(f);
        std::string s(buf);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
        if (s.size() > 16) s = s.substr(0, 16);
        return s;
    }
    fclose(f);
    return "";
}

static void loadEnv(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return;
    char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        std::string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            setenv(key.c_str(), val.c_str(), 1);
        }
    }
    fclose(f);
}

int main() {
    // Environment and logging setup
    loadEnv(".env");
    const char* fbUrl = getenv("FIREBASE_URL");
    if (fbUrl) FirebaseClient::setBaseUrl(fbUrl);

    rtc::InitLogger(rtc::LogLevel::Info);
    freopen("network.log", "w", stderr);
    std::cerr.setf(std::ios::unitbuf);
    std::cerr << "--- Network Log Started ---" << std::endl;

    installSignalHandlers();
    signal(SIGPIPE, SIG_IGN);

    // Config
    bool forceUsernameEntry = false;
    const char* env = getenv("ASCII3D_FORCE_USERNAME_ENTRY");
    if (env && env[0] != '\0' && env[0] != '0') forceUsernameEntry = true;

    std::string configDir = std::string(getenv("HOME")) + "/.ascii3d";
    std::string clientUUID = getOrGenerateUUID(configDir);
    std::string savedUsername = loadUsername(configDir + "/config");
    if (forceUsernameEntry) savedUsername.clear();

    // Create and run the game session
    GameSession session(configDir, clientUUID, savedUsername, forceUsernameEntry);
    session.run();

    return 0;
}
