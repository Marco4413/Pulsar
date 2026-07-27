#include "pulsar-bindings/extbinding.h"

#include "pulsar/version.h"

static struct {
    std::unique_ptr<PulsarBindings::DynamicLibrary> DL = nullptr;
    PulsarBindings::ExtBinding::GetCPulsarVersionFn GetVersionNumber = nullptr;
    uint64_t VersionNumber = 0;
} s_CPulsar;

bool PulsarBindings::InitCPulsar(const std::filesystem::path& fromDirectory)
{
    std::filesystem::path cpulsarPath(fromDirectory);
#if defined(CPULSAR_PLATFORM_MACOSX)
    cpulsarPath /= "libcpulsar.dylib";
#elif defined(CPULSAR_PLATFORM_UNIX)
    cpulsarPath /= "libcpulsar.so";
#elif defined(CPULSAR_PLATFORM_WINDOWS)
    cpulsarPath /= "cpulsar.dll";
#else // CPULSAR_PLATFORM_*
    cpulsarPath /= "<cpulsar-unsupported-platform>";
#endif // CPULSAR_PLATFORM_*

    s_CPulsar = {};
    s_CPulsar.DL = std::make_unique<DynamicLibrary>(
            cpulsarPath,
            DynamicLibraryFlags{
                .LoadOnConstruction = true,
                .GlobalSymbols = true,
            });

    return s_CPulsar.DL->IsLoaded();
}

bool PulsarBindings::GetCPulsarLoadError(std::string& errorMessage)
{
    if (!s_CPulsar.DL) {
        errorMessage = "CPulsar not initialized.";
        return true;
    }

    if (!s_CPulsar.DL->IsLoaded()) {
        errorMessage  = "Could not load CPulsar: ";
        errorMessage += s_CPulsar.DL->GetErrorMessage();
        return true;
    }

    return false;
}

bool PulsarBindings::GetCPulsarVersionNumber(uint64_t& cpulsarVersionNumber)
{
    if (!s_CPulsar.GetVersionNumber) {
        if (!s_CPulsar.DL || !s_CPulsar.DL->IsLoaded()) return false;
        s_CPulsar.GetVersionNumber = (ExtBinding::GetCPulsarVersionFn)s_CPulsar.DL->GetSymbol("CPulsar_GetVersionNumber");
        if (!s_CPulsar.GetVersionNumber) return false;
        s_CPulsar.VersionNumber = s_CPulsar.GetVersionNumber();
    }

    cpulsarVersionNumber = s_CPulsar.VersionNumber;
    return true;
}

bool PulsarBindings::IsCPulsarVersionSupported(uint64_t libVersionNumber)
{
    uint64_t cpulsarVersionNumber;
    if (!GetCPulsarVersionNumber(cpulsarVersionNumber)) return false;

    auto cpulsarVersion = Pulsar::SemVer::FromNumber(cpulsarVersionNumber);
    // Require perfect match for in-dev builds
    if (cpulsarVersion.Major == 0 || cpulsarVersion.Pre.Kind != Pulsar::PreReleaseKind::None)
        return cpulsarVersionNumber == libVersionNumber;

    auto libVersion = Pulsar::SemVer::FromNumber(libVersionNumber);
    return libVersion.Major == cpulsarVersion.Major;
}

PulsarBindings::ExtBinding::ExtBinding(const std::filesystem::path& path)
    : m_Lib(path)
{
    if (GetCPulsarLoadError(m_ErrorMessage)) return;

    m_Lib.Load();
    if (!m_Lib.IsLoaded()) {
        m_ErrorMessage = m_Lib.GetErrorMessage();
        return;
    }

    GetCPulsarVersionFn getCPulsarVersion = (GetCPulsarVersionFn)m_Lib.GetSymbol("PulsarExt_GetCPulsarVersion");
    if (!getCPulsarVersion) {
        m_ErrorMessage = "Function PulsarExt_GetCPulsarVersion not found.";
        m_Lib.Unload();
        return;
    }

    uint64_t pulsarVersion = getCPulsarVersion();
    if (!IsCPulsarVersionSupported(pulsarVersion)) {
        m_ErrorMessage  = "Binding was made for an unsupported CPulsar version (";
        m_ErrorMessage += Pulsar::SemVer::FromNumber(pulsarVersion).ToString().CString();
        m_ErrorMessage += ").";
        m_Lib.Unload();
        return;
    }

    m_BindTypes = (BindTypesFn)m_Lib.GetSymbol("PulsarExt_BindTypes");
    m_BindFunctions = (BindFunctionsFn)m_Lib.GetSymbol("PulsarExt_BindFunctions");
}

PulsarBindings::ExtBinding::ExtBinding(ExtBinding&& other)
    : m_Lib(std::move(other.m_Lib))
{
    m_ErrorMessage = std::move(other.m_ErrorMessage);
    m_BindTypes = other.m_BindTypes;
    m_BindFunctions = other.m_BindFunctions;

    other.m_BindTypes = nullptr;
    other.m_BindFunctions = nullptr;
}

PulsarBindings::ExtBinding::~ExtBinding()
{
    if (m_Lib.IsLoaded())
        m_Lib.Unload();
    m_BindTypes = nullptr;
    m_BindFunctions = nullptr;
}

void PulsarBindings::ExtBinding::BindTypes(Pulsar::Module& module) const
{
    if (m_Lib.IsLoaded() && m_BindTypes) {
        m_BindTypes(reinterpret_cast<CPulsar_Module*>(&module));
    }
}

void PulsarBindings::ExtBinding::BindFunctions(Pulsar::Module& module, bool declareAndBind) const
{
    if (m_Lib.IsLoaded() && m_BindFunctions) {
        m_BindFunctions(reinterpret_cast<CPulsar_Module*>(&module), declareAndBind);
    }
}

#if defined(CPULSAR_PLATFORM_UNIX)

#include <dlfcn.h>

namespace PulsarBindings
{
    struct DynamicLibraryData
    {
        void* Handle = NULL;
    };
}

#elif defined(CPULSAR_PLATFORM_WINDOWS)

#define WIN32_LEAN_AND_MEAN
#include "windows.h"

namespace PulsarBindings
{
    struct DynamicLibraryData
    {
        HMODULE Handle = NULL;
    };
}

#else // Unsupported Platform

namespace PulsarBindings
{
    struct DynamicLibraryData
    {
        void* Handle = NULL;
    };
}

#endif

PulsarBindings::DynamicLibrary::DynamicLibrary(const std::filesystem::path& path, DynamicLibraryFlags flags)
    : m_LibraryPath(path)
    , m_Flags(flags)
    , m_Data(std::make_unique<DynamicLibraryData>())
{
    if (m_Flags.LoadOnConstruction)
        Load();
}

PulsarBindings::DynamicLibrary::DynamicLibrary(DynamicLibrary&& other)
{
    m_LibraryPath = std::move(other.m_LibraryPath);
    m_Flags       = other.m_Flags;

    m_Data = std::move(other.m_Data);
    other.m_Data = std::make_unique<DynamicLibraryData>();

    m_ErrorMessage = std::move(other.m_ErrorMessage);
}

PulsarBindings::DynamicLibrary::~DynamicLibrary()
{
    Unload();
}

bool PulsarBindings::DynamicLibrary::IsLoaded() const
{
    return m_Data->Handle != NULL;
}

const std::string& PulsarBindings::DynamicLibrary::GetErrorMessage() const
{
    return m_ErrorMessage;
}

#if defined(CPULSAR_PLATFORM_UNIX)

void PulsarBindings::DynamicLibrary::Load()
{
    m_ErrorMessage.clear();
    if (IsLoaded()) return;

    int dlFlags = RTLD_NOW | (m_Flags.GlobalSymbols ? RTLD_GLOBAL : RTLD_LOCAL);
    m_Data->Handle = dlopen(m_LibraryPath.c_str(), dlFlags);
    if (m_Data->Handle == NULL) {
        m_ErrorMessage = dlerror();
    }
}

void PulsarBindings::DynamicLibrary::Unload()
{
    m_ErrorMessage.clear();
    if (!IsLoaded()) return;

    dlclose(m_Data->Handle);
    m_Data->Handle = NULL;
}

void* PulsarBindings::DynamicLibrary::GetSymbol(const char* symbolName)
{
    if (!IsLoaded()) {
        m_ErrorMessage = "Trying to get symbol of not loaded Dynamic Library.";
        return nullptr;
    }

    m_ErrorMessage.clear();
    void* symbol = dlsym(m_Data->Handle, symbolName);
    if (symbol == NULL) return nullptr;
    return symbol;
}

#elif defined(CPULSAR_PLATFORM_WINDOWS)

void PulsarBindings::DynamicLibrary::Load()
{
    m_ErrorMessage.clear();
    if (IsLoaded()) return;

    m_Data->Handle = LoadLibraryW(m_LibraryPath.c_str());
    if (m_Data->Handle == NULL) {
        DWORD errorCode = GetLastError();
        LPSTR msgBuffer = NULL;
        DWORD fmAlloc = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            errorCode, 0,
            (LPSTR)&msgBuffer, 0,
            NULL
        );

        if (fmAlloc) {
            m_ErrorMessage = (const char*)msgBuffer;
            LocalFree(msgBuffer);
        } else {
            m_ErrorMessage = "Could not retrieve error message.";
        }
    }
}

void PulsarBindings::DynamicLibrary::Unload()
{
    m_ErrorMessage.clear();
    if (!IsLoaded()) return;

    FreeLibrary(m_Data->Handle);
    m_Data->Handle = NULL;
}

void* PulsarBindings::DynamicLibrary::GetSymbol(const char* symbolName)
{
    if (!IsLoaded()) {
        m_ErrorMessage = "Trying to get symbol of not loaded Dynamic Library.";
        return nullptr;
    }

    m_ErrorMessage.clear();
    FARPROC proc = GetProcAddress(m_Data->Handle, symbolName);
    if (proc == NULL) return nullptr;
    return (void*)proc;
}

#else // Unsupported Platform

void PulsarBindings::DynamicLibrary::Load()
{
    m_ErrorMessage = "Loading of shared native libraries is not supported on your system.";
}

void PulsarBindings::DynamicLibrary::Unload()
{
    m_ErrorMessage.clear();
}

void* PulsarBindings::DynamicLibrary::GetSymbol(const char* symbolName)
{
    m_ErrorMessage = "Trying to get symbol of Dynamic Library on unsupported OS.";
    return nullptr;
}

#endif
