#include "raft.hpp"
#include "../third_party/nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <future>
#include "../api/httplib.h"
#include <random>
#include <thread>
#include <utility>

using json = nlohmann::json;

static std::vector<httplib::Result> sendRPCs(const std::vector<std::string> &peers,
                                          const std::string &path, const json &body) {
    std::vector<std::future<httplib::Result>> requests;
    const auto payload = body.dump();
    for (const auto &peer : peers) {
        requests.push_back(std::async(std::launch::async, [peer, path, payload] {
            httplib::Client cli("http://" + peer);
            cli.set_connection_timeout(0, 100000);
            cli.set_read_timeout(0, 100000);
            cli.set_write_timeout(0, 100000);
            return cli.Post(path, payload, "application/json");
        }));
    }
    std::vector<httplib::Result> results;
    for (auto &request : requests)
        results.push_back(request.get());
    return results;
}


RaftNode::RaftNode(std::string nodeID, std::vector<std::string> nodePeers)
    : id(nodeID), peers(nodePeers) {
    std::ifstream inputFile(raft_state_file);
    if (!inputFile.is_open())
        return;

    auto map = json::parse(inputFile);

    auto term = map.find("currentTerm");
    if (term != map.end() && *term != "")
        currentTerm = term->is_string() ? std::stoull(term->get<std::string>())
                                       : term->get<std::size_t>();

    auto vote = map.find("votedFor");
    if (vote != map.end())
        votedFor = vote->get<std::string>();

}

void RaftNode::start() {
    std::unique_lock<std::mutex> lock(state_mutex);
    last_append_entries = std::chrono::steady_clock::now();
    auto election_deadline = last_append_entries;
    std::mt19937_64 eng{std::random_device{}()};
    std::uniform_int_distribution<> dist{500, 1000};
    timeout = std::chrono::milliseconds{dist(eng)};
    // if commitNode > lastApplied, this node needs to catch up.
    while (!stopped) {
        if (RaftNode::state == CANDIDATE) {
            if (std::chrono::steady_clock::now() < election_deadline) {
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                lock.lock();
                continue;
            }
            // Starting election
            ++currentTerm;
            votedFor = id;
            // TODO: Persist election state
            bool electionSuccess = false;
            timeout = std::chrono::milliseconds{dist(eng)};
            election_deadline = std::chrono::steady_clock::now() + timeout;

            json body;
            body["term"] = currentTerm;
            body["candidateId"] = id;
            body["lastLogIndex"] = log.size();
            body["lastLogTerm"] = log.empty() ? 0 : log.back().term;

            size_t votes = 1;
            const auto electionTerm = currentTerm;
            lock.unlock();
            auto results = sendRPCs(peers, "/raft/request-vote", body);
            lock.lock();
            for (auto &res : results) {
                if (stopped || state != CANDIDATE || currentTerm != electionTerm)
                    break;
                if(res){
                    const auto response = json::parse(res->body);
                    auto responseTerm = response.at("term").get<std::size_t>();
                    if (responseTerm > currentTerm) {
                        currentTerm = responseTerm;
                        votedFor.clear();
                        state = FOLLOWER;
                        leaderID.clear();
                        // TODO: Persist updated state
                        break;
                    }
                    votes += responseTerm == electionTerm && response.at("voteGranted").get<bool>();
                
                    if (votes >= (peers.size()+1)/2 + 1){
                        state = LEADER;
                        leaderID = id;
                        break;
                    }
                }
            }

        }
        if (RaftNode::state == LEADER) {
            // send out heartbeats, handle replication?
            auto last_quorum = std::chrono::steady_clock::now();
            while (!stopped && state == LEADER) {
                const auto heartbeatTerm = currentTerm;
                size_t responses = 1;
                json body;
                body["term"] = currentTerm;
                body["leaderId"] = leaderID;
                body["prevLogIndex"] = log.size();
                body["prevLogTerm"] = log.empty() ? 0 : log.back().term;
                body["entries"] = json::array();
                body["leaderCommit"] = commitIndex;
                
                lock.unlock();
                auto results = sendRPCs(peers, "/raft/append-entries", body);
                lock.lock();
                for (auto &res : results) {
                    if (stopped || state != LEADER || currentTerm != heartbeatTerm)
                        break;
                    if (res && res->status == 200) {
                        const auto response = json::parse(res->body);
                        auto responseTerm = response.at("term").get<std::size_t>();
                        if (responseTerm > currentTerm) {
                            currentTerm = responseTerm;
                            votedFor.clear();
                            state = FOLLOWER;
                            leaderID.clear();
                            break;
                        }
                        responses += responseTerm == heartbeatTerm;
                    }
                }
                if (stopped || state != LEADER || currentTerm != heartbeatTerm)
                    break;
                auto now = std::chrono::steady_clock::now();
                if (responses >= (peers.size() + 1) / 2 + 1) {
                    last_quorum = now;
                } else if (now - last_quorum >= std::chrono::milliseconds(500)) {
                    state = FOLLOWER;
                    leaderID.clear();
                    last_append_entries = now;
                    break;
                }
                auto delay = heartbeat_period;
                lock.unlock();
                std::this_thread::sleep_for(delay);
                lock.lock();
            }
        } else if (RaftNode::state == FOLLOWER) {
            // check for heartbeat
            auto now = std::chrono::steady_clock::now();
            if (now - last_append_entries >= timeout) {
                state = CANDIDATE;
                leaderID = "";
            }
        }
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        lock.lock();
    }
}

void RaftNode::stop() { stopped = true; }

std::pair<State, std::string> RaftNode::leaderInfo() {
    std::lock_guard<std::mutex> lock(state_mutex);
    return {state, leaderID};
}

std::string RaftNode::clusterInfo() {
    std::lock_guard<std::mutex> lock(state_mutex);
    const char *role = state == LEADER ? "leader"
                     : state == CANDIDATE ? "candidate" : "follower";
    json leader = state == LEADER ? id : leaderID;
    if (leader == "") leader = nullptr;
    return json{{"id", id},
                {"role", role},
                {"term", currentTerm},
                {"leader", leader},
                {"peers", peers}}.dump();
}

std::pair<size_t, bool> RaftNode::appendEntries(size_t term, const std::string &leader_id,
                                      size_t prev_log_index, size_t prev_log_term,
                                      const std::string &entries, size_t leader_commit) {
    std::lock_guard<std::mutex> lock(state_mutex);
    if (term < currentTerm)
        return {currentTerm, false};

    if (term > currentTerm) {
        currentTerm = term;
        votedFor.clear();
        // TODO: Persist updated state
    }

    state = FOLLOWER;
    leaderID = leader_id;
    last_append_entries = std::chrono::steady_clock::now();

    // add to log / update data once entries are implemented 
    return {currentTerm,
            prev_log_index == 0 ||
                (prev_log_index <= log.size() &&
                 log[prev_log_index - 1].term == prev_log_term)};
}

std::pair<size_t, bool> RaftNode::requestVote(size_t term, const std::string &candidate_id,
                                    size_t last_log_index, size_t last_log_term) {
    std::lock_guard<std::mutex> lock(state_mutex);
    if (term < currentTerm)
        return {currentTerm, false};

    if (term > currentTerm) {
        currentTerm = term;
        votedFor.clear();
        state = FOLLOWER;
        leaderID.clear();
        // TODO: Persist updated state
    }

    auto local_log_term = log.empty() ? 0 : log.back().term;
    bool grant = (votedFor.empty() || votedFor == candidate_id) &&
                 (last_log_term > local_log_term ||
                  (last_log_term == local_log_term && last_log_index >= log.size()));
    if (grant) {
        votedFor = candidate_id;
        last_append_entries = std::chrono::steady_clock::now();
        // TODO: Persist granted vote
    }
    return {currentTerm, grant};
}
