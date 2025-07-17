#pragma once

#include "types.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>

namespace my_redis
{
    namespace hashtable
    {
        struct HashNode
        {
            HashNode *next = nullptr;
            types::u64 hash = 0;
        };

        struct HashTable
        {
            HashNode **table = nullptr; // array of slots
            types::size mask = 0;       // buckets - 1, buckets is a power of 2
            types::size size = 0;       // number of keys

            constexpr types::size bucketCount() const noexcept { return mask + 1; }
        };

        void init(HashTable *tbl, types::size n) noexcept;
        void insert(HashTable *tbl, HashNode *node) noexcept;

        using CompareFn = bool (*)(HashNode *lhs, HashNode *rhs) noexcept;
        HashNode **lookup(HashTable *tbl, HashNode *key, CompareFn cmp) noexcept;
        HashNode *detach(HashTable *tbl, HashNode **from) noexcept;
    } // namespace hashtable

    namespace hashmap
    {
        struct HashMap
        {
            hashtable::HashTable newer;
            hashtable::HashTable older;
            types::size migratePos = 0;
        };

        hashtable::HashNode *lookup(HashMap *map, hashtable::HashNode *key, hashtable::CompareFn cmp) noexcept;
        void insert(HashMap *map, hashtable::HashNode *node) noexcept;
        hashtable::HashNode *remove(HashMap *map, hashtable::HashNode *key, hashtable::CompareFn cmp) noexcept;
    } // namespace hashmap
}
