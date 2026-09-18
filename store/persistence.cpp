#include "persistence.hpp"
#include "../helper/jsonhandler.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>

const std::filesystem::path data_directory = "data";
const std::filesystem::path snapshot_directory = data_directory / "kv";
const std::filesystem::path wal_file = data_directory / "wal.log";

// Store map to disk in json format
bool writeSnapshot(std::unordered_map<std::string, std::string> &map) {
    auto timestamp = std::chrono::system_clock::now();
    std::filesystem::create_directories(snapshot_directory);
    auto file_path = snapshot_directory /
                     (std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                         timestamp.time_since_epoch())
                                         .count()) +
                      "_kv.json");
    std::ofstream outputFile(file_path);
    if (!outputFile.is_open()) {
        return false;
    }

    outputFile << parseMapToJSON(map);
    outputFile.close();
    return outputFile.good();
}

void recoverFromDisk(std::unordered_map<std::string, std::string> &map) {
    std::filesystem::create_directories(snapshot_directory);
    std::filesystem::path latest_file;

    for (const auto &file : std::filesystem::directory_iterator(snapshot_directory))
        if (file.path() > latest_file)
            latest_file = file.path();

    std::ifstream inputFile(latest_file);
    if (inputFile.is_open()) {
        std::stringstream buffer;
        buffer << inputFile.rdbuf();
        map = parseJSONToMap(buffer.str());
    }
    replayWAL(map);
}

// timestamp, operation, key, value
void appendToWAL(std::string operation, std::string key, std::string value) {
    std::filesystem::create_directories(data_directory);
    std::ofstream outputFile(wal_file, std::ios::app);
    if (!outputFile.is_open()) {
        return;
    }
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    outputFile << timestamp << ' ' << operation << ' ' << std::quoted(key) << ' '
               << std::quoted(value) << '\n'
               << std::flush;
}

// write to storage, clear WAL
void checkpoint(std::unordered_map<std::string, std::string> &map) {
    if (!writeSnapshot(map))
        return;

    // Empty the WAL since its operations are now in the snapshot.
    std::ofstream(wal_file, std::ios::trunc);
}

// replay all logs from WAL one by one from previous checkpoint
void replayWAL(std::unordered_map<std::string, std::string> &map) {
    std::ifstream inputFile(wal_file);
    long long timestamp;
    std::string operation, key, value;

    while (inputFile >> timestamp >> operation >> std::quoted(key) >> std::quoted(value)) {
        if (operation == "INSERT")
            map[key] = value;
        else if (operation == "ERASE")
            map.erase(key);
        else if (operation == "CLEAR")
            map.clear();
    }
}
