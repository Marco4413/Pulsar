#ifndef _PULSARTOOLS_EXTBINDING_H
#define _PULSARTOOLS_EXTBINDING_H

#include "pulsar-tools/binding.h"

namespace PulsarTools
{
    // Any instance of this class must be deleted
    //  AFTER any module it was bound to.
    class ExtBinding final
    {
    public:
        using ExtLib = void*;

        using BindTypesFn = void(*)(Pulsar::Module*);
        using BindFunctionsFn = void(*)(Pulsar::Module*, int);

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

        operator bool() const { return (bool)m_Lib; }

    private:
        ExtLib m_Lib = nullptr;
        Pulsar::String m_ErrorMessage;

        BindTypesFn m_BindTypes = nullptr;
        BindFunctionsFn m_BindFunctions = nullptr;
    };
}

#endif // _PULSARTOOLS_EXTBINDING_H
