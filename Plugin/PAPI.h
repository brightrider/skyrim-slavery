#pragma once

namespace Papyrus
{
    template <class F>
    class RawCallback : public RE::BSScript::IStackCallbackFunctor
    {
    public:
        explicit RawCallback(F a_callback) :
            callback(std::move(a_callback))
        {}

        void operator()(RE::BSScript::Variable a_result) override
        {
            callback(a_result);
        }

        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override
        {}

    private:
        F callback;
    };

    template <class F, class... Args>
    bool Call(RE::TESForm* form, const char* scriptName, const char* functionName, F&& resultCallback, Args&&... args)
    {
        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm || !form) {
            return false;
        }

        auto policy = vm->GetObjectHandlePolicy();
        if (!policy) {
            return false;
        }

        auto handle = policy->GetHandleForObject(form->GetFormType(), form);
        if (handle == policy->EmptyHandle()) {
            return false;
        }

        auto papyrusArgs = RE::MakeFunctionArguments(std::forward<Args>(args)...);

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(
            new RawCallback<std::remove_cvref_t<F>>(std::forward<F>(resultCallback))
        );

        return vm->DispatchMethodCall(
            handle,
            RE::BSFixedString(scriptName),
            RE::BSFixedString(functionName),
            papyrusArgs,
            callback
        );
    }
}
