#pragma once

#include <cstddef>
#include <string>
#include <vector>

enum State {
    CANDIDATE,
    LEADER,
    FOLLOWER
};

class RaftNode {
  private:
    std::size_t currentTerm = 0;
    std::string votedFor;
    std::string leaderID;
    std::string id;
    std::vector<std::string> peers;
    State state = FOLLOWER;

  public:
    RaftNode(std::string id, std::vector<std::string> peers);
};
