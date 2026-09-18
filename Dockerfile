FROM gcc:14-bookworm

RUN apt-get update && \
    apt-get install -y --no-install-recommends iptables iproute2 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY api/ ./api/
COPY helper/ ./helper/
COPY raft/ ./raft/
COPY store/ ./store/

RUN g++ -std=c++17 -O2 -pthread api/server.cpp store/store.cpp store/persistence.cpp \
    helper/jsonhandler.cpp raft/raft.cpp -o server

VOLUME ["/app/data"]

EXPOSE 8080
ENTRYPOINT ["./server"]
