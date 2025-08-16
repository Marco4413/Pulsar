#ifndef _PULSAR_STRUCTURES_REF_H
#define _PULSAR_STRUCTURES_REF_H

#include "pulsar/core.h"

namespace Pulsar
{
    struct RefCount
    {
        PULSAR_ATOMIC_SIZE_T SharedRefs = 0;
    };

    template <typename T>
    struct RefCountedValue
    {
        // This field MUST always come first so that calling free
        //  on a malloc'd block frees the whole struct
        Pulsar::RefCount RefCount;
        T Value;
    };

    template <typename T>
    class SharedRef
    {
    public:
        template<typename U>
        friend class SharedRef;

        template<typename ...Args>
        static SharedRef<T> New(Args&& ...args)
        {
            // Using PULSAR_MALLOC to make sure that PULSAR_FREE can be called on the block
            auto storage = static_cast<RefCountedValue<T>*>(PULSAR_MALLOC(sizeof(RefCountedValue<T>)));
            PULSAR_PLACEMENT_NEW(RefCount, &storage->RefCount, (size_t)0);
            PULSAR_PLACEMENT_NEW(T, &storage->Value, std::forward<Args>(args)...);
            SharedRef<T> ref(&storage->RefCount, &storage->Value);
            return ref;
        }

        SharedRef()
            : m_RefCount(nullptr), m_Value(nullptr) { }

        SharedRef(std::nullptr_t null)
            : SharedRef() { (void)null; }
        
        SharedRef(const SharedRef<T>& o)
            : SharedRef()
        {
            *this = o;
        }

        SharedRef(SharedRef<T>&& o)
            : SharedRef()
        {
            *this = std::move(o);
        }

        ~SharedRef()
        {
            DecrementRefCount();
        }

        SharedRef<T>& operator=(const SharedRef<T>& o)
        {
            o.IncrementRefCount();
            this->DecrementRefCount();
            m_Value = o.m_Value;
            m_RefCount = o.m_RefCount;
            return *this;
        }

        SharedRef<T>& operator=(SharedRef<T>&& o)
        {
            m_Value = o.m_Value;
            m_RefCount = o.m_RefCount;
            o.m_Value = nullptr;
            o.m_RefCount = nullptr;
            return *this;
        }

        T* operator->() const { return m_Value; }
        T& operator*() const
        {
            PULSAR_ASSERT(m_Value, "Dereferencing nullptr.");
            return *m_Value;
        }

        T& operator[](size_t idx) const
        {
            PULSAR_ASSERT(m_Value, "Indexing nullptr.");
            return m_Value[idx];
        }

        template<typename U>
        bool operator==(const SharedRef<U>& o) const { return m_Value == o.m_Value; }
        template<typename U>
        bool operator!=(const SharedRef<U>& o) const { return m_Value != o.m_Value; }
        operator bool() const { return (bool)m_RefCount; }

        template<typename U>
        operator SharedRef<U>() const { return CastTo<U>(); }

        // Create a new SharedRef sharing the same RefCount
        template<typename U>
        SharedRef<U> CastTo() const
        {
            SharedRef<U> castedRef(m_RefCount, dynamic_cast<U*>(m_Value));
            return castedRef;
        }

        T* Get() const { return m_Value; }
        size_t SharedCount() const
        {
            return m_RefCount ? (size_t)m_RefCount->SharedRefs : 1;
        }

    private:
        explicit SharedRef(RefCount* refCount, T* value)
            : m_RefCount(refCount), m_Value(value)
        {
            IncrementRefCount();
        }

        void IncrementRefCount() const
        {
            if (m_RefCount)
                m_RefCount->SharedRefs++;
        }

        void DecrementRefCount()
        {
            if (m_RefCount && m_RefCount->SharedRefs-- <= 1) {
                (*m_RefCount).~RefCount();
                (*m_Value).~T();
                PULSAR_FREE(m_RefCount);
                m_RefCount = nullptr;
                m_Value = nullptr;
            }
        }

    private:
        RefCount* m_RefCount;
        T* m_Value;
    };
}

template<typename T>
struct std::hash<Pulsar::SharedRef<T>>
{
    size_t operator()(const Pulsar::SharedRef<T>& ref) const
    {
        return std::hash<void*>{}((void*)ref.Get());
    }
};

#endif // _PULSAR_STRUCTURES_REF_H
