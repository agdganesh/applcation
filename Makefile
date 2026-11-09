CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread

test: tests.cpp lru_cache.hpp
	$(CXX) $(CXXFLAGS) tests.cpp -o tests && ./tests
asan: tests.cpp lru_cache.hpp          # memory errors + undefined behaviour
	$(CXX) -std=c++17 -O1 -g -Wall -Wextra -pthread -fsanitize=address,undefined tests.cpp -o tests_asan && ./tests_asan
tsan: tests.cpp lru_cache.hpp          # data races
	$(CXX) -std=c++17 -O1 -g -Wall -Wextra -pthread -fsanitize=thread tests.cpp -o tests_tsan && ./tests_tsan
bench: bench.cpp lru_cache.hpp
	$(CXX) $(CXXFLAGS) bench.cpp -o bench && ./bench
clean:
	rm -f tests tests_asan tests_tsan bench
