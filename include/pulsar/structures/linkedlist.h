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
        typedef LinkedListNode<T> SelfType;
        
        template<typename ...Args>
        LinkedListNode(Args&& ...init)
            : m_Value(init...) { }
        ~LinkedListNode() { PULSAR_DELETE(SelfType, m_Next); }

        // Delete Copy & Move Constructors/Assignments
        LinkedListNode(const SelfType&) = delete;
        SelfType& operator=(const SelfType&) = delete;
        LinkedListNode(SelfType&&) = delete;
        SelfType& operator=(SelfType&&) = delete;

        size_t Length() const
        {
            size_t length = 1;
            const SelfType* next = Next();
            while (next) {
                length++;
                next = next->Next();
            }
            return length;
        }

        T& Value()             { return m_Value; }
        const T& Value() const { return m_Value; }

        SelfType* Prev()             { return m_Prev; }
        const SelfType* Prev() const { return m_Prev; }

        SelfType* Next()             { return m_Next; }
        const SelfType* Next() const { return m_Next; }

        bool HasPrev() const { return m_Prev; }
        bool HasNext() const { return m_Next; }
    private:
        T m_Value;
        SelfType* m_Prev = nullptr;
        SelfType* m_Next = nullptr;
    };

    template<typename T>
    class List; // Forward Declaration

    template<typename T>
    class LinkedList
    {
    public:
        typedef LinkedList<T> SelfType;
        typedef LinkedListNode<T> NodeType;

        LinkedList() = default;
        ~LinkedList() { PULSAR_DELETE(NodeType, m_Start); }

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

        LinkedList(const SelfType& other) { *this = other; }
        SelfType& operator=(const SelfType& other)
        {
            if (m_Start)
                PULSAR_DELETE(NodeType, m_Start);
            m_Start = nullptr;
            m_End = nullptr;
            NodeType* next = other.m_Start;
            while (next) {
                Append()->m_Value = next->m_Value;
                next = next->m_Next;
            }
            return *this;
        }

        LinkedList(SelfType&& other) { *this = std::move(other); }
        SelfType& operator=(SelfType&& other)
        {
            if (m_Start)
                PULSAR_DELETE(NodeType, m_Start);
            m_Start = other.m_Start;
            m_End = other.m_End;
            other.m_Start = nullptr;
            other.m_End = nullptr;
            return *this;
        }

        template<typename ...Args>
        NodeType* Prepend(Args&& ...init)
        {
            if (!m_Start) {
                PULSAR_ASSERT(!m_End, "LinkedList has an end but doesn't have a start.");
                m_Start = PULSAR_NEW(NodeType, std::forward<Args>(init)...);
                m_End = m_Start;
                return m_Start;
            }

            NodeType* newStart = PULSAR_NEW(NodeType, std::forward<Args>(init)...);
            newStart->m_Next = m_Start;
            m_Start->m_Prev = newStart;

            m_Start = newStart;
            return m_Start;
        }

        template<typename ...Args>
        NodeType* Append(Args&& ...init)
        {
            if (!m_Start) {
                PULSAR_ASSERT(!m_End, "LinkedList has an end but doesn't have a start.");
                m_Start = PULSAR_NEW(NodeType, std::forward<Args>(init)...);
                m_End = m_Start;
                return m_Start;
            }

            NodeType* newEnd = PULSAR_NEW(NodeType, std::forward<Args>(init)...);
            newEnd->m_Prev = m_End;
            m_End->m_Next = newEnd;

            m_End = newEnd;
            return m_End;
        }

        SelfType& Concat(SelfType&& other)
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

        SelfType& RemoveFront(size_t n)
        {
            if (n == 0)
                return *this;
            // This is the node before the new starting one
            NodeType* newStart = m_Start;
            for (size_t i = 0; i < n && newStart; i++)
                newStart = newStart->m_Next;

            if (!newStart) {
                PULSAR_DELETE(NodeType, m_Start);
                m_Start = nullptr;
                m_End = nullptr;
                return *this;
            }

            // Unlink
            PULSAR_ASSERT(newStart->m_Prev, "New LinkedList head has no previous element linked.");
            newStart->m_Prev->m_Next = nullptr;
            newStart->m_Prev = nullptr;

            // Delete previous nodes
            PULSAR_DELETE(NodeType, m_Start);
            m_Start = newStart;

            return *this;
        }

        NodeType* Front()             { return m_Start; }
        const NodeType* Front() const { return m_Start; }
        NodeType* Back()              { return m_End; }
        const NodeType* Back() const  { return m_End; }

        void Clear()          { PULSAR_DELETE(NodeType, m_Start); m_Start = nullptr; m_End = nullptr; }
        size_t Length() const { return m_Start ? m_Start->Length() : 0; }
    private:
        NodeType* m_Start = nullptr;
        NodeType* m_End = nullptr;
    };
}

#endif // _PULSAR_STRUCTURES_LINKEDLIST_H
