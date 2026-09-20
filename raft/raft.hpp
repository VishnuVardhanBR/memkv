#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

enum State {
    CANDIDATE,
    LEADER,
    FOLLOWER
};

struct LogEntry {
  std::string command; 
  std::size_t term;
};

class RaftNode {
  private:
    std::size_t currentTerm = 0;
    std::string votedFor;
    std::string leaderID;
    std::string id;
    std::vector<LogEntry> log;
    std::vector<std::string> peers;
    State state = FOLLOWER;
    std::size_t commitIndex = 0;
    std::size_t lastApplied = 0;
    std::vector<std::size_t> nextIndex; 
    std::vector<std::size_t> matchIndex;
    std::chrono::milliseconds timeout;
  public:
    RaftNode(std::string id, std::vector<std::string> peers);
    void start();
    void stop();
    std::pair<size_t, bool> appendEntries(size_t term, const std::string &leader_id,
                                        size_t prev_log_index, size_t prev_log_term,
                                        const std::string &entries, size_t leader_commit);
    std::pair<size_t, bool> requestVote(size_t term, const std::string &candidate_id,
                                      size_t last_log_index, size_t last_log_term);
};
