TARGETS = server subscriber

CXX = g++
CXXFLAGS = -Wall -O2 -std=c++17

SERVER_SRCS = server.cpp utils.cpp
SUBSCRIBER_SRCS = subscriber.cpp utils.cpp
HEADERS = utils.h

all: $(TARGETS)

server: $(SERVER_SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o server $(SERVER_SRCS)

subscriber: $(SUBSCRIBER_SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o subscriber $(SUBSCRIBER_SRCS)

run_server: server
	./server $(PORT)

run_subscriber: subscriber
	./subscriber $(HOST) $(ID_CLIENT) $(IP_SERVER) $(PORT_SERVER)

clean:
	rm -f $(TARGETS)