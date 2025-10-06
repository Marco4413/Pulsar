#ifndef _PULSARTOOLS_EXTBINDING_H
#define _PULSARTOOLS_EXTBINDING_H

#include <memory> // std::unique_ptr

// We need to specifically check for CPulsar's platform
#include <cpulsar/platform.h>

#include "pulsar-tools/binding.h"

#if defined(CPULSAR_PLATFORM_WINDOWS)
#  define CPULSAR_CALL __cdecl
#else // CPULSAR_PLATFORM_*
#  define CPULSAR_CALL
#endif

namespace PulsarTools
{
    typedef struct DynamicLibraryData DynamicLibraryData;

    struct DynamicLibraryFlags
    {
        bool LoadOnConstruction = false;
        bool GlobalSymbols = false;
    };

    class DynamicLibrary final
    {
    public:
        DynamicLibrary(const char* path, DynamicLibraryFlags flags={});

        DynamicLibrary(DynamicLibrary&&);
        DynamicLibrary(const DynamicLibrary&) = delete;

        ~DynamicLibrary();

        DynamicLibrary& operator=(DynamicLibrary&&) = delete;
        DynamicLibrary& operator=(const DynamicLibrary&) = delete;

        void Load();
        void Unload();
        void* GetSymbol(const char* name);

        bool IsLoaded() const;

        bool HasError() const;
        const Pulsar::String& GetErrorMessage() const;

    private:
        Pulsar::String m_LibraryPath;
        DynamicLibraryFlags m_Flags;

        std::unique_ptr<DynamicLibraryData> m_Data;
        Pulsar::String m_ErrorMessage;
    };

    // Any instance of this class must be deleted
    //  AFTER any module it was bound to.
    class ExtBinding final
    {
    public:
        using GetPulsarVersionFn = uint64_t(CPULSAR_CALL *)(void);
        using BindTypesFn        = void(CPULSAR_CALL *)(Pulsar::Module*);
        using BindFunctionsFn    = void(CPULSAR_CALL *)(Pulsar::Module*, int);

    public:
        ExtBinding(const char* path);

        ExtBinding(ExtBinding&&);
        ExtBinding(const ExtBinding&) = delete;

        ~ExtBinding();

        ExtBinding& operator=(ExtBinding&&) = delete;
        ExtBinding& operator=(const ExtBinding&) = delete;

        void BindAll(Pulsar::Module& module, bool declareAndBind=false) const
        {
            BindTypes(module);
            BindFunctions(module, declareAndBind);
        }

        void BindTypes(Pulsar::Module& module) const;
        void BindFunctions(Pulsar::Module& module, bool declareAndBind) const;

        const Pulsar::String& GetErrorMessage() const { return m_ErrorMessage; }

        operator bool() const { return (bool)m_Lib.IsLoaded(); }

    private:
        static bool IsPulsarVersionSupported(uint64_t versionNumber);

    private:
        DynamicLibrary m_Lib;
        Pulsar::String m_ErrorMessage;

        BindTypesFn m_BindTypes = nullptr;
        BindFunctionsFn m_BindFunctions = nullptr;
    };
}

#endif // _PULSARTOOLS_EXTBINDING_H
