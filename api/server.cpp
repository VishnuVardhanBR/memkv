#include "server.hpp"
#include "../helper/jsonhandler.hpp"
#include "../raft/raft.hpp"
#include "../store/store.hpp"
#include "httplib.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

KeyValue kv;
std::atomic<bool> shutdownRequested(false);

void handleSignal(int) { shutdownRequested.store(true); }

int main() {
    httplib::Server svr;
    svr.new_task_queue = [] { return new httplib::ThreadPool(100); };

    // read env variables and assign id and peers to raft
    const auto addrEnv = std::getenv("ADDR");
    std::string id = addrEnv ? addrEnv : "";

    std::vector<std::string> peers;
    if (const auto peersEnv = std::getenv("PEERS")) {
        std::stringstream peerStream(peersEnv);
        std::string peer;
        while (std::getline(peerStream, peer, ',')) {
            if (!peer.empty())
                peers.push_back(peer);
        }
    }

    RaftNode raft(id, peers);

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

    svr.Post("/raft/request-vote", [&raft](const httplib::Request &req, httplib::Response &res) {
        auto body = parseJSONToMap(req.body);
        auto candidate_term = std::stoull(body.at("term"));
        auto candidate_id = body.at("candidateId");
        auto last_log_index = std::stoull(body.at("lastLogIndex"));
        auto last_log_term = std::stoull(body.at("lastLogTerm"));

        auto [term, voteGranted] =
            raft.requestVote(candidate_term, candidate_id, last_log_index, last_log_term);
        res.status = 200;
        res.set_content("{\"term\":" + std::to_string(term) +
                            ",\"voteGranted\":" + (voteGranted ? "true" : "false") + "}",
                        "application/json");
    });

    svr.Post("/raft/append-entries", [&raft](const httplib::Request &req, httplib::Response &res) {
        auto body = parseJSONToMap(req.body);
        auto leader_term = std::stoull(body.at("term"));
        auto leader_id = body.at("leaderId");
        auto prev_log_index = std::stoull(body.at("prevLogIndex"));
        auto prev_log_term = std::stoull(body.at("prevLogTerm"));
        auto entries = body.at("entries");
        auto leader_commit = std::stoull(body.at("leaderCommit"));

        auto [term, success] = raft.appendEntries(leader_term, leader_id, prev_log_index,
                                                  prev_log_term, entries, leader_commit);
        res.status = 200;
        res.set_content("{\"term\":" + std::to_string(term) +
                            ",\"success\":" + (success ? "true" : "false") + "}",
                        "application/json");
    });

    kv.load();
    // we run in a seperate thread, so when we do .join, it waits gracefully for everything to
    // complete and then it joins it back.
    std::thread serverThread([&svr]() { svr.listen("0.0.0.0", 8080); });

    svr.wait_until_ready();
    std::thread raftThread([&raft]() { raft.start(); });

    while (!shutdownRequested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    raft.stop();
    svr.stop();
    serverThread.join();
    raftThread.join();

    kv.save();
    return 0;
}
