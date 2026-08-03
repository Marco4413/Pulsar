#ifndef _PULSARDEBUGGER_LOCK_H
#define _PULSARDEBUGGER_LOCK_H

#include <mutex>

namespace PulsarDebugger
{
    template<typename Mutex>
    class ScopeLock; // Forward declaration

    template<typename Mutex>
    class ILockable
    {
    public:
        friend ScopeLock<Mutex>;

        ILockable() = default;
        virtual ~ILockable() = default;

    private:
        Mutex m_Mutex;
    };

    template<typename Mutex>
    class ScopeLock final
    {
    public:
        ScopeLock(ILockable<Mutex>& lockable)
            : m_Lock(lockable.m_Mutex) { }
        ~ScopeLock() = default;

        void Lock()   { m_Lock.lock();   }
        void Unlock() { m_Lock.unlock(); }

    private:
        std::unique_lock<Mutex> m_Lock;
    };

    template<typename T>
    class LockedValue : public ILockable<std::recursive_mutex>
    {
    public:
        LockedValue() = default;
        LockedValue(T initValue)
            : m_Value(std::move(initValue)) {}
        ~LockedValue() = default;

        T Load()
        {
            ScopeLock _lock(*this);
            T value = m_Value;
            return value;
        }

        void Store(T newValue)
        {
            ScopeLock _lock(*this);
            m_Value = std::move(newValue);
        }

    private:
        T m_Value;
    };
}

#endif // _PULSARDEBUGGER_LOCK_H
