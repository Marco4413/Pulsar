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
        using Self = HashMapBucket<K, V>;
        using Map = HashMap<K, V>;
        friend Map;

        HashMapBucket()
            : m_KeyStorage{}, m_ValueStorage{} { };
        ~HashMapBucket() { Delete(); }

        HashMapBucket(const Self& other)
        {
            if (other.IsPopulated()) {
                Fill(other.m_Key, other.m_Value);
            } else {
                m_IsFilled = other.m_IsFilled;
                m_IsDeleted = other.m_IsDeleted;
            }
        }

        HashMapBucket(Self&& other)
        {
            if (other.IsPopulated()) {
                Fill(std::move(other.m_Key), std::move(other.m_Value));
            } else {
                m_IsFilled = other.m_IsFilled;
                m_IsDeleted = other.m_IsDeleted;
            }
            other.Delete();
        }

        Self& operator=(const Self& other) = delete;
        Self& operator=(Self&& other) = delete;

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
        using Self = HashMap<K, V>;
        using Bucket = HashMapBucket<K, V>;
        // Forward Declarations
        class ConstIterator;
        class MutableIterator;

        struct Pair
        {
            K Key;
            V Value;
        };

        HashMap() { Clear(); }
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

            List<Bucket> buckets(std::move(m_Buckets));
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

        Bucket& Insert(const K& key, const V& value) { return Emplace(key, value); }
        Bucket& Insert(const K& key, V&& value)      { return Emplace(key, std::move(value)); }
        Bucket& Insert(K&& key, const V& value)      { return Emplace(std::move(key), value); }
        Bucket& Insert(K&& key, V&& value)           { return Emplace(std::move(key), std::move(value)); }

        template<typename ...Args>
        Bucket& Emplace(const K& key, Args&& ...args) { K _k = key; return Emplace(std::move(_k), std::forward<Args>(args)...); }

        template<typename ...Args>
        Bucket& Emplace(K&& key, Args&& ...args)
        {
            size_t keyHash = HashKey(key);
            for (size_t probeIdx = 0; probeIdx < m_Buckets.Size(); probeIdx++) {
                size_t hash = HashProbe(keyHash, probeIdx);
                Bucket& bucket = m_Buckets[hash];
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

            return Emplace(std::move(key), std::forward<Args>(args)...);
        }

        /**
         * Returns nullptr if key is not in this map.
         * If the returned value is not nullptr then the method Bucket::IsPopulated() must return true.
         * Which means that the value returned by the Value() method is valid.
         */
        Bucket* Find(const K& key)
        {
            size_t keyHash = HashKey(key);
            for (size_t probeIdx = 0; probeIdx < m_Buckets.Size(); probeIdx++) {
                size_t hash = HashProbe(keyHash, probeIdx);
                Bucket& bucket = m_Buckets[hash];
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

        const Bucket* Find(const K& key) const
        {
            size_t keyHash = HashKey(key);
            for (size_t probeIdx = 0; probeIdx < m_Buckets.Size(); probeIdx++) {
                size_t hash = HashProbe(keyHash, probeIdx);
                const Bucket& bucket = m_Buckets[hash];
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
            Bucket* bucket = Find(key);
            if (bucket) {
                PULSAR_ASSERT(bucket->IsPopulated(), "HashMap::Find returned a non-populated bucket.");
                bucket->Delete();
                return true;
            }
            return false;
        }

        void ReHash()
        {
            List<Bucket> buckets(std::move(m_Buckets));
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

        void ForEach(std::function<void(const Bucket&)> fn) const
        {
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                if (m_Buckets[i].IsPopulated())
                    fn(m_Buckets[i]);
            }
        }

        void ForEach(std::function<void(Bucket&)> fn)
        {
            for (size_t i = 0; i < m_Buckets.Size(); i++) {
                if (m_Buckets[i].IsPopulated())
                    fn(m_Buckets[i]);
            }
        }

        void Clear()
        {
            m_Buckets.Clear();
            m_Buckets.Resize(1);
        }

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

        ConstIterator Begin() const
        {
            auto it = m_Buckets.Begin();
            for (; it != m_Buckets.End() && !(*it).IsPopulated(); ++it);
            return ConstIterator(it, m_Buckets.End());
        }

        ConstIterator End() const { return ConstIterator(m_Buckets.End(), m_Buckets.End()); }

        MutableIterator Begin()
        {
            auto it = m_Buckets.Begin();
            for (; it != m_Buckets.End() && !(*it).IsPopulated(); ++it);
            return MutableIterator(it, m_Buckets.End());
        }

        MutableIterator End() { return MutableIterator(m_Buckets.End(), m_Buckets.End()); }

        PULSAR_ITERABLE_IMPL(Self, ConstIterator, MutableIterator)

    public:
        class ConstIterator
        {
        public:
            using BucketIterator = List<Bucket>::ConstIterator;
            struct Pair
            {
                const K& Key;
                const V& Value;
            };

            explicit ConstIterator(BucketIterator begin, BucketIterator end)
                : m_Begin(begin), m_End(end) {}

            bool operator==(const ConstIterator& other) const = default;
            bool operator!=(const ConstIterator& other) const = default;
            Pair operator*() const
            {
                PULSAR_ASSERT(m_Begin && m_Begin->IsPopulated(), "Called *HashMap<K, V>::ConstIterator on invalid data.");
                return { m_Begin->Key(), m_Begin->Value() };
            }

            ConstIterator& operator++()
            {
                PULSAR_ASSERT(m_Begin != m_End, "Called ++HashMap<K, V>::ConstIterator on complete iterator.");
                for (++m_Begin; m_Begin != m_End && !m_Begin->IsPopulated(); ++m_Begin);
                return *this;
            }

            ConstIterator operator++(int)
            {
                ConstIterator ret = *this;
                ++(*this);
                return ret;
            }
        private:
            BucketIterator m_Begin;
            BucketIterator m_End;
        };

        class MutableIterator
        {
        public:
            using BucketIterator = List<Bucket>::MutableIterator;
            struct Pair
            {
                const K& Key;
                V& Value;
            };

            explicit MutableIterator(BucketIterator begin, BucketIterator end)
                : m_Begin(begin), m_End(end) {}

            bool operator==(const MutableIterator& other) const = default;
            bool operator!=(const MutableIterator& other) const = default;
            Pair operator*()
            {
                PULSAR_ASSERT(m_Begin && m_Begin->IsPopulated(), "Called *HashMap<K, V>::MutableIterator on invalid data.");
                return { m_Begin->Key(), m_Begin->Value() };
            }
            ConstIterator::Pair operator*() const
            {
                PULSAR_ASSERT(m_Begin && m_Begin->IsPopulated(), "Called *HashMap<K, V>::MutableIterator on invalid data.");
                return { m_Begin->Key(), m_Begin->Value() };
            }

            MutableIterator& operator++()
            {
                PULSAR_ASSERT(m_Begin != m_End, "Called ++HashMap<K, V>::MutableIterator on complete iterator.");
                for (++m_Begin; m_Begin != m_End && !m_Begin->IsPopulated(); ++m_Begin);
                return *this;
            }

            MutableIterator operator++(int)
            {
                MutableIterator ret = *this;
                ++(*this);
                return ret;
            }
        private:
            BucketIterator m_Begin;
            BucketIterator m_End;
        };

    private:
        List<Bucket> m_Buckets;
    };
}

#endif // _PULSAR_STRUCTURES_HASHMAP_H
