CXX = g++ -std=c++1z -lform -lcursesw -lcurl -g

out/jDict: jDict.cpp
	$(CXX) jDict.cpp -o out/jDict

all: out/jDict

clean:
	$(RM) out/

.PHONY: clean