#include "pulsar-tools/extbinding.h"

#include "pulsar-tools/version.h"

PulsarTools::ExtBinding::ExtBinding(ExtBinding&& other)
{
    m_Lib = other.m_Lib;
    m_ErrorMessage = std::move(other.m_ErrorMessage);
    m_BindTypes = other.m_BindTypes;
    m_BindFunctions = other.m_BindFunctions;

    other.m_Lib = nullptr;
    other.m_BindTypes = nullptr;
    other.m_BindFunctions = nullptr;
}

void PulsarTools::ExtBinding::BindTypes(Pulsar::Module& module) const
{
    if (m_Lib && m_BindTypes) {
        m_BindTypes(&module);
    }
}

void PulsarTools::ExtBinding::BindFunctions(Pulsar::Module& module, bool declareAndBind) const
{
    if (m_Lib && m_BindFunctions) {
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

struct ExtLibImpl
{
    void* Handle;
};

PulsarTools::ExtBinding::ExtBinding(const char* path)
{
    void* handle = dlopen(path, RTLD_NOW);
    if (handle) {
        GetPulsarVersionFn getPulsarVersion = (GetPulsarVersionFn)dlsym(handle, "GetPulsarVersion");
        if (!getPulsarVersion) {
            m_ErrorMessage = "Function GetPulsarVersion not found.";
            dlclose(handle);
            return;
        }

        uint64_t pulsarVersion = getPulsarVersion();
        if (!IsPulsarVersionSupported(pulsarVersion)) {
            m_ErrorMessage  = "Binding was made for an unsupported Pulsar version (";
            m_ErrorMessage += Version::ToString(Version::FromNumber(pulsarVersion)).c_str();
            m_ErrorMessage += ").";
            dlclose(handle);
            return;
        }

        m_BindTypes = (BindTypesFn)dlsym(handle, "BindTypes");
        m_BindFunctions = (BindFunctionsFn)dlsym(handle, "BindFunctions");

        ExtLibImpl* lib = (ExtLibImpl*)(m_Lib = PULSAR_MALLOC(sizeof(ExtLibImpl)));
        lib->Handle = handle;
    } else {
        m_ErrorMessage = dlerror();
    }
}

PulsarTools::ExtBinding::~ExtBinding()
{
    if (m_Lib) {
        ExtLibImpl* lib = (ExtLibImpl*)m_Lib;
        dlclose(lib->Handle);
        PULSAR_FREE(lib);

        m_Lib = nullptr;
        m_BindTypes = nullptr;
        m_BindFunctions = nullptr;
    }
}

#elif defined(CPULSAR_PLATFORM_WINDOWS)

#define WIN32_LEAN_AND_MEAN
#include "windows.h"

struct ExtLibImpl
{
    HMODULE Handle;
};

PulsarTools::ExtBinding::ExtBinding(const char* path)
{
    HMODULE handle = LoadLibraryA(path);
    if (handle) {
        GetPulsarVersionFn getPulsarVersion = (GetPulsarVersionFn)(void*)GetProcAddress(handle, "GetPulsarVersion");
        if (!getPulsarVersion) {
            m_ErrorMessage = "Function GetPulsarVersion not found.";
            FreeLibrary(handle);
            return;
        }

        uint64_t pulsarVersion = getPulsarVersion();
        if (!IsPulsarVersionSupported(pulsarVersion)) {
            m_ErrorMessage  = "Binding was made for an unsupported Pulsar version (";
            m_ErrorMessage += Version::ToString(Version::FromNumber(pulsarVersion)).c_str();
            m_ErrorMessage += ").";
            FreeLibrary(handle);
            return;
        }

        m_BindTypes = (BindTypesFn)(void*)GetProcAddress(handle, "BindTypes");
        m_BindFunctions = (BindFunctionsFn)(void*)GetProcAddress(handle, "BindFunctions");

        ExtLibImpl* lib = (ExtLibImpl*)(m_Lib = PULSAR_MALLOC(sizeof(ExtLibImpl)));
        lib->Handle = handle;
    } else {
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
            m_ErrorMessage = "Could not retrieve error message";
        }
    }

}

PulsarTools::ExtBinding::~ExtBinding()
{
    if (m_Lib) {
        ExtLibImpl* lib = (ExtLibImpl*)m_Lib;
        FreeLibrary(lib->Handle);
        PULSAR_FREE(lib);

        m_Lib = nullptr;
        m_BindTypes = nullptr;
        m_BindFunctions = nullptr;
    }
}

#else // Unsupported Platform

PulsarTools::ExtBinding::ExtBinding(const char* path)
{
    (void)path;
    m_ErrorMessage = "Loading of shared native libraries is not supported on your system.";
}

PulsarTools::ExtBinding::~ExtBinding() = default;

#endif
