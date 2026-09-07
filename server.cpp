#include "server.hpp"
#include "httplib.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <csignal>
#include <stdexcept>
#include <thread>

KeyValue kv;
std::atomic<bool> shutdownRequested(false);

void handleSignal(int) { shutdownRequested.store(true); }

int main() {
    httplib::Server svr;
    svr.new_task_queue = [] { return new httplib::ThreadPool(100); };

    std::signal(SIGTERM, handleSignal);
    std::signal(SIGINT, handleSignal);

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
    svr.Put("/kv/:key", [](const httplib::Request &req, httplib::Response &res) {
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

    svr.Get("/kv/:key", [](const httplib::Request &req, httplib::Response &res) {
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

    svr.Delete("/kv/:key", [](const httplib::Request &req, httplib::Response &res) {
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

    svr.Delete("/clear", [](const httplib::Request &, httplib::Response &res) {
        kv.clear();
        res.status = 200;
        res.set_content("ok", "text/plain");
    });

    svr.Get("/health", [](const httplib::Request &, httplib::Response &res) {
        res.status = 200;
        res.set_content("ok", "text/plain");
    });

    kv.load();
    // we run in a seperate thread, so when we do .join, it waits gracefully for everything to
    // complete and then it joins it back.
    std::thread serverThread([&svr]() { svr.listen("0.0.0.0", 8080); });

    svr.wait_until_ready();

    while (!shutdownRequested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    svr.stop();
    serverThread.join();

    kv.save();
    return 0;
}
