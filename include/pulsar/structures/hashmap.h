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
        ~HashMapBucket() { Delete(); }

        HashMapBucket(const SelfType& other)
        {
            if (other.IsPopulated()) {
                Fill(other.m_Key, other.m_Value);
            } else {
                m_IsFilled = other.m_IsFilled;
                m_IsDeleted = other.m_IsDeleted;
            }
        }

        HashMapBucket(SelfType&& other)
        {
            if (other.IsPopulated()) {
                Fill(std::move(other.m_Key), std::move(other.m_Value));
            } else {
                m_IsFilled = other.m_IsFilled;
                m_IsDeleted = other.m_IsDeleted;
            }
            other.Delete();
        }

        SelfType& operator=(const SelfType& other) = delete;
        SelfType& operator=(SelfType&& other) = delete;

        void Delete()
        {
            if (IsPopulated()) {
                m_Key.~K();
                m_Value.~V();
            }
            m_IsDeleted = true;
        }

        bool IsDeleted() const { return m_IsDeleted; }
        bool IsPopulated() const { return m_IsFilled && !m_IsDeleted; }

        operator bool() const { return IsPopulated(); }

        // V& operator*() { return Value(); }
        // const V& operator*() const { return Value(); }

        const K& Key() const
        {
            PULSAR_ASSERT(IsPopulated(), "Trying to get the key of an empty HashMapBucket.");
            return m_Key;
        }

        V& Value()
        {
            PULSAR_ASSERT(IsPopulated(), "Trying to get the value of an empty HashMapBucket.");
            return m_Value;
        }

        const V& Value() const
        {
            PULSAR_ASSERT(IsPopulated(), "Trying to get the value of an empty HashMapBucket.");
            return m_Value;
        }

    protected:
        template<typename ...Args>
        void Fill(const K& key, Args&& ...args) { K _k = key; Fill(std::move(_k), std::forward<Args>(args)...); }

        template<typename ...Args>
        void Fill(K&& key, Args&& ...args)
        {
            if (IsPopulated()) {
                Delete();
            }

            m_IsFilled = true;
            m_IsDeleted = false;

            PULSAR_PLACEMENT_NEW(K, &m_Key, std::move(key));
            PULSAR_PLACEMENT_NEW(V, &m_Value, std::forward<Args>(args)...);
        }

    private:
        bool m_IsFilled  = false;
        bool m_IsDeleted = false;
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

            List<BucketType> buckets(std::move(m_Buckets));
            m_Buckets.Reserve(newCapacity);
            m_Buckets.Resize(newCapacity);
            for (size_t i = 0; i < buckets.Size(); i++) {
                if (buckets[i].IsPopulated()) {
                    Emplace(
                        std::move(buckets[i].m_Key),
                        std::move(buckets[i].m_Value));
                }
            }
        }

        BucketType& Insert(const K& key, const V& value) { return Emplace(key, value); }
        BucketType& Insert(const K& key, V&& value)      { return Emplace(key, std::move(value)); }
        BucketType& Insert(K&& key, const V& value)      { return Emplace(std::move(key), value); }
        BucketType& Insert(K&& key, V&& value)           { return Emplace(std::move(key), std::move(value)); }

        template<typename ...Args>
        BucketType& Emplace(const K& key, Args&& ...args) { K _k = key; return Emplace(std::move(_k), std::forward<Args>(args)...); }

        template<typename ...Args>
        BucketType& Emplace(K&& key, Args&& ...args)
        {
            for (int attempt = 0; attempt < 2; attempt++) {
                size_t keyHash = HashKey(key);
                for (size_t probeIdx = 0; probeIdx < m_Buckets.Size(); probeIdx++) {
                    size_t hash = HashProbe(keyHash, probeIdx);
                    BucketType& bucket = m_Buckets[hash];
                    if (!bucket.IsPopulated()) {
                        bucket.Fill(
                            std::move(key),
                            std::forward<Args>(args)...);
                        return bucket;
                    } else if (bucket.Key() == key) {
                        bucket.m_Value.~V();
                        PULSAR_PLACEMENT_NEW(V, &bucket.m_Value, std::forward<Args>(args)...);
                        return bucket;
                    }
                }

                // No free buckets! Reserve and repeat.
                Reserve(Capacity()*3/2+1);
            }

            PULSAR_ASSERT(false, "Failed to insert value into HashMap.");
        }

        /**
         * Returns nullptr if key is not in this map.
         * If the returned value is not nullptr then the method BucketType::IsPopulated() must return true.
         * Which means that the value returned by the Value() method is valid.
         */
        BucketType* Find(const K& key)
        {
            size_t keyHash = HashKey(key);
            for (size_t probeIdx = 0; probeIdx < m_Buckets.Size(); probeIdx++) {
                size_t hash = HashProbe(keyHash, probeIdx);
                BucketType& bucket = m_Buckets[hash];
                if (bucket.IsDeleted()) {
                    continue;
                } else if (!bucket.IsPopulated()) {
                    break;
                } else if (bucket.Key() == key) {
                    return &bucket;
                }
            }
            return nullptr;
        }

        const BucketType* Find(const K& key) const
        {
            size_t keyHash = HashKey(key);
            for (size_t probeIdx = 0; probeIdx < m_Buckets.Size(); probeIdx++) {
                size_t hash = HashProbe(keyHash, probeIdx);
                const BucketType& bucket = m_Buckets[hash];
                if (bucket.IsDeleted()) {
                    continue;
                } else if (!bucket.IsPopulated()) {
                    break;
                } else if (bucket.Key() == key) {
                    return &bucket;
                }
            }
            return nullptr;
        }

        bool Remove(const K& key)
        {
            BucketType* bucket = Find(key);
            if (bucket) {
                PULSAR_ASSERT(bucket->IsPopulated(), "HashMap::Find returned a non-populated bucket.");
                bucket->Delete();
                return true;
            }
            return false;
        }

        void ReHash()
        {
            List<BucketType> buckets(std::move(m_Buckets));
            m_Buckets.Reserve(buckets.Size());
            m_Buckets.Resize(buckets.Size());
            for (size_t i = 0; i < buckets.Size(); i++) {
                if (buckets[i].IsPopulated()) {
                    Emplace(
                        std::move(buckets[i].m_Key),
                        std::move(buckets[i].m_Value));
                }
            }
        }

        void ForEach(std::function<void(const BucketType&)> fn) const
        {
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                if (m_Buckets[i].IsPopulated())
                    fn(m_Buckets[i]);
            }
        }

        void ForEach(std::function<void(BucketType&)> fn)
        {
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                if (m_Buckets[i].IsPopulated())
                    fn(m_Buckets[i]);
            }
        }

        void Clear() { m_Buckets.Clear(); }

        size_t Count() const
        {
            size_t count = 0;
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                if (m_Buckets[i].IsPopulated())
                    count++;
            }
            return count;
        }

        size_t Capacity() const { return m_Buckets.Size(); }

        /** Computes the full hash from a specific key and probe index. */
        size_t Hash(const K& key, size_t probeIdx) const {
            return HashProbe(HashKey(key), probeIdx);
        }

        /** Computes the hash for a specific key. */
        size_t HashKey(const K& key) const {
            size_t keyHash = std::hash<K>{}(key);
            return keyHash % Capacity();
        }

        /** Computes the hash for a specific probe of a key hash. */
        size_t HashProbe(size_t keyHash, size_t probeIdx) const {
            // TODO: Use a better probing method.
            // See: https://en.wikipedia.org/wiki/Open_addressing
            return (keyHash + probeIdx) % Capacity();
        }
    private:
        List<BucketType> m_Buckets;
    };
}

#endif // _PULSAR_STRUCTURES_HASHMAP_H
