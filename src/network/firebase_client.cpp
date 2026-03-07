#include "network/firebase_client.h"
#include <curl/curl.h>
#include <iostream>
#include <sstream>

// ---- CURL write callback for normal requests ----
static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* response = static_cast<std::string*>(userdata);
    size_t totalBytes = size * nmemb;
    response->append(ptr, totalBytes);
    return totalBytes;
}

// ---- SSE context for streaming callback ----
struct SSEContext {
    std::atomic<bool>* running;
    FirebaseClient::SSECallback callback;
    std::string buffer;
    std::string currentEvent;
};

// ---- CURL write callback for SSE streaming ----
static size_t sseWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<SSEContext*>(userdata);
    if (!ctx->running->load()) return 0; // Returning 0 aborts the transfer

    size_t totalBytes = size * nmemb;
    ctx->buffer.append(ptr, totalBytes);

    // Process complete lines from the buffer
    size_t pos;
    while ((pos = ctx->buffer.find('\n')) != std::string::npos) {
        std::string line = ctx->buffer.substr(0, pos);
        ctx->buffer.erase(0, pos + 1);

        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Parse SSE format
        if (line.empty()) {
            // Empty line — end of event (reset for next event)
            ctx->currentEvent.clear();
            continue;
        }

        if (line.rfind("event:", 0) == 0) {
            // "event: put" or "event: patch"
            ctx->currentEvent = line.substr(6);
            // Trim leading whitespace
            size_t start = ctx->currentEvent.find_first_not_of(" \t");
            if (start != std::string::npos) {
                ctx->currentEvent = ctx->currentEvent.substr(start);
            }
        } else if (line.rfind("data:", 0) == 0) {
            std::string dataStr = line.substr(5);
            // Trim leading whitespace
            size_t start = dataStr.find_first_not_of(" \t");
            if (start != std::string::npos) {
                dataStr = dataStr.substr(start);
            }

            // Skip keep-alive or null data
            if (dataStr.empty() || dataStr == "null") continue;

            try {
                json parsed = json::parse(dataStr);
                std::string path = parsed.value("path", "/");
                json data = parsed.value("data", json());

                std::string eventType = ctx->currentEvent.empty() ? "put" : ctx->currentEvent;

                if (ctx->callback) {
                    ctx->callback(eventType, path, data);
                }
            } catch (const json::parse_error&) {
                // Malformed JSON data — skip
            }
        }
        // Ignore lines starting with ":" (comments/keep-alive)
    }

    return totalBytes;
}

// ---- CURL progress callback to allow cancellation of SSE ----
static int sseProgressCallback(void* userdata, curl_off_t /*dltotal*/, curl_off_t /*dlnow*/,
                                curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    auto* running = static_cast<std::atomic<bool>*>(userdata);
    return running->load() ? 0 : 1; // Return non-zero to abort
}

// ---- FirebaseClient implementation ----

FirebaseClient::FirebaseClient() = default;

FirebaseClient::~FirebaseClient() {
    stopListening();
}

void FirebaseClient::init(const std::string& baseUrl) {
    m_baseUrl = baseUrl;
    // Remove trailing slash if present
    if (!m_baseUrl.empty() && m_baseUrl.back() == '/') {
        m_baseUrl.pop_back();
    }
    m_initialized = true;
}

std::string FirebaseClient::buildUrl(const std::string& path) const {
    if (path.empty()) {
        return m_baseUrl + "/.json";
    }
    return m_baseUrl + "/" + path + ".json";
}

std::string FirebaseClient::httpRequest(const std::string& method, const std::string& url,
                                         const std::string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[Firebase] Failed to init CURL" << std::endl;
        return "";
    }

    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    // Set Content-Type header
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set the HTTP method and body
    if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "[Firebase] CURL error (" << method << " " << url << "): "
                  << curl_easy_strerror(res) << std::endl;
        response.clear();
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return response;
}

json FirebaseClient::get(const std::string& path) {
    if (!m_initialized) return json();

    std::string url = buildUrl(path);
    std::string response = httpRequest("GET", url);

    if (response.empty()) return json();

    try {
        json result = json::parse(response);
        return result;
    } catch (const json::parse_error& e) {
        std::cerr << "[Firebase] JSON parse error on GET " << path << ": " << e.what() << std::endl;
        return json();
    }
}

bool FirebaseClient::put(const std::string& path, const json& data) {
    if (!m_initialized) return false;

    std::string url = buildUrl(path);
    std::string body = data.dump();
    std::string response = httpRequest("PUT", url, body);

    if (response.empty()) return false;

    try {
        json::parse(response);
        return true;
    } catch (const json::parse_error&) {
        return false;
    }
}

bool FirebaseClient::patch(const std::string& path, const json& data) {
    if (!m_initialized) return false;

    std::string url = buildUrl(path);
    std::string body = data.dump();
    std::string response = httpRequest("PATCH", url, body);

    if (response.empty()) return false;

    try {
        json::parse(response);
        return true;
    } catch (const json::parse_error&) {
        return false;
    }
}

bool FirebaseClient::del(const std::string& path) {
    if (!m_initialized) return false;

    std::string url = buildUrl(path);
    std::string response = httpRequest("DELETE", url);

    // Firebase returns "null" on successful delete
    return true;
}

std::string FirebaseClient::push(const std::string& path, const json& data) {
    if (!m_initialized) return "";

    std::string url = buildUrl(path);
    std::string body = data.dump();
    std::string response = httpRequest("POST", url, body);

    if (response.empty()) return "";

    try {
        json result = json::parse(response);
        if (result.contains("name")) {
            return result["name"].get<std::string>();
        }
    } catch (const json::parse_error& e) {
        std::cerr << "[Firebase] JSON parse error on POST " << path << ": " << e.what() << std::endl;
    }

    return "";
}

void FirebaseClient::startListening(const std::string& path, SSECallback callback) {
    if (!m_initialized) return;

    // Stop any existing listener first
    stopListening();

    m_ssePath = path;
    m_sseCallback = callback;
    m_sseRunning.store(true);

    std::string url = buildUrl(path);
    m_sseThread = std::thread(&FirebaseClient::sseThreadFunc, this, url, callback);
}

void FirebaseClient::stopListening() {
    m_sseRunning.store(false);
    if (m_sseThread.joinable()) {
        m_sseThread.join();
    }
}

void FirebaseClient::sseThreadFunc(const std::string& url, SSECallback callback) {
    while (m_sseRunning.load()) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::cerr << "[Firebase] SSE: Failed to init CURL" << std::endl;
            // Wait before retrying
            for (int i = 0; i < 50 && m_sseRunning.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            continue;
        }

        SSEContext ctx;
        ctx.running = &m_sseRunning;
        ctx.callback = callback;

        // Set up headers for SSE
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Accept: text/event-stream");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sseWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L); // No timeout — long-lived connection
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects

        // Use progress callback to allow cancellation
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, sseProgressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &m_sseRunning);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

        // Low-speed limit to detect dead connections (at least 1 byte per 60 seconds)
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK && res != CURLE_ABORTED_BY_CALLBACK) {
            std::cerr << "[Firebase] SSE connection error: "
                      << curl_easy_strerror(res) << std::endl;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        // If we're still supposed to be running, wait before reconnecting
        if (m_sseRunning.load()) {
            std::cerr << "[Firebase] SSE: Reconnecting in 3 seconds..." << std::endl;
            for (int i = 0; i < 30 && m_sseRunning.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
}
