#include "server.hpp"
#include <stdexcept>

std::string KeyValue::getValue(std::string &key) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = key_value.find(key);
    if (it == key_value.end()) {
        throw std::out_of_range("key not found");
    }
    return it->second;
}

void KeyValue::incrementOperations() {
    operations++;
    // hack : clear WAL and force write persistence
    if (operations % 10 == 0)
        flushWAL(key_value);
}
int KeyValue::insert(std::string &key, std::string &value) {
    std::lock_guard<std::mutex> lock(mutex);
    appendToWAL("INSERT", key, value);
    key_value[key] = value;
    incrementOperations();
    return 0;
}

int KeyValue::erase(std::string &key) {
    std::lock_guard<std::mutex> lock(mutex);
    appendToWAL("ERASE", key);
    key_value.erase(key);
    incrementOperations();
    return 0;
}

void KeyValue::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    appendToWAL("CLEAR");
    key_value.clear();
    incrementOperations();
}

void KeyValue::save() {
    std::lock_guard<std::mutex> lock(mutex);
    saveToDisk(key_value);
}

void KeyValue::load() {
    std::lock_guard<std::mutex> lock(mutex);
    readFromDisk(key_value);
}
