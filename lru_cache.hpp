// Thread-safe LRU caches (C++17).
//  - LRUCache:        one mutex, O(1) get/put (hash map + doubly linked list).
//  - ShardedLRUCache: N independent LRUCache shards chosen by key hash, so threads
//                     touching different shards do not contend on the same lock.
#pragma once
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

template <typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(std::size_t capacity) : capacity_(capacity) {
        if (capacity == 0) throw std::invalid_argument("capacity must be > 0");
    }

    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(m_);
        auto it = index_.find(key);
        if (it == index_.end()) { ++misses_; return std::nullopt; }
        items_.splice(items_.begin(), items_, it->second);   // move to front (most recent)
        ++hits_;
        return it->second->second;
    }

    void put(const K& key, V value) {
        std::lock_guard<std::mutex> lock(m_);
        auto it = index_.find(key);
        if (it != index_.end()) {                              // update + mark most recent
            it->second->second = std::move(value);
            items_.splice(items_.begin(), items_, it->second);
            return;
        }
        if (items_.size() == capacity_) {                      // evict least recently used
            index_.erase(items_.back().first);
            items_.pop_back();
        }
        items_.emplace_front(key, std::move(value));
        index_[key] = items_.begin();
    }

    std::size_t size() const   { std::lock_guard<std::mutex> l(m_); return items_.size(); }
    std::size_t capacity() const { return capacity_; }
    std::size_t hits() const   { std::lock_guard<std::mutex> l(m_); return hits_; }
    std::size_t misses() const { std::lock_guard<std::mutex> l(m_); return misses_; }

private:
    using Item = std::pair<K, V>;
    const std::size_t capacity_;
    mutable std::mutex m_;
    std::list<Item> items_;                                    // front = most recent
    std::unordered_map<K, typename std::list<Item>::iterator> index_;
    std::size_t hits_ = 0, misses_ = 0;
};

template <typename K, typename V, typename Hash = std::hash<K>>
class ShardedLRUCache {
public:
    // Total capacity is split across shards (rounded up), so the real limit can be
    // slightly above `capacity` when it is not divisible by `shards`.
    ShardedLRUCache(std::size_t capacity, std::size_t shards) {
        if (shards == 0) throw std::invalid_argument("shards must be > 0");
        std::size_t per = (capacity + shards - 1) / shards;
        for (std::size_t i = 0; i < shards; ++i)
            shards_.push_back(std::make_unique<LRUCache<K, V>>(per));
    }
    std::optional<V> get(const K& k) { return shard(k).get(k); }
    void put(const K& k, V v)        { shard(k).put(k, std::move(v)); }
    std::size_t size() const { std::size_t n = 0; for (auto& s : shards_) n += s->size(); return n; }
    std::size_t max_size() const { return shards_.size() * shards_[0]->capacity(); }
private:
    LRUCache<K, V>& shard(const K& k) { return *shards_[Hash{}(k) % shards_.size()]; }
    std::vector<std::unique_ptr<LRUCache<K, V>>> shards_;
};
