#include "game/save_system.h"
#include "game/root_state.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>
#include <dirent.h>
#include <cerrno>

using json = nlohmann::json;

SaveSystem::SaveSystem() {
    // Determine saves directory: ~/.ascii3d/saves/
    const char* home = std::getenv("HOME");
    if (home) {
        m_savesDir = std::string(home) + "/.ascii3d/saves";
    } else {
        m_savesDir = ".ascii3d/saves";
    }
}

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

bool SaveSystem::ensureDirectory() {
    mkdirRecursive(m_savesDir);
    
    // Verify it exists
    struct stat st;
    return stat(m_savesDir.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool SaveSystem::save(const RootState& state) {
    if (!ensureDirectory()) return false;
    
    std::string filename = makeFilename(state.metadata.name);
    std::string path = m_savesDir + "/" + filename;
    
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;
    
    try {
        json j = state;
        std::vector<uint8_t> v = json::to_cbor(j);
        out.write(reinterpret_cast<const char*>(v.data()), v.size());
    } catch (...) {
        return false;
    }
    
    return out.good();
}

bool SaveSystem::load(const std::string& filename, RootState& outState) {
    std::string path = m_savesDir + "/" + filename;
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    
    try {
        std::vector<uint8_t> v((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        json j = json::from_cbor(v);
        outState = j.get<RootState>();
    } catch (...) {
        return false;
    }
    
    return true;
}

bool SaveSystem::deleteSave(const std::string& filename) {
    std::string path = m_savesDir + "/" + filename;
    return std::remove(path.c_str()) == 0;
}

std::vector<SaveSummary> SaveSystem::listSaves() {
    std::vector<SaveSummary> summaries;
    
    DIR* dir = opendir(m_savesDir.c_str());
    if (!dir) return summaries;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        // Only load .sav files
        if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".sav") {
            RootState state;
            if (load(filename, state)) {
                SaveSummary summary;
                summary.name = state.metadata.name;
                summary.seed = state.world.seed;
                summary.timestamp = state.metadata.timestamp;
                summary.filename = filename;
                summaries.push_back(summary);
            }
        }
    }
    closedir(dir);
    
    // Sort by timestamp, newest first
    std::sort(summaries.begin(), summaries.end(), [](const SaveSummary& a, const SaveSummary& b) {
        return a.timestamp > b.timestamp;
    });
    
    return summaries;
}

std::string SaveSystem::makeFilename(const std::string& worldName) {
    std::string filename;
    for (char c : worldName) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
            (c >= '0' && c <= '9') || c == '_' || c == '-') {
            filename += c;
        } else if (c == ' ') {
            filename += '_';
        }
    }
    if (filename.empty()) filename = "world";
    filename += ".sav";
    return filename;
}
