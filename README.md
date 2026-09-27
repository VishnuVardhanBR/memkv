# memkv

A simple key-value store written in C++.

JSON parsing and serialization use [nlohmann/json v3.12.0](https://github.com/nlohmann/json/releases/tag/v3.12.0),
vendored in `third_party/nlohmann/` with its MIT license. No separate installation is needed.
The original `helper/jsonhandler.*` files are retained for reference.

## TODO

- [x] HTTP API
- [x] Persistence
- [x] Crash Recovery
- [ ] Leader Election
- [ ] Log Replication
- [ ] Log Compaction
- [ ] Membership Changes
- [ ] Joint Consensus
