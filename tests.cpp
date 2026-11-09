#include "lru_cache.hpp"
#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; \
    std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

void test_basic() {
    LRUCache<int, std::string> c(2);
    c.put(1, "a"); c.put(2, "b");
    CHECK(c.get(1).value() == "a");
    CHECK(!c.get(3).has_value());
    CHECK(c.size() == 2);
}
void test_eviction_order() {
    LRUCache<int, int> c(2);
    c.put(1, 10); c.put(2, 20);
    c.get(1);                       // 1 is now most recent, so 2 is the LRU
    c.put(3, 30);                   // evicts 2
    CHECK(c.get(1).has_value());
    CHECK(!c.get(2).has_value());
    CHECK(c.get(3).has_value());
}
void test_update_refreshes() {
    LRUCache<int, int> c(2);
    c.put(1, 1); c.put(2, 2);
    c.put(1, 100);                  // update key 1 -> most recent
    c.put(3, 3);                    // evicts 2
    CHECK(c.get(1).value() == 100);
    CHECK(!c.get(2).has_value());
}
void test_capacity_one_and_zero() {
    LRUCache<int, int> c(1);
    c.put(1, 1); c.put(2, 2);
    CHECK(!c.get(1).has_value());
    CHECK(c.get(2).value() == 2);
    bool threw = false;
    try { LRUCache<int, int> bad(0); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}
void test_hit_miss_counters() {
    LRUCache<int, int> c(2);
    c.put(1, 1);
    c.get(1); c.get(2); c.get(1);
    CHECK(c.hits() == 2);
    CHECK(c.misses() == 1);
}
// Runs many threads doing random put/get. Returns how many reads returned a value that
// did not match its key (must be 0). Uses an atomic counter so the test itself is race-free.
template <typename Cache>
long hammer(Cache& c, int threads, int ops) {
    std::atomic<long> bad{0};
    std::vector<std::thread> ts;
    for (int t = 0; t < threads; ++t)
        ts.emplace_back([&c, &bad, t, ops] {
            unsigned x = 12345u + t;
            for (int i = 0; i < ops; ++i) {
                x = x * 1664525u + 1013904223u;          // simple LCG, deterministic per thread
                int key = x % 200;
                if (x & 0x100) {
                    c.put(key, key * 2);
                } else {
                    auto v = c.get(key);
                    if (v && *v != key * 2) ++bad;       // a value must always match its key
                }
            }
        });
    for (auto& t : ts) t.join();
    return bad.load();
}
void test_concurrent_single_lock() {
    LRUCache<int, int> c(64);
    CHECK(hammer(c, 8, 20000) == 0);
    CHECK(c.size() <= 64);
}
void test_concurrent_sharded() {
    ShardedLRUCache<int, int> c(64, 8);
    CHECK(hammer(c, 8, 20000) == 0);
    CHECK(c.size() <= c.max_size());
}

int main() {
    test_basic(); test_eviction_order(); test_update_refreshes();
    test_capacity_one_and_zero(); test_hit_miss_counters();
    test_concurrent_single_lock(); test_concurrent_sharded();
    std::cout << (checks - failures) << "/" << checks << " checks passed\n";
    return failures ? 1 : 0;
}
