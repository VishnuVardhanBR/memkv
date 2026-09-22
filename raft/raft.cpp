#include "raft.hpp"
#include "../helper/jsonhandler.hpp"

#include <filesystem>
#include <fstream>
#include <httplib.h>
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
    
    last_append_entries = std::chrono::steady_clock::now();
    auto election_deadline = last_append_entries;
    std::mt19937_64 eng{std::random_device{}()};
    std::uniform_int_distribution<> dist{500, 1000};
    timeout = std::chrono::milliseconds{dist(eng)};
    // if commitNode > lastApplied, this node needs to catch up.
    while(true) {
        if (RaftNode::state == CANDIDATE) {
            if (std::chrono::steady_clock::now() < election_deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            // Starting election
            ++currentTerm;
            votedFor = id;
            // TODO: Persist election state
            timeout = std::chrono::milliseconds{dist(eng)};
            election_deadline = std::chrono::steady_clock::now() + timeout;

            std::unordered_map<std::string, std::string> body; 
            body["term"] = currentTerm;
            body["candidateId"] = id;
            body["lastLogIndex"] = std::to_string(log.size());
            body["lastLogTerm"] = std::to_string(log.back().term-1);

            size_t votes = 1;
            for(auto peer : peers) {
                httplib::Client cli("http://" + peer + ":8080");            
                if(auto res = cli.Post("/raft/append-entries", parseMapToJSON(body), "application/json")){
                    const auto response = parseJSONToMap(res->body);
                    votes += response.at("voteGranted") == "true";
                    // TODO: Handle higher terms
                    currentTerm = std::max<std::size_t>(currentTerm, std::stoull(response.at("term")));
                
                    if (votes >= (peers.size()+1)/2 + 1){
                        state = LEADER;
                    }
                }
            }

        } else if (RaftNode::state == LEADER) {
            // send out heartbeats, handle replication?
            while(true){
                std::unordered_map<std::string, std::string> body;
                body["term"] = currentTerm;
                body["leaderId"] = leaderID;
                body["prevLogIndex"] = std::to_string(log.size());
                body["prevLogTerm"] = std::to_string(log.back().term-1);
                body["entries"] = "";
                body["leaderCommit"] = commitIndex;
                
                for(auto peer : peers) {
                    httplib::Client cli("http://" + peer + ":8080");            
                    if(auto res = cli.Post("/raft/append-entries", parseMapToJSON(body), "application/json")){
                        // actions to be done later when entries are implemented 
                    }
                }
                std::this_thread::sleep_for(heartbeat_period);
            }
        } else if (RaftNode::state == FOLLOWER) {
            // check for heartbeat
            auto now = std::chrono::steady_clock::now();
            if (now - last_append_entries >= timeout) {
                // TODO: Become a candidate
            }
        }
    }
}

void RaftNode::stop() {}

std::pair<size_t, bool> RaftNode::appendEntries(size_t term, const std::string &leader_id,
                                      size_t prev_log_index, size_t prev_log_term,
                                      const std::string &entries, size_t leader_commit) {
    // TODO: Handle leader terms
    // TODO: Record valid leader
    // TODO: Reset valid-heartbeat timer
    // add to log / update data once entries are implemented 
    return {currentTerm, term >= currentTerm && (prev_log_index == 0 || (prev_log_index <= log.size() && log[prev_log_index - 1].term == prev_log_term))};
}

std::pair<size_t, bool> RaftNode::requestVote(size_t term, const std::string &candidate_id,
                                    size_t last_log_index, size_t last_log_term) {
    // TODO: Handle higher terms
    // TODO: Persist granted vote
    // TODO: Reset granted-vote timer
    return {currentTerm, term >= currentTerm && (term > currentTerm || votedFor.empty() || votedFor == candidate_id) && (last_log_term > (log.empty() ? 0 : log.back().term) || (last_log_term == (log.empty() ? 0 : log.back().term) && last_log_index >= log.size()))};
}
