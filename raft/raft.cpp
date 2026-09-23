#include "raft.hpp"
#include "../helper/jsonhandler.hpp"

#include <filesystem>
#include <fstream>
#include "../api/httplib.h"
#include <random>
#include <sstream>
#include <thread>
#include <utility>


// chrono last append entry when


RaftNode::RaftNode(std::string nodeID, std::vector<std::string> nodePeers)
    : id(nodeID), peers(nodePeers) {
    std::ifstream inputFile(raft_state_file);
    if (!inputFile.is_open())
        return;

    std::stringstream buffer;
    buffer << inputFile.rdbuf();
    auto map = parseJSONToMap(buffer.str());

    auto term = map.find("currentTerm");
    if (term != map.end() && !term->second.empty())
        currentTerm = std::stoull(term->second);

    auto vote = map.find("votedFor");
    if (vote != map.end())
        votedFor = vote->second;

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

            std::unordered_map<std::string, std::string> body; 
            body["term"] = currentTerm;
            body["candidateId"] = id;
            body["lastLogIndex"] = std::to_string(log.size());
            body["lastLogTerm"] = std::to_string(log.empty() ? 0 : log.back().term);

            size_t votes = 1;
            const auto electionTerm = currentTerm;
            for(auto peer : peers) {
                httplib::Client cli("http://" + peer + ":8080");            
                lock.unlock();
                auto res = cli.Post("/raft/request-vote", parseMapToJSON(body), "application/json");
                lock.lock();
                if (stopped || state != CANDIDATE || currentTerm != electionTerm)
                    break;
                if(res){
                    const auto response = parseJSONToMap(res->body);
                    auto responseTerm = std::stoull(response.at("term"));
                    if (responseTerm > currentTerm) {
                        currentTerm = responseTerm;
                        votedFor.clear();
                        state = FOLLOWER;
                        leaderID.clear();
                        // TODO: Persist updated state
                        break;
                    }
                    votes += responseTerm == electionTerm && response.at("voteGranted") == "true";
                
                    if (votes >= (peers.size()+1)/2 + 1){
                        state = LEADER;
                        break;
                    }
                }
            }

        } else if (RaftNode::state == LEADER) {
            // send out heartbeats, handle replication?
            while (!stopped && state == LEADER) {
                const auto heartbeatTerm = currentTerm;
                std::unordered_map<std::string, std::string> body;
                body["term"] = currentTerm;
                body["leaderId"] = leaderID;
                body["prevLogIndex"] = std::to_string(log.size());
                body["prevLogTerm"] = std::to_string(log.empty() ? 0 : log.back().term);
                body["entries"] = "";
                body["leaderCommit"] = commitIndex;
                
                for(auto peer : peers) {
                    httplib::Client cli("http://" + peer + ":8080");            
                    lock.unlock();
                    auto res = cli.Post("/raft/append-entries", parseMapToJSON(body), "application/json");
                    lock.lock();
                    if (stopped || state != LEADER || currentTerm != heartbeatTerm)
                        break;
                    if(res){
                        // actions to be done later when entries are implemented 
                    }
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
            }
        }
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        lock.lock();
    }
}

void RaftNode::stop() { stopped = true; }

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
