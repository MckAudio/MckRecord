CXX = g++
BIN = mck-record
SOURCES = ./src/main.cpp ./src/AudioHandler.cpp
REL_FLAGS = -O2 -DNDEBUG -std=c++17
DEB_FLAGS = -O0 -DDEBUG -ggdb3 -std=c++17
LINKS = -lrtaudio -lsndfile


default: release

release:
	mkdir -p bin/release | true
	$(CXX) $(REL_FLAGS) -o ./bin/release/$(BIN) $(SOURCES) $(LINKS)

debug:
	mkdir -p bin/debug | true
	$(CXX) $(DEB_FLAGS) -o ./bin/debug/$(BIN) $(SOURCES) $(LINKS)

install: release
	cp ./bin/release/mck-record /usr/bin/mck-record