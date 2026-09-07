# HTTP Server
- I used a unordered map, made a class for it and exposed getter and setter methods. 
- Used httplib.h which is a minimal cpp library, had to edit the ThreadPool to 100 since its what is required by the challenge. 
- Used a std::atomic<bool> ready flag to keep track of ready state. Atomic ensures only one thread operates it at a time. I then removed it because its not needed logically. Since we are using svr.wait_until_ready();
- for the key value store, i had to use mutex locks, because concurrent writes will cause issues. I can use mutex lock and unlock directly, but wrapping it with lock guard basically makes it easy. It's generally not advised to use directly, and is recommended to use the wrapper to avoid exceptions. Avoids deadlocks. 


# Persistence
- Used JSON format to store to file, basic mapping from unordered map, no formatting validation. 
- SIGTERM and SIGINT handled using std::signal, which takes a function which runs when encountering these signals, I use a atomic bool which indicates shutdown has been requested.
- We use this shutdownRequested, and check it by polling every 50ms, this is hacky but works. If requested then we move on to stopping it and joinging the serverThread back to main thread which waits for everything in the thread to be processed, after which we save the kv to file. 
- httplib already defaults both server timeouts to five seconds (5s was mentioned in the challenge)

# Crash Recovery
- Modified KeyStore's operations to write to log before doing the operation.
- Wrote a hacky way to checkpoint when operationsSinceCheckpoint reaches 10; an observer pattern could be considered in the future.
- Changed writeSnapshot to include a timestamp in the file name, recoverFromDisk gets the latest file and calls replayWAL.



## TODO
- Implement WAL
  [X] before each operation, append to WAL on disk and force flush it.
  [X] use persistence after every x operations, we clear WAL after that.
  [X] need timestamps now

# New cpp learnings 
- std::atomic, normal datatypes in int essentially do three things, read value from memory, modify value and write it back. However when we are working with async programs like a webserver, this will cause concurrency issues. std::atomic makes sure that there are no data races. 

- Recap of lambda. [] allows lambda function to use data outside of it, () are params, {} is body 

- std::lock_guard is a mutex wrapper to avoid deadlocks and exceptions 
