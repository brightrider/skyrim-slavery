#pragma once

#include <cstdint>

#include "jc_interface.h"

namespace RE
{
    class TESForm;
}

namespace jc
{
    struct root_interface;
}

namespace JC
{
    using ObjectId = std::int32_t;

    extern void* Domain;

    using JFormMapCount = ObjectId (*)(void*, ObjectId);
    using JFormMapNextKey = RE::TESForm* (*)(void*, ObjectId, RE::TESForm*, RE::TESForm*);
    using JFormMapGetInt = ObjectId (*)(void*, ObjectId, RE::TESForm*, ObjectId);
    using JFormMapSetInt = void (*)(void*, ObjectId, RE::TESForm*, ObjectId);
    using JFormMapGetObj = ObjectId (*)(void*, ObjectId, RE::TESForm*, ObjectId);
    using JFormMapSetObj = void (*)(void*, ObjectId, RE::TESForm*, ObjectId);
    using JFormMapGetForm = RE::TESForm* (*)(void*, ObjectId, RE::TESForm*, RE::TESForm*);
    using JFormMapSetForm = void (*)(void*, ObjectId, RE::TESForm*, RE::TESForm*);
    using JFormMapHasKey = bool (*)(void*, ObjectId, RE::TESForm*);
    using JFormMapRemoveKey = bool (*)(void*, ObjectId, RE::TESForm*);

    extern JFormMapCount jFormMapCount;
    extern JFormMapNextKey jFormMapNextKey;
    extern JFormMapGetInt jFormMapGetInt;
    extern JFormMapSetInt jFormMapSetInt;
    extern JFormMapGetObj jFormMapGetObj;
    extern JFormMapSetObj jFormMapSetObj;
    extern JFormMapGetForm jFormMapGetForm;
    extern JFormMapSetForm jFormMapSetForm;
    extern JFormMapHasKey jFormMapHasKey;
    extern JFormMapRemoveKey jFormMapRemoveKey;

    bool Load(const jc::root_interface* root);
    ObjectId GetActorDb();
}
