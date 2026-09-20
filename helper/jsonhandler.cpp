#include "jsonhandler.hpp"
#include <stdexcept>

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
    while (pos < json.length()) {
        size_t key_start = json.find('"', pos);
        if (key_start == std::string::npos)
            break;
        ++key_start;
        size_t key_end = json.find('"', key_start);
        if (key_end == std::string::npos)
            throw std::invalid_argument("unterminated JSON key");
        std::string key = json.substr(key_start, key_end - key_start);

        size_t colon = json.find(':', key_end + 1);
        if (colon == std::string::npos)
            throw std::invalid_argument("missing JSON colon");
        size_t value_start = json.find_first_not_of(" \t\r\n", colon + 1);
        if (value_start == std::string::npos)
            throw std::invalid_argument("missing JSON value");
        bool quoted = json[value_start] == '"';
        if (quoted)
            ++value_start;
        size_t value_end = quoted ? json.find('"', value_start)
                                  : json.find_first_of(",}", value_start);
        if (value_end == std::string::npos)
            throw std::invalid_argument("unterminated JSON value");
        std::string value = json.substr(value_start, value_end - value_start);
        if (!quoted)
            value.erase(value.find_last_not_of(" \t\r\n") + 1);

        map[key] = value;
        pos = value_end + 1;
    }
    return map;
}
