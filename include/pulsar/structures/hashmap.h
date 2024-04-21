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
        typedef HashMap<K, V> MapType;
        friend MapType;

        HashMapBucket()
            : m_KeyStorage{}, m_ValueStorage{} { };
        ~HashMapBucket() { Clear(); }

        HashMapBucket(const SelfType& other)
        {
            if (other.m_Populated) {
                m_Populated = true;
                new(&m_Key) K(other.m_Key);
                new(&m_Value) V(other.m_Value);
            }
        }

        HashMapBucket(SelfType&& other)
        {
            if (other.m_Populated) {
                m_Populated = true;
                new(&m_Key) K(std::move(other.m_Key));
                new(&m_Value) V(std::move(other.m_Value));
                other.Clear();
            }
        }

        SelfType& operator=(const SelfType& other) = delete;
        SelfType& operator=(SelfType&& other) = delete;

        void Clear()
        {
            if (!m_Populated)
                return;
            m_Populated = false;
            m_Key.~K();
            m_Value.~V();
        }

        bool IsPopulated() const { return m_Populated; }
        operator bool() const { return IsPopulated(); }

        const K& Key() const { return m_Key; }

        V& Value() { return m_Value; }
        const V& Value() const { return m_Value; }

    private:
        bool m_Populated = false;
        union
        {
            K m_Key;
            uint8_t m_KeyStorage[sizeof(K)];
        };
        union
        {
            V m_Value;
            uint8_t m_ValueStorage[sizeof(V)];
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

        // Capacity cannot be == 0
        // So if newCapacity is 0, 1 is the value used
        void Reserve(size_t newCapacity)
        {
            if (newCapacity == Capacity())
                return;
            else if (newCapacity < Capacity()) {
                size_t count = Count();
                if (newCapacity < count)
                    return;
            }
            // Force Capacity to be 1
            if (newCapacity <= 0)
                newCapacity = 1;
            List<BucketType> oldBuckets(std::move(m_Buckets));
            m_Buckets.Reserve(newCapacity);
            m_Buckets.Resize(newCapacity);
            for (size_t i = 0; i < oldBuckets.Size(); i++) {
                if (oldBuckets[i].m_Populated)
                    Emplace(std::move(oldBuckets[i].m_Key), std::move(oldBuckets[i].m_Value));
            }
        }

        PairRef Insert(const K& key, const V& value) { return Emplace(key, value); }
        PairRef Insert(const K& key, V&& value)      { return Emplace(key, std::move(value)); }
        PairRef Insert(K&& key, const V& value)      { return Emplace(std::move(key), value); }
        PairRef Insert(K&& key, V&& value)           { return Emplace(std::move(key), std::move(value)); }

        template<typename ...Args>
        PairRef Emplace(const K& key, Args&& ...args) { K _k = key; return Emplace(std::move(_k), std::forward<Args>(args)...); }

        template<typename ...Args>
        PairRef Emplace(K&& key, Args&& ...args)
        {
            size_t hash = std::hash<K>{}(key);
            size_t startIdx = hash % m_Buckets.Size();
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                BucketType& bucket = m_Buckets[(startIdx+i)%m_Buckets.Size()];
                if (!bucket.m_Populated) {
                    bucket.m_Populated = true;
                    new(&bucket.m_Key) K(std::move(key));
                    new(&bucket.m_Value) V(std::forward<Args>(args)...);
                    return {&bucket.m_Key, &bucket.m_Value};
                } else if (bucket.m_Key == key) {
                    bucket.m_Value.~V();
                    new(&bucket.m_Value) V(std::forward<Args>(args)...);
                    return {&bucket.m_Key, &bucket.m_Value};
                }
            }
            // No free buckets!
            Reserve(Capacity()*3/2+1);
            return Emplace(std::move(key), std::forward<Args>(args)...);
        }

        PairRef Find(const K& key)
        {
            size_t hash = std::hash<K>{}(key);
            size_t startIdx = hash % m_Buckets.Size();
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                BucketType& bucket = m_Buckets[(startIdx+i)%m_Buckets.Size()];
                if (!bucket.m_Populated)
                    continue;
                else if (bucket.m_Key == key)
                    return {&bucket.m_Key, &bucket.m_Value};
            }
            return {};
        }

        ConstPairRef Find(const K& key) const
        {
            size_t hash = std::hash<K>{}(key);
            size_t startIdx = hash % m_Buckets.Size();
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                const BucketType& bucket = m_Buckets[(startIdx+i)%m_Buckets.Size()];
                if (!bucket.m_Populated)
                    continue;
                else if (bucket.m_Key == key)
                    return {&bucket.m_Key, &bucket.m_Value};
            }
            return {};
        }

        void Remove(const K& key)
        {
            size_t hash = std::hash<K>{}(key);
            size_t startIdx = hash % m_Buckets.Size();
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                BucketType& bucket = m_Buckets[(startIdx+i)%m_Buckets.Size()];
                if (!bucket.m_Populated)
                    continue;
                else if (bucket.m_Key == key) {
                    bucket.Clear();
                    return;
                }
            }
        }

        void ReHash()
        {
            List<BucketType> buckets(std::move(m_Buckets));
            m_Buckets.Reserve(buckets.Size());
            m_Buckets.Resize(buckets.Size());
            for (size_t i = 0; i < buckets.Size(); i++) {
                if (buckets[i].m_Populated)
                    Emplace(std::move(buckets[i].Key), std::move(buckets[i].Value));
            }
        }

        void ForEach(std::function<void(const BucketType&)> fn) const
        {
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                if (m_Buckets[i].m_Populated)
                    fn(m_Buckets[i]);
            }
        }

        void ForEach(std::function<void(BucketType&)> fn)
        {
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                if (m_Buckets[i].m_Populated)
                    fn(m_Buckets[i]);
            }
        }

        void Clear() { m_Buckets.Clear(); }

        size_t Count() const
        {
            size_t count = 0;
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                if (m_Buckets[i].m_Populated)
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
