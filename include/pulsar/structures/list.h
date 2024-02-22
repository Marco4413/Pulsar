#ifndef _PULSAR_STRUCTURES_LIST_H
#define _PULSAR_STRUCTURES_LIST_H

#include "pulsar/core.h"

namespace Pulsar
{
    template<typename T>
    class List
    {
    public:
        typedef List<T> SelfType;

        List() = default;
        List(size_t initCapacity) { Reserve(initCapacity); }

        ~List()
        {
            if (!m_Data)
                return;
            Clear();
            PULSAR_FREE((void*)m_Data);
        }

        List(const SelfType& other) { *this = other; }
        SelfType& operator=(const SelfType& other)
        {
            size_t oldSize = m_Size;
            ResizeUninitialized(other.Size());
            for (size_t i = 0; i < other.Size(); i++) {
                if (i < oldSize)
                    m_Data[i].~T();
                new(&m_Data[i]) T(other.m_Data[i]);
            }
            return *this;
        }

        List(SelfType&& other) { *this = std::move(other); }
        SelfType& operator=(SelfType&& other)
        {
            size_t oldSize = m_Size;
            ResizeUninitialized(other.Size());
            for (size_t i = 0; i < other.Size(); i++) {
                if (i < oldSize)
                    m_Data[i].~T();
                new(&m_Data[i]) T(std::move(other.m_Data[i]));
                other.m_Data[i].~T();
            }
            other.m_Size = 0;
            return *this;
        }

        template<typename ...Args>
        void Resize(size_t newSize, Args ...args)
        {
            size_t oldSize = m_Size;
            ResizeUninitialized(newSize);
            if (oldSize < m_Size) {
                for (size_t i = oldSize; i < m_Size; i++)
                    new(&m_Data[i]) T(args...);
                return;
            }
            
            for (size_t i = m_Size; i < oldSize; i++)
                m_Data[i].~T();
        }

        void Reserve(size_t newCapacity)
        {
            if (m_Data && newCapacity <= m_Capacity)
                return;
            T* newData = (T*)PULSAR_MALLOC(sizeof(T) * newCapacity);
            for (size_t i = 0; i < m_Size; i++) {
                new(&newData[i]) T(std::move(m_Data[i]));
                m_Data[i].~T();
            }
            PULSAR_FREE((void*)m_Data);
            m_Data = newData;
            m_Capacity = newCapacity;
        }

        void PushBack(const T& value)
        {
            size_t appendIdx = m_Size;
            ResizeUninitialized(m_Size+1);
            new(&m_Data[appendIdx]) T(value);
        }

        void PushBack(T&& value)
        {
            size_t appendIdx = m_Size;
            ResizeUninitialized(m_Size+1);
            new(&m_Data[appendIdx]) T(std::move(value));
        }

        template<typename ...Args>
        T& EmplaceBack(Args&& ...args)
        {
            size_t appendIdx = m_Size;
            ResizeUninitialized(m_Size+1);
            new(&m_Data[appendIdx]) T(args...);
            return m_Data[appendIdx];
        }

        void PopBack() { m_Data[--m_Size].~T(); }

        T& operator[](size_t index)             { return m_Data[index]; }
        const T& operator[](size_t index) const { return m_Data[index]; }

        T& Back()              { return m_Data[m_Size-1]; }
        const T& Back() const  { return m_Data[m_Size-1]; }

        void Clear()
        {
            for (size_t i = 0; i < m_Size; i++)
                m_Data[i].~T();
            m_Size = 0;
        }

        const T* Data() const   { return m_Data; }
        size_t Size() const     { return m_Size; }
        size_t Capacity() const { return m_Capacity; }
        bool IsEmpty() const    { return m_Size == 0; }
    protected:
        void ResizeUninitialized(size_t newSize)
        {
            if (!m_Data || newSize > m_Capacity)
                Reserve(newSize*3/2);
            m_Size = newSize;
        }

    private:
        T* m_Data = nullptr;
        size_t m_Size = 0;
        size_t m_Capacity = 0;
    };
}

#endif // _PULSAR_STRUCTURES_LIST_H
