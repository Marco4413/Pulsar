#include "pulsar-tools/extbinding.h"

// TODO: PulsarTools::CLI::GetThisProcessExecutable() should probably be moved elsewhere
#include "pulsar-tools/cli.h" // PulsarTools::CLI::GetThisProcessExecutable()
#include "pulsar-tools/version.h"

// Load cpulsar relative to this version of pulsar-tools
//  so that another version won't be loaded by external bindings.
// Moreover, while it seems that Windows loads DLL dependencies
//  of loaded DLLs relative to the executable, Linux does not,
//  so we're required to make the symbols available Globally.
// Even though this is kind of useless on Windows, it's still
//  good checking if the DLL exists before loading External
//  Bindings so that we get a meaningful error message.
static PulsarTools::DynamicLibrary s_CPulsarDL(
        PulsarTools::CLI::GetThisProcessExecutable()
            .parent_path()
#if defined(CPULSAR_PLATFORM_UNIX)
            .append("libcpulsar.so")
#elif defined(CPULSAR_PLATFORM_WINDOWS)
            .append("cpulsar.dll")
#else // CPULSAR_PLATFORM_*
            .append("dummy-name-for-cpulsar-dynamic-library-on-unsupported-platform")
#endif // CPULSAR_PLATFORM_*
            .string()
            .c_str(),
        {
            .LoadOnConstruction = true,
            .GlobalSymbols = true,
        });

PulsarTools::ExtBinding::ExtBinding(const char* path)
    : m_Lib(path)
{
    if (!s_CPulsarDL.IsLoaded()) {
        m_ErrorMessage  = "Could not load CPulsar: ";
        m_ErrorMessage += s_CPulsarDL.HasError()
            ? s_CPulsarDL.GetErrorMessage()
            : "unknown reason.";
        return;
    }

    m_Lib.Load();
    if (!m_Lib.IsLoaded()) {
        m_ErrorMessage = m_Lib.HasError()
            ? m_Lib.GetErrorMessage()
            : "Could not load dynamic library.";
        return;
    }

    GetPulsarVersionFn getPulsarVersion = (GetPulsarVersionFn)m_Lib.GetSymbol("GetPulsarVersion");
    if (!getPulsarVersion) {
        m_ErrorMessage = "Function GetPulsarVersion not found.";
        m_Lib.Unload();
        return;
    }

    uint64_t pulsarVersion = getPulsarVersion();
    if (!IsPulsarVersionSupported(pulsarVersion)) {
        m_ErrorMessage  = "Binding was made for an unsupported Pulsar version (";
        m_ErrorMessage += Version::ToString(Version::FromNumber(pulsarVersion)).c_str();
        m_ErrorMessage += ").";
        m_Lib.Unload();
        return;
    }

    m_BindTypes = (BindTypesFn)m_Lib.GetSymbol("BindTypes");
    m_BindFunctions = (BindFunctionsFn)m_Lib.GetSymbol("BindFunctions");
}

PulsarTools::ExtBinding::ExtBinding(ExtBinding&& other)
    : m_Lib(std::move(other.m_Lib))
{
    m_ErrorMessage = std::move(other.m_ErrorMessage);
    m_BindTypes = other.m_BindTypes;
    m_BindFunctions = other.m_BindFunctions;

    other.m_BindTypes = nullptr;
    other.m_BindFunctions = nullptr;
}

PulsarTools::ExtBinding::~ExtBinding()
{
    if (m_Lib.IsLoaded())
        m_Lib.Unload();
    m_BindTypes = nullptr;
    m_BindFunctions = nullptr;
}

void PulsarTools::ExtBinding::BindTypes(Pulsar::Module& module) const
{
    if (m_Lib.IsLoaded() && m_BindTypes) {
        m_BindTypes(&module);
    }
}

void PulsarTools::ExtBinding::BindFunctions(Pulsar::Module& module, bool declareAndBind) const
{
    if (m_Lib.IsLoaded() && m_BindFunctions) {
        m_BindFunctions(&module, declareAndBind);
    }
}

bool PulsarTools::ExtBinding::IsPulsarVersionSupported(uint64_t versionNumber)
{
    auto vPls = GetPulsarVersion();
    // Require perfect match for in-dev builds
    if (vPls.Major == 0 || vPls.Pre.Kind != Version::PreReleaseKind::None)
        return vPls.ToNumber() == versionNumber;

    auto vLib = Version::FromNumber(versionNumber);
    return vLib.Major == vPls.Major;
}

#if defined(CPULSAR_PLATFORM_UNIX)

#include <dlfcn.h>

namespace PulsarTools
{
    struct DynamicLibraryData
    {
        void* Handle = nullptr;
    };
}

#elif defined(CPULSAR_PLATFORM_WINDOWS)

#define WIN32_LEAN_AND_MEAN
#include "windows.h"

namespace PulsarTools
{
    struct DynamicLibraryData
    {
        HMODULE Handle = NULL;
    };
}

#else // Unsupported Platform

namespace PulsarTools
{
    struct DynamicLibraryData
    {
        void* Handle = nullptr;
    };
}

#endif

PulsarTools::DynamicLibrary::DynamicLibrary(const char* path, DynamicLibraryFlags flags)
    : m_LibraryPath(path)
    , m_Flags(flags)
    , m_Data(std::make_unique<DynamicLibraryData>())
    , m_ErrorMessage("None")
{
    if (m_Flags.LoadOnConstruction)
        Load();
}

PulsarTools::DynamicLibrary::DynamicLibrary(DynamicLibrary&& other)
{
    m_LibraryPath = std::move(other.m_LibraryPath);
    m_Flags       = other.m_Flags;

    m_Data = std::move(other.m_Data);
    other.m_Data = std::make_unique<DynamicLibraryData>();

    m_ErrorMessage = std::move(other.m_ErrorMessage);
}

PulsarTools::DynamicLibrary::~DynamicLibrary()
{
    Unload();
}

bool PulsarTools::DynamicLibrary::IsLoaded() const
{
    return (bool)m_Data->Handle;
}

bool PulsarTools::DynamicLibrary::HasError() const
{
    return m_ErrorMessage.Length() > 0;
}

const Pulsar::String& PulsarTools::DynamicLibrary::GetErrorMessage() const
{
    return m_ErrorMessage;
}

#if defined(CPULSAR_PLATFORM_UNIX)

void PulsarTools::DynamicLibrary::Load()
{
    m_ErrorMessage.Resize(0);
    if (m_Data->Handle) return;
    int dlFlags = RTLD_NOW | (m_Flags.GlobalSymbols ? RTLD_GLOBAL : RTLD_LOCAL);
    m_Data->Handle = dlopen(m_LibraryPath.CString(), dlFlags);
    if (!m_Data->Handle)
        m_ErrorMessage = dlerror();
}

void PulsarTools::DynamicLibrary::Unload()
{
    m_ErrorMessage.Resize(0);
    if (!m_Data->Handle) return;
    dlclose(m_Data->Handle);
}

void* PulsarTools::DynamicLibrary::GetSymbol(const char* symbolName)
{
    if (!m_Data->Handle) {
        m_ErrorMessage = "Trying to get symbol of not loaded Dynamic Library.";
        return nullptr;
    }

    m_ErrorMessage.Resize(0);
    dlerror();
    void* symbol = dlsym(m_Data->Handle, symbolName);
    const char* error = dlerror();
    if (error) m_ErrorMessage = error;
    return symbol;
}

#elif defined(CPULSAR_PLATFORM_WINDOWS)

void PulsarTools::DynamicLibrary::Load()
{
    m_ErrorMessage.Resize(0);
    if (m_Data->Handle) return;
    m_Data->Handle = LoadLibraryA(m_LibraryPath.CString());
    if (!m_Data->Handle) {
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

void PulsarTools::DynamicLibrary::Unload()
{
    m_ErrorMessage.Resize(0);
    if (!m_Data->Handle) return;
    FreeLibrary(m_Data->Handle);
}

void* PulsarTools::DynamicLibrary::GetSymbol(const char* symbolName)
{
    if (!m_Data->Handle) {
        m_ErrorMessage = "Trying to get symbol of not loaded Dynamic Library.";
        return nullptr;
    }

    m_ErrorMessage.Resize(0);
    return (void*)GetProcAddress(m_Data->Handle, symbolName);
}

#else // Unsupported Platform

void PulsarTools::DynamicLibrary::Load()
{
    m_ErrorMessage = "Loading of shared native libraries is not supported on your system.";
}

void PulsarTools::DynamicLibrary::Unload()
{
}

void* PulsarTools::DynamicLibrary::GetSymbol(const char* symbolName)
{
    return nullptr;
}

#endif
