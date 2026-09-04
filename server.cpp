#include "server.hpp"
#include "httplib.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

std::string PERSISTENT_FILE_PATH = "kv.json";

std::string KeyValue::getValue(std::string &key) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = key_value.find(key);
    if (it == key_value.end()) {
        throw std::out_of_range("key not found");
    }
    return it->second;
}

int KeyValue::insert(std::string &key, std::string &value) {
    std::lock_guard<std::mutex> lock(mutex);
    key_value[key] = value;
    return 0;
}

int KeyValue::erase(std::string &key) {
    std::lock_guard<std::mutex> lock(mutex);
    key_value.erase(key);
    return 0;
}

void KeyValue::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    key_value.clear();
}

std::string parseMapToJSON(std::unordered_map<std::string, std::string> &map) {
    std::string JSON;
    JSON.append("{");
    bool first = true;
    for (std::pair<std::string, std::string> p : map) {
        if (!first)
            JSON.append(",");
        JSON.append("\"" + p.first + "\"");
        JSON.append(":");
        JSON.append("\"" + p.second + "\"");
        first = false;
    }
    JSON.append("}");
    return JSON;
}

std::unordered_map<std::string, std::string> parseJSONToMap(std::string json) {
    std::unordered_map<std::string, std::string> map;
    size_t pos = 1;
    while (pos < json.length() - 1) {
        size_t key_start = json.find('"', pos) + 1;
        size_t key_end = json.find('"', key_start);
        std::string key = json.substr(key_start, key_end - key_start);

        size_t value_start = json.find('"', key_end + 1) + 1;
        size_t value_end = json.find('"', value_start);
        std::string value = json.substr(value_start, value_end - value_start);

        map[key] = value;
        pos = value_end + 1;
    }
    return map;
}

// Store map to disk in json format
void saveToDisk(std::unordered_map<std::string, std::string> &map) {
    std::ofstream outputFile(PERSISTENT_FILE_PATH);
    if (!outputFile.is_open()) {
        return;
    }

    outputFile << parseMapToJSON(map);
    outputFile.close();
}

void readFromDisk(std::unordered_map<std::string, std::string> &map) {
    std::ifstream inputFile(PERSISTENT_FILE_PATH);
    if (!inputFile.is_open()) {
        return;
    }

    std::stringstream buffer;
    buffer << inputFile.rdbuf();
    std::string content = buffer.str();
    map = parseJSONToMap(content);
    inputFile.close();
}

int main() {
    httplib::Server svr;
    svr.new_task_queue = [] { return new httplib::ThreadPool(100); };
    KeyValue kv;
    std::atomic<bool> ready{false};

    svr.set_pre_routing_handler([](const httplib::Request &req, httplib::Response &res) {
        bool method_allowed = true;

        if (req.path == "/") {
            method_allowed = (req.method == "GET");
        } else if (req.path == "/health") {
            method_allowed = (req.method == "GET");
        } else if (req.path == "/clear") {
            method_allowed = (req.method == "DELETE");
        } else if (req.path.rfind("/kv/", 0) == 0) {
            method_allowed = (req.method == "GET" || req.method == "PUT" || req.method == "DELETE");
        } else {
            return httplib::Server::HandlerResponse::Unhandled;
        }

        if (!method_allowed) {
            res.status = 405;
            res.set_content("method not allowed", "text/plain");
            return httplib::Server::HandlerResponse::Handled;
        }

        return httplib::Server::HandlerResponse::Unhandled;
    });

    svr.Get("/", [](const httplib::Request &, httplib::Response &res) {
        res.set_content("Hello, World!", "text/plain");
    });

    // Add or update a key-value pair in the store.
    svr.Put("/kv/:key", [&kv](const httplib::Request &req, httplib::Response &res) {
        auto key = req.path_params.at("key");
        auto value = req.body;
        if (key.empty()) {
            res.status = 400;
            res.set_content("key cannot be empty", "text/plain");
            return;
        }
        if (value.empty()) {
            res.status = 400;
            res.set_content("value cannot be empty", "text/plain");
            return;
        }
        if (std::all_of(key.begin(), key.end(), [](unsigned char c) {
                return std::isalnum(c) || c == ':' || c == '_' || c == '.' || c == '-';
            })) {
            kv.insert(key, value);
        } else {
            res.status = 400;
            res.set_content("key must be alphanumeric", "text/plain");
            return;
        }
        res.status = 200;
        res.set_content("ok", "text/plain");
    });

    svr.Get("/kv/:key", [&kv](const httplib::Request &req, httplib::Response &res) {
        auto key = req.path_params.at("key");

        if (key.empty()) {
            res.status = 400;
            res.set_content("key cannot be empty", "text/plain");
            return;
        }

        try {
            auto value = kv.getValue(key);
            res.status = 200;
            res.set_content(value, "text/plain");
        } catch (const std::out_of_range &) {
            res.status = 404;
            res.set_content("key not found", "text/plain");
        }
    });

    svr.Delete("/kv/:key", [&kv](const httplib::Request &req, httplib::Response &res) {
        auto key = req.path_params.at("key");

        if (key.empty()) {
            res.status = 400;
            res.set_content("key cannot be empty", "text/plain");
            return;
        }

        kv.erase(key);
        res.status = 200;
        res.set_content("ok", "text/plain");
    });

    svr.Delete("/clear", [&kv](const httplib::Request &req, httplib::Response &res) {
        kv.clear();
        res.status = 200;
        res.set_content("ok", "text/plain");
    });

    svr.Get("/health", [&ready](const httplib::Request &, httplib::Response &res) {
        if (ready.load()) {
            res.status = 200;
            res.set_content("ok", "text/plain");
        } else {
            res.status = 503;
            res.set_content("not ready", "text/plain");
        }
    });

    ready.store(true);
    svr.listen("0.0.0.0", 8080);
}

// file handling and writing implemented, now need to implement graceful shutdown
