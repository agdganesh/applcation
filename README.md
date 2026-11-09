# Thread-safe LRU Cache in C++17 (Linux)

A small systems-programming project: an LRU (least-recently-used) cache with O(1) `get`/`put`,
plus a sharded version designed to reduce lock contention, with unit tests, sanitizer builds and a benchmark.

## Design
- **`LRUCache<K,V>`** - `std::unordered_map` (key -> list iterator) + `std::list` (recency order).
  `get` moves the entry to the front with `splice` (O(1), no copies); `put` evicts from the back when full.
  One `std::mutex` guards all state.
- **`ShardedLRUCache<K,V>`** - N independent `LRUCache` shards chosen by `hash(key) % N`.
  Threads touching different shards do not wait on the same lock.
  Trade-off: eviction is LRU *per shard*, not global, and total capacity is rounded up per shard.

## Build and run (Linux, g++ 9+)
```bash
make test    # 17 unit checks incl. eviction order, update-refreshes-recency, capacity edge cases, 8-thread stress test
make asan    # same tests under AddressSanitizer + UBSan (memory errors / undefined behaviour)
make tsan    # same tests under ThreadSanitizer (data races)
make bench   # throughput: single-lock vs sharded at 1/2/4/8 threads
```
Run `make bench` on **your own machine** and record your numbers; results depend on core count.
(Benchmarks run on a 1-core machine cannot show lock-contention differences.)

## GitHub Actions
The CI workflow runs `make test`, `make asan`, and `make tsan` on Ubuntu for each push
and pull request. It can also be started manually from the repository's Actions tab.

## Debugging exercise (do this yourself, it's the point of the project)
1. In `lru_cache.hpp`, delete the `std::lock_guard` line inside `LRUCache::get`.
2. Run `make tsan` and read the report: which two accesses race? Which mutex was held by the other thread?
3. Restore the lock. Write down what happened and why `get` needs a lock even though it "only reads"
   (hint: it reorders the list).
4. Repeat with `gdb`: build with `-g`, break inside `put`, inspect `items_` and `index_`.

## Known limitations
- Single global lock per shard; no lock-free design.
- No TTL, no memory-size-based eviction, no persistence.
- Tests use a tiny hand-written CHECK macro instead of GoogleTest/Catch2 (an easy upgrade).

## Ideas to extend
- Replace the CHECK macro with GoogleTest. Expand CI coverage to additional compilers or platforms.
- Add TTL expiry; add a `std::shared_mutex` read path and measure whether it helps.
- Profile with `perf` and write up what you find.
