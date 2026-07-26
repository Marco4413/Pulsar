#ifndef _PULSARBINDINGS_EXTBINDING_H
#define _PULSARBINDINGS_EXTBINDING_H

#include <filesystem> // std::filesystem::path
#include <memory> // std::unique_ptr
#include <string> // std::string

#if !__has_include("cpulsar/core.h")
#  error "cpulsar/core.h" not found, it is required by ExtBinding(s)
#endif // "cpulsar/core.h"

#include "cpulsar/core.h" // CPULSAR_CALL, CPulsar_Module

#include "pulsar-bindings/ibinding.h"

namespace PulsarBindings
{
    bool InitCPulsar(const std::filesystem::path& fromDirectory);
    bool GetCPulsarLoadError(std::string& error);

    bool GetCPulsarVersionNumber(uint64_t& cpulsarVersionNumber);
    bool IsCPulsarVersionSupported(uint64_t libVersionNumber);

    typedef struct DynamicLibraryData DynamicLibraryData;

    struct DynamicLibraryFlags
    {
        bool LoadOnConstruction = false;
        bool GlobalSymbols = false;
    };

    class DynamicLibrary final
    {
    public:
        DynamicLibrary(const std::filesystem::path& path, DynamicLibraryFlags flags={});

        DynamicLibrary(DynamicLibrary&&);
        DynamicLibrary(const DynamicLibrary&) = delete;

        ~DynamicLibrary();

        DynamicLibrary& operator=(DynamicLibrary&&) = delete;
        DynamicLibrary& operator=(const DynamicLibrary&) = delete;

        void Load();
        void Unload();
        void* GetSymbol(const char* name);

        bool IsLoaded() const;
        const std::string& GetErrorMessage() const;

    private:
        std::filesystem::path m_LibraryPath;
        DynamicLibraryFlags m_Flags;

        std::unique_ptr<DynamicLibraryData> m_Data;
        std::string m_ErrorMessage;
    };

    class ExtBinding final : public IBinding
    {
    public:
        using GetCPulsarVersionFn = uint64_t(CPULSAR_CALL *)(void);
        using BindTypesFn         = void(CPULSAR_CALL *)(CPulsar_Module*);
        using BindFunctionsFn     = void(CPULSAR_CALL *)(CPulsar_Module*, bool);

    public:
        ExtBinding(const std::filesystem::path& path);

        ExtBinding(ExtBinding&&);
        ExtBinding(const ExtBinding&) = delete;

        ~ExtBinding();

        ExtBinding& operator=(ExtBinding&&) = delete;
        ExtBinding& operator=(const ExtBinding&) = delete;

        void BindTypes(Pulsar::Module& module) const override;
        void BindFunctions(Pulsar::Module& module, bool declareAndBind) const override;

        const std::string& GetErrorMessage() const { return m_ErrorMessage; }

        operator bool() const { return (bool)m_Lib.IsLoaded(); }

    private:
        DynamicLibrary m_Lib;
        std::string m_ErrorMessage;

        BindTypesFn m_BindTypes = nullptr;
        BindFunctionsFn m_BindFunctions = nullptr;
    };
}

#endif // _PULSARBINDINGS_EXTBINDING_H
