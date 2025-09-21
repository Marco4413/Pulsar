#ifndef _PULSAR_STRUCTURES_LIST_H
#define _PULSAR_STRUCTURES_LIST_H

#include "pulsar/core.h"

#include "pulsar/structures/linkedlist.h"

namespace Pulsar
{
    template<typename T>
    class LinkedList; // Forward Declaration

    template<typename T>
    class List
    {
    public:
        using Self = List<T>;

        List()
            : m_Data(nullptr)
            , m_Size(0)
            , m_Capacity(0)
        {}

        List(size_t initCapacity)
            : List()
        {
            Reserve(initCapacity);
        }

        List(std::initializer_list<T> init)
            : List(init.size())
        {
            for (auto it = init.begin(); it != init.end(); it++)
                PushBack(*it);
        }

        ~List()
        {
            if (!m_Data)
                return;
            Clear();
            PULSAR_FREE((void*)m_Data);
        }
        
        explicit List(const LinkedList<T>& ll)
            : List()
        {
            const LinkedListNode<T>* next = ll.Front();
            while (next) {
                EmplaceBack(next->Value());
                next = next->Next();
            }
        }

        explicit List(LinkedList<T>&& ll)
            : List()
        {
            LinkedListNode<T>* next = ll.Front();
            while (next) {
                EmplaceBack(std::move(next->Value()));
                next = next->Next();
            }
            ll.Clear();
        }

        List(const Self& other)
            : List()
        {
            *this = other;
        }

        List(Self&& other)
            : List()
        {
            *this = std::move(other);
        }

        Self& operator=(const Self& other)
        {
            size_t oldSize = m_Size;
            ResizeUninitialized(other.Size());
            for (size_t i = 0; i < other.Size(); i++) {
                if (i < oldSize)
                    m_Data[i].~T();
                PULSAR_PLACEMENT_NEW(T, &m_Data[i], other.m_Data[i]);
            }
            return *this;
        }

        Self& operator=(Self&& other)
        {
            size_t oldSize = m_Size;
            ResizeUninitialized(other.Size());
            for (size_t i = 0; i < other.Size(); i++) {
                if (i < oldSize)
                    m_Data[i].~T();
                PULSAR_PLACEMENT_NEW(T, &m_Data[i], std::move(other.m_Data[i]));
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
                    PULSAR_PLACEMENT_NEW(T, &m_Data[i], args...);
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
                PULSAR_PLACEMENT_NEW(T, &newData[i], std::move(m_Data[i]));
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
            PULSAR_PLACEMENT_NEW(T, &m_Data[appendIdx], value);
        }

        void PushBack(T&& value)
        {
            size_t appendIdx = m_Size;
            ResizeUninitialized(m_Size+1);
            PULSAR_PLACEMENT_NEW(T, &m_Data[appendIdx], std::move(value));
        }

        template<typename ...Args>
        T& EmplaceBack(Args&& ...args)
        {
            size_t appendIdx = m_Size;
            ResizeUninitialized(m_Size+1);
            PULSAR_PLACEMENT_NEW(T, &m_Data[appendIdx], std::forward<Args>(args)...);
            return m_Data[appendIdx];
        }

        void PopBack()
        {
            PULSAR_ASSERT(!IsEmpty(), "Popping values from an empty list.");
            m_Data[--m_Size].~T();
        }

        T& operator[](size_t index)
        {
            PULSAR_ASSERT(index < Size(), "List index out of bounds.");
            return m_Data[index];
        }

        const T& operator[](size_t index) const
        {
            PULSAR_ASSERT(index < Size(), "List index out of bounds.");
            return m_Data[index];
        }

        T& Back()             { return (*this)[m_Size-1]; }
        const T& Back() const { return (*this)[m_Size-1]; }

        void Clear()
        {
            for (size_t i = 0; i < m_Size; i++)
                m_Data[i].~T();
            m_Size = 0;
        }

        T* Data()               { return m_Data; }
        const T* Data() const   { return m_Data; }
        size_t Size() const     { return m_Size; }
        size_t Capacity() const { return m_Capacity; }
        bool IsEmpty() const    { return m_Size == 0; }
    protected:
        void ResizeUninitialized(size_t newSize)
        {
            if (!m_Data || newSize > m_Capacity)
                Reserve(newSize*3/2+1);
            m_Size = newSize;
        }

    private:
        T* m_Data;
        size_t m_Size;
        size_t m_Capacity;
    };
}

#endif // _PULSAR_STRUCTURES_LIST_H
