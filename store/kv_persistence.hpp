#pragma once

#include <string>
#include <unordered_map>

bool writeSnapshot(std::unordered_map<std::string, std::string> &map);
void recoverFromDisk(std::unordered_map<std::string, std::string> &map);
void appendToWAL(std::string operation, std::string key = "", std::string value = "");
void checkpoint(std::unordered_map<std::string, std::string> &map);
void replayWAL(std::unordered_map<std::string, std::string> &map);
