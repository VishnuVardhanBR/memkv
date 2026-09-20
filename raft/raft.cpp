#include "raft.hpp"
#include "../helper/jsonhandler.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>
#include <random>
#include <httplib.h>

const std::filesystem::path data_directory = "data";
const std::filesystem::path raft_state_file = data_directory / "raft_state";

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


    //set random timeout 
    std::mt19937_64 eng{std::random_device{}()};
    std::uniform_int_distribution<> dist{500, 1000};
    timeout = std::chrono::milliseconds{dist(eng)};
}

void RaftNode::start(){
    
    // if commitNode > lastApplied, this node needs to catch up. 


    if(RaftNode::state == CANDIDATE){
        // handle votes and election
    }
    else if(RaftNode::state == LEADER){
        //send out heartbeats, handle replication? 
    }
    else if(RaftNode::state == FOLLOWER){
        // check for heartbeat
    }
}

void RaftNode::stop(){

}