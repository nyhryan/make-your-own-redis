#include "hashtable.hpp"
#include "types.hpp"
#include <cstdlib>

namespace my_redis
{
    using namespace my_redis::types;
    
    namespace hashtable
    {

        void init(HashTable *hashTable, size n) noexcept
        {
            assert(n > 0 && ((n - 1) & n) == 0); // n must be a power of 2

            // Allocate n nodes for the hash table
            hashTable->table = static_cast<HashNode **>(std::calloc(n, sizeof(HashNode *)));
            hashTable->mask = n - 1;
            hashTable->size = 0;
        }

        // Insert a new node into the hash table, before head
        void insert(HashTable *hashTable, HashNode *newNode) noexcept
        {
            size pos = newNode->hash & hashTable->mask; // hash(key) % n, but faster
            HashNode *head = hashTable->table[pos];
            newNode->next = head;
            hashTable->table[pos] = newNode;
            hashTable->size++;
        }

        HashNode **lookup(HashTable *hashTable, HashNode *key, CompareFn cmp) noexcept
        {
            if (!hashTable->table)
                return nullptr;

            // To make node removal easier, return an address of `next` pointer of the parent node
            size pos = key->hash & hashTable->mask;
            HashNode **head = &hashTable->table[pos];
            HashNode **pNext = head;
            
            for (
                HashNode *curr = nullptr;
                (curr = *pNext) != nullptr; // Move to the next node
                pNext = &curr->next         // Update the pointer to the address of next node pointer
            )
            {
                if (curr->hash == key->hash && cmp(curr, key))
                    return pNext;
            }
            return nullptr;
        }

        HashNode *detach(HashTable *hashTable, HashNode **from) noexcept
        {
            HashNode *node = *from;
            *from = node->next;
            hashTable->size--;
            return node;
        }
    } // namespace hashtable

    namespace hashmap
    {
        constexpr size K_REHASHING_WORK = 128;
        using namespace hashtable;
        
        // Prepare for rehashing
        void triggerRehash(HashMap *map) noexcept
        {
            assert(map->older.table == nullptr);
            // Move the current `newer` table to `older`
            map->older = map->newer;

            // Reset `newer` table to double the capacity
            hashtable::init(&map->newer, (map->newer.bucketCount()) * 2);

            // Reset the migration position
            map->migratePos = 0;
        }

        // Move `K_REHASHING_WORK` nodes from `older` to `newer` table
        void helpRehash(HashMap *map) noexcept
        {
            size nwork = 0;
            while (nwork < K_REHASHING_WORK && map->older.size > 0)
            {
                HashNode **from = &map->older.table[map->migratePos];
                if (!*from)
                {
                    map->migratePos++;
                    continue;
                }

                hashtable::insert(&map->newer, hashtable::detach(&map->older, from));
                nwork++;
            }

            if (map->older.size == 0 && map->older.table)
            {
                std::free(map->older.table);
                map->older = {};
            }
        }

        HashNode *lookup(HashMap *map, HashNode *key, CompareFn cmp) noexcept
        {
            helpRehash(map);

            HashNode **from = hashtable::lookup(&map->newer, key, cmp);
            if (!from)
                from = hashtable::lookup(&map->older, key, cmp);
            return from ? *from : nullptr;
        }

        /*
            load_factor = entries / buckets
                        = hashtable.size / (hashtable.bucketCount())
            If load_factor > K_MAX_LOAD_FACTOR, then rehashing is triggered.
        */
        constexpr size K_MAX_LOAD_FACTOR = 8;
        void insert(HashMap *map, HashNode *node) noexcept
        {
            if (!map->newer.table)
                hashtable::init(&map->newer, 4);

            hashtable::insert(&map->newer, node);

            if (!map->older.table)
            {
                size threshold = (map->newer.bucketCount()) * K_MAX_LOAD_FACTOR;
                if (map->newer.size >= threshold)
                    triggerRehash(map);
            }

            helpRehash(map);
        }

        HashNode *remove(HashMap *map, HashNode *key, CompareFn cmp) noexcept
        {
            helpRehash(map);

            if (HashNode **from = hashtable::lookup(&map->newer, key, cmp))
                return hashtable::detach(&map->newer, from);

            if (HashNode **from = hashtable::lookup(&map->older, key, cmp))
                return hashtable::detach(&map->older, from);

            return nullptr;
        }
    } // namespace hashmap
} // namespace my_redis