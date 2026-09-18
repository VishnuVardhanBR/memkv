#pragma once

#include <string>
#include <unordered_map>

std::string parseMapToJSON(std::unordered_map<std::string, std::string> &map);
std::unordered_map<std::string, std::string> parseJSONToMap(std::string json);
