#ifndef _PULSAR_STRUCTURES_REF_H
#define _PULSAR_STRUCTURES_REF_H

#include "pulsar/core.h"

namespace Pulsar
{
    namespace Ref
    {
        struct RefCount
        {
            std::atomic_size_t SharedRefs = 0;
        };
    }

    template <typename T>
    class SharedRef
    {
    public:
        template<typename U>
        friend class SharedRef;

        template<typename ...Args>
        static SharedRef<T> New(Args&& ...args)
        {
            SharedRef<T> ref(PULSAR_NEW(T, std::forward<Args>(args)...));
            return ref;
        }

        SharedRef()
            : m_Ptr(nullptr), m_RefCount(nullptr) { }

        SharedRef(nullptr_t null)
            : SharedRef<T>() { (void)null; }
        
        SharedRef(const SharedRef<T>& o) { *this = o; };
        SharedRef(SharedRef<T>&& o) { *this = std::move(o); };
        
        ~SharedRef()
        {
            DecrementRefCount();
        }

        SharedRef<T>& operator=(const SharedRef<T>& o)
        {
            o.IncrementRefCount();
            this->DecrementRefCount();
            m_Ptr = o.m_Ptr;
            m_RefCount = o.m_RefCount;
            return *this;
        }

        SharedRef<T>& operator=(SharedRef<T>&& o)
        {
            m_Ptr = o.m_Ptr;
            m_RefCount = o.m_RefCount;
            o.m_Ptr = nullptr;
            o.m_RefCount = nullptr;
            return *this;
        }

        T* operator->() const { return m_Ptr; }
        T& operator*() const { return *m_Ptr; }
        T& operator[](size_t idx) const { return m_Ptr[idx]; }

        template<typename U>
        bool operator==(const SharedRef<U>& o) const { return m_Ptr == o.m_Ptr; }
        template<typename U>
        bool operator!=(const SharedRef<U>& o) const { return m_Ptr != o.m_Ptr; }
        operator bool() const { return (bool)m_Ptr; }

        template<typename U>
        operator SharedRef<U>() const { return CastTo<U>(); }

        // Create a new SharedRef sharing the same RefCount
        template<typename U>
        SharedRef<U> CastTo() const
        {
            SharedRef<U> sRef(dynamic_cast<U*>(m_Ptr), m_RefCount);
            return sRef;
        }

        T* Get() const { return m_Ptr; }

    private:
        explicit SharedRef(T* ptr)
            : m_Ptr(ptr), m_RefCount(PULSAR_NEW(Ref::RefCount))
        {
            IncrementRefCount();
        }

        explicit SharedRef(T* ptr, Ref::RefCount* refs)
            : m_Ptr(ptr), m_RefCount(refs)
        {
            IncrementRefCount();
        }

        void IncrementRefCount() const
        {
            if (m_RefCount)
                m_RefCount->SharedRefs.fetch_add(1);
        }

        void DecrementRefCount()
        {
            if (m_RefCount && m_RefCount->SharedRefs.fetch_sub(1) <= 1) {
                PULSAR_DELETE(T, m_Ptr);
                PULSAR_DELETE(Ref::RefCount, m_RefCount);
                m_Ptr = nullptr;
                m_RefCount = nullptr;
            }
        }

    private:
        T* m_Ptr = nullptr;
        Ref::RefCount* m_RefCount = nullptr;
    };
}

#endif // _PULSAR_STRUCTURES_REF_H
