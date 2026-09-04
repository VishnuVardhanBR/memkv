#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

class KeyValue {
  private:
    std::unordered_map<std::string, std::string> key_value;
    std::mutex mutex;

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
void saveToDisk(std::unordered_map<std::string, std::string> &map);
void readFromDisk(std::unordered_map<std::string, std::string> &map);
