#include "JCAPI.h"

#include "PAPI.h"

namespace JC {
    static std::atomic<ObjectId> ActorDb = 0;
    static std::atomic_bool ActorDbRequested = false;

    static std::atomic<ObjectId> MarkerDb = 0;
    static std::atomic_bool MarkerDbRequested = false;

    void* Domain = nullptr;

    JFormMapCount jFormMapCount = nullptr;
    JFormMapNextKey jFormMapNextKey = nullptr;
    JFormMapGetInt jFormMapGetInt = nullptr;
    JFormMapSetInt jFormMapSetInt = nullptr;
    JFormMapGetStr jFormMapGetStr = nullptr;
    JFormMapGetObj jFormMapGetObj = nullptr;
    JFormMapSetObj jFormMapSetObj = nullptr;
    JFormMapGetForm jFormMapGetForm = nullptr;
    JFormMapSetForm jFormMapSetForm = nullptr;
    JFormMapHasKey jFormMapHasKey = nullptr;
    JFormMapRemoveKey jFormMapRemoveKey = nullptr;

    template <class T>
    static void GetFunc(const jc::reflection_interface* refl, const char* name, const char* cls, T& out) {
        out = reinterpret_cast<T>(refl->tes_function_of_class(name, cls));
    }

    bool Load(const jc::root_interface* root) {
        if (!root) {
            return false;
        }

        auto refl = root->query_interface<jc::reflection_interface>();
        auto domain = root->query_interface<jc::domain_interface>();

        if (!refl || !domain) {
            return false;
        }

        Domain = domain->get_default_domain();

        GetFunc(refl, "count", "JFormMap", jFormMapCount);
        GetFunc(refl, "nextKey", "JFormMap", jFormMapNextKey);
        GetFunc(refl, "getInt", "JFormMap", jFormMapGetInt);
        GetFunc(refl, "setInt", "JFormMap", jFormMapSetInt);
        GetFunc(refl, "getStr", "JFormMap", jFormMapGetStr);
        GetFunc(refl, "getObj", "JFormMap", jFormMapGetObj);
        GetFunc(refl, "setObj", "JFormMap", jFormMapSetObj);
        GetFunc(refl, "getForm", "JFormMap", jFormMapGetForm);
        GetFunc(refl, "setForm", "JFormMap", jFormMapSetForm);
        GetFunc(refl, "hasKey", "JFormMap", jFormMapHasKey);
        GetFunc(refl, "removeKey", "JFormMap", jFormMapRemoveKey);

        return true;
    }

    static void RequestActorDb() {
        if (ActorDb.load() > 0) {
            return;
        }

        bool expected = false;
        if (!ActorDbRequested.compare_exchange_strong(expected, true)) {
            return;
        }

        auto dh = RE::TESDataHandler::GetSingleton();
        auto quest = dh ? dh->LookupForm<RE::TESQuest>(0x0002E123, "SkyrimSlavery.esp") : nullptr;

        if (!quest) {
            ActorDbRequested.store(false);
            return;
        }

        bool ok = Papyrus::Call(
            quest,
            "BRSSControllerScript",
            "GetActorDb",
            [](RE::BSScript::Variable result) {
                if (result.IsInt()) {
                    std::int32_t id = result.GetSInt();

                    if (id > 0) {
                        ActorDb.store(id);
                        return;
                    }
                }

                ActorDbRequested.store(false);
            }
        );

        if (!ok) {
            ActorDbRequested.store(false);
        }
    }

    static void RequestMarkerDb() {
        if (MarkerDb.load() > 0) {
            return;
        }

        bool expected = false;
        if (!MarkerDbRequested.compare_exchange_strong(expected, true)) {
            return;
        }

        auto dh = RE::TESDataHandler::GetSingleton();
        auto quest = dh ? dh->LookupForm<RE::TESQuest>(0x0002E123, "SkyrimSlavery.esp") : nullptr;

        if (!quest) {
            MarkerDbRequested.store(false);
            return;
        }

        bool ok = Papyrus::Call(
            quest,
            "BRSSMarkerControllerScript",
            "GetMarkerDb",
            [](RE::BSScript::Variable result) {
                if (result.IsInt()) {
                    std::int32_t id = result.GetSInt();

                    if (id > 0) {
                        MarkerDb.store(id);
                        return;
                    }
                }

                MarkerDbRequested.store(false);
            }
        );

        if (!ok) {
            MarkerDbRequested.store(false);
        }
    }

    ObjectId GetActorDb() {
        ObjectId id = ActorDb.load();

        if (id > 0) {
            return id;
        }

        RequestActorDb();

        return ActorDb.load();
    }

    ObjectId GetMarkerDb() {
        ObjectId id = MarkerDb.load();

        if (id > 0) {
            return id;
        }

        RequestMarkerDb();

        return MarkerDb.load();
    }
}
