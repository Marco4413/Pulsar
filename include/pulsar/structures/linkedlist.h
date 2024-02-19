#ifndef _PULSAR_STRUCTURES_LINKEDLIST_H
#define _PULSAR_STRUCTURES_LINKEDLIST_H

#include "pulsar/core.h"

namespace Pulsar
{
    template<typename T, auto ...DefaultInit>
    class LinkedList; // Forward Declaration

    template<typename T, auto ...DefaultInit>
    class LinkedListNode
    {
    public:
        friend LinkedList<T, DefaultInit...>;
        typedef LinkedListNode<T, DefaultInit...> SelfType;
        
        LinkedListNode()
            : m_Value(DefaultInit...) { }
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

        SelfType* Next()             { return m_Next; }
        const SelfType* Next() const { return m_Next; }

        bool HasNext() const { return m_Next; }
    private:
        T m_Value;
        SelfType* m_Next = nullptr;
    };

    template<typename T, auto ...DefaultInit>
    class LinkedList
    {
    public:
        typedef LinkedList<T, DefaultInit...> SelfType;
        typedef LinkedListNode<T, DefaultInit...> NodeType;

        LinkedList() = default;
        ~LinkedList() { PULSAR_DELETE(NodeType, m_Start); }

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

        LinkedList(SelfType&& other) { *this = other; }
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

        NodeType* Prepend()
        {
            if (!m_Start) {
                m_Start = PULSAR_NEW(NodeType);
                m_End = m_Start;
                return m_Start;
            }
            NodeType* newStart = PULSAR_NEW(NodeType);
            newStart->m_Next = m_Start;
            m_Start = newStart;
            return m_Start;
        }

        NodeType* Append()
        {
            if (!m_Start)
                return Prepend();
            m_End->m_Next = PULSAR_NEW(NodeType);
            m_End = m_End->m_Next;
            return m_End;
        }

        SelfType& Concat(SelfType& other)
        {
            m_End->m_Next = other.m_Start;
            other.m_Start = nullptr;
            other.m_End = nullptr;
            return *this;
        }

        SelfType& RemoveFront(size_t n)
        {
            if (n == 0)
                return *this;
            // This is the node before the new starting one
            NodeType* lastNode = m_Start;
            for (size_t i = 0; i < n-1 && lastNode; i++)
                lastNode = lastNode->m_Next;
            if (!lastNode->m_Next) {
                PULSAR_DELETE(NodeType, m_Start);
                m_Start = nullptr;
                m_End = nullptr;
                return *this;
            }
            NodeType* newStart = lastNode->m_Next;
            lastNode->m_Next = nullptr;
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
