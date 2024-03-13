#ifndef _PULSAR_STRUCTURES_HASHMAP_H
#define _PULSAR_STRUCTURES_HASHMAP_H

#include "pulsar/core.h"

#include "pulsar/structures/list.h"

namespace Pulsar
{
    template<typename K, typename V>
    class HashMap; // Forward Declaration

    template<typename K, typename V>
    class HashMapBucket
    {
    public:
        typedef HashMapBucket<K, V> SelfType;

        HashMapBucket()
            : KeyStorage{}, ValueStorage{} { };
        ~HashMapBucket() { Clear(); }

        HashMapBucket(const SelfType& other)
        {
            if (other.Populated) {
                Populated = true;
                new(&Key) K(other.Key);
                new(&Value) V(other.Value);
            }
        }

        HashMapBucket(SelfType&& other)
        {
            if (other.Populated) {
                Populated = true;
                new(&Key) K(std::move(other.Key));
                new(&Value) V(std::move(other.Value));
                other.Clear();
            }
        }

        SelfType& operator=(const SelfType& other) = delete;
        SelfType& operator=(SelfType&& other) = delete;

        void Clear()
        {
            if (!Populated)
                return;
            Populated = false;
            Key.~K();
            Value.~V();
        }

        bool Populated = false;
        union
        {
            K Key;
            uint8_t KeyStorage[sizeof(K)];
        };
        union
        {
            V Value;
            uint8_t ValueStorage[sizeof(V)];
        };     
    };

    template<typename K, typename V>
    class HashMap
    {
    public:
        typedef HashMap<K, V> SelfType;
        typedef HashMapBucket<K, V> BucketType;
        struct Pair
        {
            K Key;
            V Value;
        };
        struct PairRef
        {
            const K* Key = nullptr;
            V* Value = nullptr;
            operator bool() const { return Key; }
        };
        struct ConstPairRef
        {
            const K* Key = nullptr;
            const V* Value = nullptr;
            operator bool() const { return Key; }
        };

        HashMap() { m_Buckets.Resize(1); }
        HashMap(std::initializer_list<Pair> init)
            : HashMap()
        {
            for (auto it = init.begin(); it != init.end(); it++)
                Insert(it->Key, it->Value);
        }

        ~HashMap() = default;

        void Reserve(size_t newCapacity)
        {
            if (newCapacity == Capacity())
                return;
            else if (newCapacity < Capacity()) {
                size_t count = Count();
                if (newCapacity < count)
                    return;
            }
            List<BucketType> oldBuckets(std::move(m_Buckets));
            m_Buckets.Reserve(newCapacity);
            m_Buckets.Resize(newCapacity);
            for (size_t i = 0; i < oldBuckets.Size(); i++) {
                if (oldBuckets[i].Populated)
                    Emplace(std::move(oldBuckets[i].Key), std::move(oldBuckets[i].Value));
            }
        }

        PairRef Insert(const K& key, const V& value) { K _k = key; V _v = value; return Insert(std::move(_k), std::move(_v)); }
        PairRef Insert(const K& key, V&& value)      { K _k = key; return Insert(std::move(_k), std::move(value)); }
        PairRef Insert(K&& key, const V& value)      { V _v = value; return Insert(std::move(key), std::move(_v)); }
        PairRef Insert(K&& key, V&& value)           { return Emplace(std::move(key), std::move(value)); }

        template<typename ...Args>
        PairRef Emplace(const K& key, Args&& ...args) { K _k = key; return Emplace(std::move(_k), args...); }

        template<typename ...Args>
        PairRef Emplace(K&& key, Args&& ...args)
        {
            size_t hash = std::hash<K>{}(key);
            size_t startIdx = hash % m_Buckets.Size();
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                BucketType& bucket = m_Buckets[(startIdx+i)%m_Buckets.Size()];
                if (!bucket.Populated) {
                    bucket.Populated = true;
                    new(&bucket.Key) K(std::move(key));
                    new(&bucket.Value) V(args...);
                    return {&bucket.Key, &bucket.Value};
                } else if (bucket.Key == key) {
                    bucket.Value.~V();
                    new(&bucket.Value) V(args...);
                    return {&bucket.Key, &bucket.Value};
                }
            }
            // No free buckets!
            Reserve(Capacity()*3/2+1);
            return Emplace(std::move(key), args...);
        }

        PairRef Find(const K& key)
        {
            size_t hash = std::hash<K>{}(key);
            size_t startIdx = hash % m_Buckets.Size();
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                BucketType& bucket = m_Buckets[(startIdx+i)%m_Buckets.Size()];
                if (!bucket.Populated)
                    continue;
                else if (bucket.Key == key)
                    return {&bucket.Key, &bucket.Value};
            }
            return {};
        }

        ConstPairRef Find(const K& key) const
        {
            size_t hash = std::hash<K>{}(key);
            size_t startIdx = hash % m_Buckets.Size();
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                const BucketType& bucket = m_Buckets[(startIdx+i)%m_Buckets.Size()];
                if (!bucket.Populated)
                    continue;
                else if (bucket.Key == key)
                    return {&bucket.Key, &bucket.Value};
            }
            return {};
        }

        void Remove(const K& key)
        {
            size_t hash = std::hash<K>{}(key);
            size_t startIdx = hash % m_Buckets.Size();
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                BucketType& bucket = m_Buckets[(startIdx+i)%m_Buckets.Size()];
                if (!bucket.Populated)
                    continue;
                else if (bucket.Key == key) {
                    bucket.Clear();
                    return;
                }
            }
        }

        void Clear() { m_Buckets.Clear(); }

        size_t Count() const
        {
            size_t count = 0;
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                if (m_Buckets[i].Populated)
                    count++;
            }
            return count;
        }

        size_t Capacity() const { return m_Buckets.Size(); }
    private:
        List<BucketType> m_Buckets;
    };
}

#endif // _PULSAR_STRUCTURES_HASHMAP_H
