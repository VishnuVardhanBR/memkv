FROM gcc:14-bookworm

RUN apt-get update && \
    apt-get install -y --no-install-recommends iptables iproute2 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY server.cpp httplib.h ./

RUN g++ -std=c++17 -O2 -pthread server.cpp -o server

VOLUME ["/app/data"]

EXPOSE 8080
ENTRYPOINT ["./server"]
