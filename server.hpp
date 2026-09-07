#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

class KeyValue {
  private:
    std::unordered_map<std::string, std::string> key_value;
    std::mutex mutex;
    int operationsSinceCheckpoint = 0;
    void incrementOperations();

  public:
    std::string getValue(std::string &key);
    int insert(std::string &key, std::string &value);
    int erase(std::string &key);
    void clear();
    void save();
    void load();
};

std::string parseMapToJSON(std::unordered_map<std::string, std::string> &map);
std::unordered_map<std::string, std::string> parseJSONToMap(std::string json);
bool writeSnapshot(std::unordered_map<std::string, std::string> &map);
void recoverFromDisk(std::unordered_map<std::string, std::string> &map);
void appendToWAL(std::string operation, std::string key = "", std::string value = "");
void checkpoint(std::unordered_map<std::string, std::string> &map);
void replayWAL(std::unordered_map<std::string, std::string> &map);
