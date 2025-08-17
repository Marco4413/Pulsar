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

    private:
        std::unique_lock<Mutex> m_Lock;
    };
}

#endif // _PULSARDEBUGGER_LOCK_H
