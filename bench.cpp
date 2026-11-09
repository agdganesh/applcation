// Throughput benchmark: single-lock LRUCache vs ShardedLRUCache under N threads.
// Workload: 80% get / 20% put on a key range 2x the cache capacity.
#include "lru_cache.hpp"
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

template <typename Cache>
double run(Cache& c, int threads, int ops) {
    auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    for (int t = 0; t < threads; ++t)
        ts.emplace_back([&c, t, ops] {
            unsigned x = 2463534242u + t;
            for (int i = 0; i < ops; ++i) {
                x ^= x << 13; x ^= x >> 17; x ^= x << 5;  // xorshift
                int key = x % 20000;
                if (x % 10 < 8) c.get(key); else c.put(key, key);
            }
        });
    for (auto& t : ts) t.join();
    double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return (double)threads * ops / sec;
}

int main() {
    const int OPS = 400000;
    std::printf("hardware threads: %u\n", std::thread::hardware_concurrency());
    std::printf("%-8s %-18s %-18s\n", "threads", "single-lock ops/s", "sharded(16) ops/s");
    for (int th : {1, 2, 4, 8}) {
        LRUCache<int, int> a(10000);
        ShardedLRUCache<int, int> b(10000, 16);
        std::printf("%-8d %-18.0f %-18.0f\n", th, run(a, th, OPS), run(b, th, OPS));
    }
}
