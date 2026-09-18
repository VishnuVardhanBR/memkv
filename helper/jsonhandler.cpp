#include "jsonhandler.hpp"

std::string parseMapToJSON(std::unordered_map<std::string, std::string> &map) {
    std::string JSON;
    JSON.append("{");
    bool first = true;
    for (std::pair<std::string, std::string> p : map) {
        if (!first)
            JSON.append(",");
        JSON.append("\"" + p.first + "\"");
        JSON.append(":");
        JSON.append("\"" + p.second + "\"");
        first = false;
    }
    JSON.append("}");
    return JSON;
}

std::unordered_map<std::string, std::string> parseJSONToMap(std::string json) {
    std::unordered_map<std::string, std::string> map;
    size_t pos = 1;
    while (pos < json.length() - 1) {
        size_t key_start = json.find('"', pos) + 1;
        size_t key_end = json.find('"', key_start);
        std::string key = json.substr(key_start, key_end - key_start);

        size_t value_start = json.find('"', key_end + 1) + 1;
        size_t value_end = json.find('"', value_start);
        std::string value = json.substr(value_start, value_end - value_start);

        map[key] = value;
        pos = value_end + 1;
    }
    return map;
}
