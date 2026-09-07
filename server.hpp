#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

class KeyValue {
  private:
    std::unordered_map<std::string, std::string> key_value;
    std::mutex mutex;
    int operations = 0;

  public:
    std::string getValue(std::string &key);
    void incrementOperations();
    int insert(std::string &key, std::string &value);
    int erase(std::string &key);
    void clear();
    void save();
    void load();
};

std::string parseMapToJSON(std::unordered_map<std::string, std::string> &map);
std::unordered_map<std::string, std::string> parseJSONToMap(std::string json);
bool saveToDisk(std::unordered_map<std::string, std::string> &map);
void readFromDisk(std::unordered_map<std::string, std::string> &map);
void appendToWAL(std::string operation, std::string key = "", std::string value = "");
void flushWAL(std::unordered_map<std::string, std::string> &map);
void replayWAL(std::unordered_map<std::string, std::string> &map);
