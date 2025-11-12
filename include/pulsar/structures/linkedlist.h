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
        // Forward Declarations
        class ConstIterator;
        class MutableIterator;

        LinkedList() = default;
        ~LinkedList() { PULSAR_DELETE(Node, m_Start); }

        explicit LinkedList(const List<T>& list)
            : LinkedList()
        {
            for (const T& v : list) Append(v);
        }

        explicit LinkedList(List<T>&& list)
            : LinkedList()
        {
            for (T& v : list) Append(std::move(v));
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

        Self& RemoveBack(size_t n)
        {
            if (n == 0)
                return *this;
            // This is the node before the new starting one
            Node* newEnd = m_End;
            for (size_t i = 0; i < n && newEnd; i++)
                newEnd = newEnd->m_Prev;

            if (!newEnd) {
                PULSAR_DELETE(Node, m_Start);
                m_Start = nullptr;
                m_End = nullptr;
                return *this;
            }

            // Unlink
            PULSAR_ASSERT(newEnd->m_Next, "New LinkedList back has no next element linked.");
            Node* chainToDelete = newEnd->m_Next;
            newEnd->m_Next->m_Prev = nullptr;
            newEnd->m_Next = nullptr;

            // Delete previous nodes
            PULSAR_DELETE(Node, chainToDelete);
            m_End = newEnd;

            return *this;
        }

        Node* Front()             { return m_Start; }
        const Node* Front() const { return m_Start; }
        Node* Back()              { return m_End; }
        const Node* Back() const  { return m_End; }

        void Clear()          { PULSAR_DELETE(Node, m_Start); m_Start = nullptr; m_End = nullptr; }
        size_t Length() const { return m_Start ? m_Start->Length() : 0; }

        ConstIterator Begin() const { return ConstIterator(m_Start); }
        ConstIterator End()   const { return ConstIterator(nullptr); }
        MutableIterator Begin() { return MutableIterator(m_Start); }
        MutableIterator End()   { return MutableIterator(nullptr); }

        PULSAR_ITERABLE_IMPL(Self, ConstIterator, MutableIterator)

    public:
        class ConstIterator
        {
        public:
            explicit ConstIterator(const Node* node)
                : m_Node(node) {}

            bool operator==(const ConstIterator& other) const = default;
            bool operator!=(const ConstIterator& other) const = default;
            const T& operator*() const { return m_Node->Value(); }

            ConstIterator& operator++()
            {
                PULSAR_ASSERT(m_Node, "Called ++LinkedList<T>::ConstIterator on complete iterator.");
                m_Node = m_Node->Next();
                return *this;
            }

            ConstIterator operator++(int)
            {
                ConstIterator ret = *this;
                ++(*this);
                return ret;
            }
        private:
            const Node* m_Node;
        };

        class MutableIterator
        {
        public:
            MutableIterator(Node* node)
                : m_Node(node) {}

            bool operator==(const MutableIterator& other) const = default;
            bool operator!=(const MutableIterator& other) const = default;
            T& operator*()             { return m_Node->Value(); }
            const T& operator*() const { return m_Node->Value(); }

            MutableIterator& operator++()
            {
                PULSAR_ASSERT(m_Node, "Called ++LinkedList<T>::MutableIterator on complete iterator.");
                m_Node = m_Node->Next();
                return *this;
            }

            MutableIterator operator++(int)
            {
                MutableIterator ret = *this;
                ++(*this);
                return ret;
            }
        private:
            Node* m_Node;
        };

    private:
        Node* m_Start = nullptr;
        Node* m_End = nullptr;
    };
}

#endif // _PULSAR_STRUCTURES_LINKEDLIST_H
