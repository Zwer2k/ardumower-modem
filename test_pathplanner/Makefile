CXX ?= g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -I../lib/pathplanner -I../lib/Clipper2/CPP/Clipper2Lib/include

CLIPPER_SRC = \
	../lib/Clipper2/CPP/Clipper2Lib/src/clipper.engine.cpp \
	../lib/Clipper2/CPP/Clipper2Lib/src/clipper.offset.cpp \
	../lib/Clipper2/CPP/Clipper2Lib/src/clipper.rectclip.cpp

PLANNER_SRC = ../lib/pathplanner/pathplanner.cpp

all: test_exclusion

test_exclusion: test_exclusion.cpp $(PLANNER_SRC) $(CLIPPER_SRC)
	$(CXX) $(CXXFLAGS) -o $@ test_exclusion.cpp $(PLANNER_SRC) $(CLIPPER_SRC)

run: test_exclusion
	./test_exclusion

clean:
	rm -f test_exclusion

.PHONY: all run clean
