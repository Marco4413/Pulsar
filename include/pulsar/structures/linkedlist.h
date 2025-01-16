#ifndef _PULSAR_STRUCTURES_LINKEDLIST_H
#define _PULSAR_STRUCTURES_LINKEDLIST_H

#include "pulsar/core.h"

#include "pulsar/structures/list.h"

namespace Pulsar
{
    template<typename T>
    class LinkedList; // Forward Declaration

    template<typename T>
    class LinkedListNode
    {
    public:
        friend LinkedList<T>;
        using Self = LinkedListNode<T>;
        
        template<typename ...Args>
        LinkedListNode(Args&& ...init)
            : m_Value(init...) { }
        ~LinkedListNode() { PULSAR_DELETE(Self, m_Next); }

        // Delete Copy & Move Constructors/Assignments
        LinkedListNode(const Self&) = delete;
        Self& operator=(const Self&) = delete;
        LinkedListNode(Self&&) = delete;
        Self& operator=(Self&&) = delete;

        size_t Length() const
        {
            size_t length = 1;
            const Self* next = Next();
            while (next) {
                length++;
                next = next->Next();
            }
            return length;
        }

        T& Value()             { return m_Value; }
        const T& Value() const { return m_Value; }

        Self* Prev()             { return m_Prev; }
        const Self* Prev() const { return m_Prev; }

        Self* Next()             { return m_Next; }
        const Self* Next() const { return m_Next; }

        bool HasPrev() const { return m_Prev; }
        bool HasNext() const { return m_Next; }
    private:
        T m_Value;
        Self* m_Prev = nullptr;
        Self* m_Next = nullptr;
    };

    template<typename T>
    class List; // Forward Declaration

    template<typename T>
    class LinkedList
    {
    public:
        using Self = LinkedList<T>;
        using Node = LinkedListNode<T>;

        LinkedList() = default;
        ~LinkedList() { PULSAR_DELETE(Node, m_Start); }

        explicit LinkedList(const List<T>& list)
            : LinkedList()
        {
            for (size_t i = 0; i < list.Size(); i++)
                Append(list[i]);
        }

        explicit LinkedList(List<T>&& list)
            : LinkedList()
        {
            for (size_t i = 0; i < list.Size(); i++)
                Append(std::move(list[i]));
            list.Clear();
        }

        LinkedList(const Self& other) { *this = other; }
        Self& operator=(const Self& other)
        {
            if (m_Start)
                PULSAR_DELETE(Node, m_Start);
            m_Start = nullptr;
            m_End = nullptr;
            Node* next = other.m_Start;
            while (next) {
                Append()->m_Value = next->m_Value;
                next = next->m_Next;
            }
            return *this;
        }

        LinkedList(Self&& other) { *this = std::move(other); }
        Self& operator=(Self&& other)
        {
            if (m_Start)
                PULSAR_DELETE(Node, m_Start);
            m_Start = other.m_Start;
            m_End = other.m_End;
            other.m_Start = nullptr;
            other.m_End = nullptr;
            return *this;
        }

        template<typename ...Args>
        Node* Prepend(Args&& ...init)
        {
            if (!m_Start) {
                PULSAR_ASSERT(!m_End, "LinkedList has an end but doesn't have a start.");
                m_Start = PULSAR_NEW(Node, std::forward<Args>(init)...);
                m_End = m_Start;
                return m_Start;
            }

            Node* newStart = PULSAR_NEW(Node, std::forward<Args>(init)...);
            newStart->m_Next = m_Start;
            m_Start->m_Prev = newStart;

            m_Start = newStart;
            return m_Start;
        }

        template<typename ...Args>
        Node* Append(Args&& ...init)
        {
            if (!m_Start) {
                PULSAR_ASSERT(!m_End, "LinkedList has an end but doesn't have a start.");
                m_Start = PULSAR_NEW(Node, std::forward<Args>(init)...);
                m_End = m_Start;
                return m_Start;
            }

            Node* newEnd = PULSAR_NEW(Node, std::forward<Args>(init)...);
            newEnd->m_Prev = m_End;
            m_End->m_Next = newEnd;

            m_End = newEnd;
            return m_End;
        }

        Self& Concat(Self&& other)
        {
            if (!other.m_Start) {
                return *this;
            } else if (!m_Start) {
                m_Start = other.m_Start;
                m_End = other.m_End;
            } else {
                m_End->m_Next = other.m_Start;
                other.m_Start->m_Prev = m_End;
                m_End = other.m_End;
            }

            other.m_Start = nullptr;
            other.m_End = nullptr;

            return *this;
        }

        Self& RemoveFront(size_t n)
        {
            if (n == 0)
                return *this;
            // This is the node before the new starting one
            Node* newStart = m_Start;
            for (size_t i = 0; i < n && newStart; i++)
                newStart = newStart->m_Next;

            if (!newStart) {
                PULSAR_DELETE(Node, m_Start);
                m_Start = nullptr;
                m_End = nullptr;
                return *this;
            }

            // Unlink
            PULSAR_ASSERT(newStart->m_Prev, "New LinkedList head has no previous element linked.");
            newStart->m_Prev->m_Next = nullptr;
            newStart->m_Prev = nullptr;

            // Delete previous nodes
            PULSAR_DELETE(Node, m_Start);
            m_Start = newStart;

            return *this;
        }

        Node* Front()             { return m_Start; }
        const Node* Front() const { return m_Start; }
        Node* Back()              { return m_End; }
        const Node* Back() const  { return m_End; }

        void Clear()          { PULSAR_DELETE(Node, m_Start); m_Start = nullptr; m_End = nullptr; }
        size_t Length() const { return m_Start ? m_Start->Length() : 0; }
    private:
        Node* m_Start = nullptr;
        Node* m_End = nullptr;
    };
}

#endif // _PULSAR_STRUCTURES_LINKEDLIST_H
