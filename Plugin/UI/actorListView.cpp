#include "actorListView.h"

#include <array>
#include <charconv>
#include <cctype>
#include <cstring>
#include <memory_resource>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../SKSEMenuFramework.h"

#include "common/builders.h"
#include "common/filter.h"

#include "../JCAPI.h"
#include "../PAPI.h"
#include "../utility.h"

#include "actorSelector.h"
#include "markerSelector.h"

enum class ActorFilterField : std::uint8_t {
    Unknown = 0,
    Name,
    Id,
    Type,
    Weapon,
    Age,
    Status,
    Location,
    Distance,
    Resource,
    Task,
};

static constexpr const char* kNameAliases[] = { "name", "na" };
static constexpr const char* kIdAliases[] = { "id" };
static constexpr const char* kTypeAliases[] = { "type", "ty" };
static constexpr const char* kWeaponAliases[] = { "weapon", "we" };
static constexpr const char* kAgeAliases[] = { "age", "ag" };
static constexpr const char* kStatusAliases[] = { "status", "st" };
static constexpr const char* kLocationAliases[] = { "location", "lo" };
static constexpr const char* kDistanceAliases[] = { "distance", "di" };
static constexpr const char* kResourceAliases[] = { "resource", "re" };
static constexpr const char* kTaskAliases[] = { "task", "ta" };

static constexpr FilterFieldSpec kActorFilterFields[] = {
    { static_cast<std::uint8_t>(ActorFilterField::Name), FilterFieldKind::Text, kNameAliases, std::size(kNameAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Id), FilterFieldKind::Text, kIdAliases, std::size(kIdAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Type), FilterFieldKind::Text, kTypeAliases, std::size(kTypeAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Weapon), FilterFieldKind::Text, kWeaponAliases, std::size(kWeaponAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Age), FilterFieldKind::Text, kAgeAliases, std::size(kAgeAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Status), FilterFieldKind::Text, kStatusAliases, std::size(kStatusAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Location), FilterFieldKind::Text, kLocationAliases, std::size(kLocationAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Distance), FilterFieldKind::Number, kDistanceAliases, std::size(kDistanceAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Resource), FilterFieldKind::Text, kResourceAliases, std::size(kResourceAliases), true },
    { static_cast<std::uint8_t>(ActorFilterField::Task), FilterFieldKind::Text, kTaskAliases, std::size(kTaskAliases), true },
};

static constexpr std::uint8_t kActorTextShorthandFieldIds[] = {
    static_cast<std::uint8_t>(ActorFilterField::Name),
    static_cast<std::uint8_t>(ActorFilterField::Id),
    static_cast<std::uint8_t>(ActorFilterField::Type),
    static_cast<std::uint8_t>(ActorFilterField::Weapon),
    static_cast<std::uint8_t>(ActorFilterField::Age),
    static_cast<std::uint8_t>(ActorFilterField::Status),
    static_cast<std::uint8_t>(ActorFilterField::Location),
    static_cast<std::uint8_t>(ActorFilterField::Resource),
    static_cast<std::uint8_t>(ActorFilterField::Task),
};

static constexpr FilterShorthandConfig kActorFilterShorthand{
    static_cast<std::uint8_t>(ActorFilterField::Distance),
    { kActorTextShorthandFieldIds, std::size(kActorTextShorthandFieldIds) },
    {},
};

static constexpr FilterSchema kActorFilterSchema{ kActorFilterFields, std::size(kActorFilterFields), kActorFilterShorthand };

static std::string_view ActorFilterRowText(const void* rowContext, std::uint8_t fieldId);
static bool ActorFilterRowBool(const void* rowContext, std::uint8_t fieldId);
static float ActorFilterRowNumber(const void* rowContext, std::uint8_t fieldId);

static const FilterRowAccess kActorFilterRowAccess{
    ActorFilterRowText,
    ActorFilterRowBool,
    ActorFilterRowNumber,
};

static constexpr const char* kActorRowDragDropPayloadType = "SS_ActorRow";

static constexpr int kActionButtonCount = 9;

static RE::Actor* g_pendingTravelActor = nullptr;
static RE::Actor* g_pendingUseActor = nullptr;
static RE::Actor* g_pendingPatrolActor = nullptr;
static RE::Actor* g_pendingAttackActor = nullptr;
static std::string_view g_actorListFilterResourceView;

static bool g_collectResourcePopupRequested = false;
static RE::Actor* g_collectResourceActor = nullptr;
static RE::TESBoundObject* g_collectResourceObject = nullptr;
static std::int32_t g_collectResourceMax = 0;
static int g_collectResourceAmount = 0;
static char g_collectResourcePopupLabel[160] = {};

struct OutfitPreset {
    const char* name;
    std::uint32_t formId;
};

static constexpr const char* kOutfitPlugin = "SkyrimSlaveryOutfits.esp";

static constexpr OutfitPreset kOutfitPresets[] = {
    { "Naked", 0xAA00 },
    { "Naked (HBH)", 0xAA01 },
    { "Cuffs", 0xFB07 },
    { "CrossPole", 0xFB06 },
    { "Chastity", 0xFB04 },
    { "Chastity (HBH)", 0xFB05 },
    { "Pee", 0xFB02 },
    { "Pee (HBH)", 0xFB03 },
};

static constexpr std::size_t kMaxOutfitVisibleActors = 1024;

static bool g_outfitPopupRequested = false;
static bool g_outfitApplyToAllVisible = false;
static bool g_outfitDeferPopupUntilVisibleCollected = false;
static RE::Actor* g_outfitActor = nullptr;
static RE::Actor* g_outfitAppliedActors[kMaxOutfitVisibleActors];
static std::size_t g_outfitAppliedActorCount = 0;

static void FormatActorResourceText(RE::Actor* actor, std::string& buf) {
    const auto [resource, count] = Utility::GetCurrentResource(actor);
    char countBuf[16];
    buf.clear();
    if (resource) {
        if (const char* name = resource->GetName()) {
            buf.append(name);
        }
    } else {
        buf.append("NORES");
    }
    buf.append(" [");
    const auto [ptr, ec] = std::to_chars(countBuf, countBuf + sizeof(countBuf), count);
    if (ec == std::errc{}) {
        buf.append(countBuf, ptr);
    }
    buf.append("]");
}

static void RequestCollectResourcePopup(RE::Actor* actor) {
    const auto [resource, count] = Utility::GetCurrentResource(actor);
    g_collectResourceActor = actor;
    g_collectResourceObject = resource;
    g_collectResourceMax = count;
    g_collectResourceAmount = static_cast<int>(count);

    char nameBuf[128] = {};
    if (resource) {
        if (const char* name = resource->GetName()) {
            strncpy_s(nameBuf, name, _TRUNCATE);
        }
    } else {
        strncpy_s(nameBuf, "NORES", _TRUNCATE);
    }
    strncpy_s(g_collectResourcePopupLabel, nameBuf, _TRUNCATE);
    strncat_s(g_collectResourcePopupLabel, "##CollectResource", _TRUNCATE);

    g_collectResourcePopupRequested = true;
}

static void OnCollectResourceConfirmed(RE::Actor* actor, RE::TESBoundObject* resource, std::int32_t amount) {
    actor->RemoveItem(resource, amount, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, RE::PlayerCharacter::GetSingleton());
}

static void RenderCollectResourcePopup() {
    if (g_collectResourcePopupRequested) {
        ImGuiMCP::OpenPopup(g_collectResourcePopupLabel);
        g_collectResourcePopupRequested = false;
    }

    constexpr ImGuiMCP::ImGuiWindowFlags popupFlags = ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize;
    if (!ImGuiMCP::BeginPopupModal(g_collectResourcePopupLabel, nullptr, popupFlags)) {
        return;
    }

    ImGuiMCP::SetNextItemWidth(240.0f);
    ImGuiMCP::SliderInt(
        "##CollectResourceAmount",
        &g_collectResourceAmount,
        0,
        static_cast<int>(g_collectResourceMax),
        "%d",
        ImGuiMCP::ImGuiSliderFlags_AlwaysClamp);
    if (ImGuiMCP::IsWindowAppearing()) {
        ImGuiMCP::SetKeyboardFocusHere(-1);
    }

    ImGuiMCP::ImGuiIO* io = ImGuiMCP::GetIO();
    const bool cancelShortcut = io && io->KeyCtrl && !io->KeyAlt && !io->KeySuper &&
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Q, false);
    const bool confirmPressed =
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter, false) ||
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_KeypadEnter, false);

    bool closePopup = false;
    if (confirmPressed) {
        OnCollectResourceConfirmed(g_collectResourceActor, g_collectResourceObject, g_collectResourceAmount);
        closePopup = true;
    } else if (cancelShortcut) {
        closePopup = true;
    }

    if (closePopup) {
        g_collectResourceActor = nullptr;
        g_collectResourceObject = nullptr;
        g_collectResourceMax = 0;
        ImGuiMCP::CloseCurrentPopup();
    }

    ImGuiMCP::EndPopup();
}

static void ApplyOutfitToActor(RE::Actor* actor, RE::BGSOutfit* outfit) {
    if (!actor || !outfit) {
        return;
    }
    Papyrus::Call(
        actor,
        "BRSSActorScript",
        "SetOutfit",
        [](RE::BSScript::Variable) {},
        static_cast<RE::BGSOutfit*>(outfit),
        false
    );
}

static void RequestOutfitPopup(RE::Actor* actor, bool applyToAllVisible) {
    g_outfitApplyToAllVisible = applyToAllVisible;
    if (applyToAllVisible) {
        g_outfitActor = nullptr;
        g_outfitDeferPopupUntilVisibleCollected = true;
    } else {
        g_outfitActor = actor;
        g_outfitPopupRequested = true;
    }
}

static void RenderOutfitPopup() {
    if (g_outfitPopupRequested) {
        ImGuiMCP::OpenPopup("Select outfit");
        g_outfitPopupRequested = false;
    }

    constexpr ImGuiMCP::ImGuiWindowFlags popupFlags = ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize;
    if (!ImGuiMCP::BeginPopupModal("Select outfit", nullptr, popupFlags)) {
        return;
    }

    RE::TESDataHandler* dh = RE::TESDataHandler::GetSingleton();
    RE::Actor* actor = g_outfitActor;
    bool closePopup = false;

    for (const OutfitPreset& preset : kOutfitPresets) {
        if (ImGuiMCP::Selectable(preset.name, false)) {
            if (dh) {
                RE::BGSOutfit* outfit = dh->LookupForm<RE::BGSOutfit>(preset.formId, kOutfitPlugin);
                if (outfit) {
                    if (g_outfitApplyToAllVisible) {
                        for (std::size_t i = 0; i < g_outfitAppliedActorCount; ++i) {
                            ApplyOutfitToActor(g_outfitAppliedActors[i], outfit);
                        }
                    } else {
                        ApplyOutfitToActor(actor, outfit);
                    }
                }
            }
            closePopup = true;
            break;
        }
    }

    ImGuiMCP::ImGuiIO* io = ImGuiMCP::GetIO();
    const bool cancelShortcut = io && io->KeyCtrl && !io->KeyAlt && !io->KeySuper &&
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Q, false);
    if (cancelShortcut || ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Escape, false)) {
        closePopup = true;
    }

    if (closePopup) {
        g_outfitActor = nullptr;
        g_outfitApplyToAllVisible = false;
        g_outfitAppliedActorCount = 0;
        ImGuiMCP::CloseCurrentPopup();
    }

    ImGuiMCP::EndPopup();
}

static RE::Actor* ActorPtrFromDragDropPayload(const ImGuiMCP::ImGuiPayload* payload) {
    if (!payload || payload->DataSize != static_cast<int>(sizeof(RE::Actor*))) {
        return nullptr;
    }
    return *static_cast<RE::Actor* const*>(payload->Data);
}

static void OnActorRowDragDropDelivered(RE::Actor* sourceActor, RE::Actor* targetActor) {
    if (!sourceActor) {
        return;
    }

    if (targetActor) {
        Papyrus::Call(
            sourceActor,
            "BRSSActorScript",
            "Follow",
            [](RE::BSScript::Variable) {},
            static_cast<RE::TESObjectREFR*>(targetActor),
            false
        );
    } else {
        Papyrus::Call(
            sourceActor,
            "BRSSActorScript",
            "Wait",
            [](RE::BSScript::Variable) {},
            false
        );
    }
}

static void AcceptActorRowDragDropAtTarget(RE::Actor* targetActor) {
    if (!ImGuiMCP::BeginDragDropTarget()) {
        return;
    }
    const ImGuiMCP::ImGuiPayload* payload =
        ImGuiMCP::AcceptDragDropPayload(kActorRowDragDropPayloadType);
    if (payload) {
        RE::Actor* sourceActor = ActorPtrFromDragDropPayload(payload);
        if (sourceActor && payload->Delivery) {
            OnActorRowDragDropDelivered(sourceActor, targetActor);
        }
    }
    ImGuiMCP::EndDragDropTarget();
}

using FollowChildrenMap = std::pmr::unordered_map<RE::Actor*, std::pmr::vector<RE::Actor*>>;
using FollowChildSet = std::pmr::unordered_set<RE::Actor*>;

static const char* ActorTableRowTextOrEmpty(std::string_view text) {
    return text.empty() ? "" : text.data();
}

static bool ActorNameHasNonWhitespace(const char* text) {
    if (!text || text[0] == '\0') {
        return false;
    }
    for (const char* p = text; *p != '\0'; ++p) {
        if (!std::isspace(static_cast<unsigned char>(*p))) {
            return true;
        }
    }
    return false;
}

static void CopyActorNameToEditBuffer(const ActorTableRow& row, char* buffer, std::size_t bufferSize) {
    const char* src = ActorTableRowTextOrEmpty(row.name);
    strncpy_s(buffer, bufferSize, src, _TRUNCATE);
}

static std::string_view ActorFilterRowText(const void* rowContext, std::uint8_t fieldId) {
    const auto& row = *static_cast<const ActorTableRow*>(rowContext);
    switch (static_cast<ActorFilterField>(fieldId)) {
    case ActorFilterField::Name:
        return row.name;
    case ActorFilterField::Id:
        return ActorTableRowIdHex(row);
    case ActorFilterField::Type:
        return row.type;
    case ActorFilterField::Weapon:
        return row.weapon;
    case ActorFilterField::Age:
        return row.age;
    case ActorFilterField::Status:
        return row.status;
    case ActorFilterField::Location:
        return row.location;
    case ActorFilterField::Resource:
        return g_actorListFilterResourceView;
    case ActorFilterField::Task:
        return row.task;
    default:
        return {};
    }
}

static bool ActorFilterRowBool(const void* rowContext, std::uint8_t fieldId) {
    (void)rowContext;
    (void)fieldId;
    return false;
}

static float ActorFilterRowNumber(const void* rowContext, std::uint8_t fieldId) {
    const auto& row = *static_cast<const ActorTableRow*>(rowContext);
    switch (static_cast<ActorFilterField>(fieldId)) {
    case ActorFilterField::Distance:
        return row.distance;
    default:
        return 0.0f;
    }
}

static RE::Actor* GetFollowParentInActorDb(
    RE::Actor* actor,
    RE::BGSKeyword* packageKeyword1,
    JC::ObjectId actorDb) {
    if (!actor || !packageKeyword1 || !JC::jFormMapHasKey) {
        return nullptr;
    }

    RE::ActorValueOwner* valueOwner = actor->AsActorValueOwner();
    if (!valueOwner || static_cast<int>(valueOwner->GetBaseActorValue(RE::ActorValue::kVariable08)) != 2) {
        return nullptr;
    }

    RE::TESObjectREFR* link = actor->GetLinkedRef(packageKeyword1);
    RE::Actor* parent = link ? link->As<RE::Actor>() : nullptr;
    if (!parent || !JC::jFormMapHasKey(JC::Domain, actorDb, parent)) {
        return nullptr;
    }

    return parent;
}

static bool FollowSubtreeMatchesFilter(
    RE::Actor* actor,
    const FollowChildrenMap& followChildrenByParent,
    const FilterParseResult& parseResult,
    bool fillExpensiveFields,
    std::string& taskBuffer,
    std::string& resourceBuffer,
    ActorTableRow& row) {
    PopulateActorTableRow(actor, fillExpensiveFields ? &taskBuffer : nullptr, row);
    if (fillExpensiveFields) {
        FormatActorResourceText(actor, resourceBuffer);
        g_actorListFilterResourceView = resourceBuffer;
    } else {
        g_actorListFilterResourceView = {};
    }
    if (FilterMatchesExpression(parseResult, &row, kActorFilterSchema, kActorFilterRowAccess)) {
        return true;
    }

    const auto childrenIt = followChildrenByParent.find(actor);
    if (childrenIt == followChildrenByParent.end()) {
        return false;
    }

    for (RE::Actor* child : childrenIt->second) {
        if (FollowSubtreeMatchesFilter(child, followChildrenByParent, parseResult, fillExpensiveFields, taskBuffer, resourceBuffer, row)) {
            return true;
        }
    }

    return false;
}

static void RenderActorTableRow(
    const ActorTableRow& row,
    std::string_view resourceText,
    std::string_view taskText,
    RE::Actor* actor,
    int followDepth,
    bool hideTaskColumn) {
    static char nameEditBuffer[256] = {};

    ImGuiMCP::TableNextRow();
    ImGuiMCP::TableSetColumnIndex(0);

    const float indentAmount = followDepth > 0
        ? static_cast<float>(followDepth) * ImGuiMCP::GetStyle()->IndentSpacing
        : 0.0f;
    if (indentAmount > 0.0f) {
        ImGuiMCP::Indent(indentAmount);
    }

    ImGuiMCP::PushID(static_cast<int>(actor->GetFormID()));
    const ImGuiMCP::ImGuiID nameFieldId = ImGuiMCP::GetID("##ActorName");
    if (ImGuiMCP::GetActiveID() != nameFieldId) {
        CopyActorNameToEditBuffer(row, nameEditBuffer, sizeof(nameEditBuffer));
    }
    ImGuiMCP::SetNextItemWidth(-ImGuiMCP::GET_FLT_MIN());
    constexpr ImGuiMCP::ImVec4 kTransparentFrame{ 0.0f, 0.0f, 0.0f, 0.0f };
    ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FramePadding, ImGuiMCP::ImVec2(0.0f, 0.0f));
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_FrameBg, kTransparentFrame);
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_FrameBgHovered, kTransparentFrame);
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_FrameBgActive, kTransparentFrame);
    ImGuiMCP::InputText("##ActorName", nameEditBuffer, sizeof(nameEditBuffer));
    ImGuiMCP::PopStyleColor(3);
    ImGuiMCP::PopStyleVar(2);
    if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
        if (ActorNameHasNonWhitespace(nameEditBuffer)) {
            Papyrus::Call(
                actor,
                "BRSSActorScript",
                "SetActorName",
                [](RE::BSScript::Variable) {},
                std::string(nameEditBuffer)
            );
        } else {
            CopyActorNameToEditBuffer(row, nameEditBuffer, sizeof(nameEditBuffer));
        }
    }
    ImGuiMCP::PopID();

    if (indentAmount > 0.0f) {
        ImGuiMCP::Unindent(indentAmount);
    }

    ImGuiMCP::TableSetColumnIndex(1);
    ImGuiMCP::Text("%08X", actor->GetFormID());
    ImGuiMCP::TableSetColumnIndex(2);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.type));
    ImGuiMCP::TableSetColumnIndex(3);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.weapon));
    ImGuiMCP::TableSetColumnIndex(4);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.age));
    ImGuiMCP::TableSetColumnIndex(5);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.status));
    ImGuiMCP::TableSetColumnIndex(6);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.location));
    ImGuiMCP::TableSetColumnIndex(7);
    ImGuiMCP::Text("%.2f", row.distance);
    ImGuiMCP::TableSetColumnIndex(8);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(resourceText));
    ImGuiMCP::TableSetColumnIndex(9);
    if (!hideTaskColumn) {
        ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(taskText));
    }
    ImGuiMCP::TableSetColumnIndex(10);
    ImGuiMCP::PushID(static_cast<int>(actor->GetFormID()));
    const float actionButtonSize = ImGuiMCP::GetFrameHeight();
    const ImGuiMCP::ImVec2 actionButtonSizeVec{ actionButtonSize, actionButtonSize };
    if (ImGuiMCP::Button("W", actionButtonSizeVec)) {
        Papyrus::Call(
            actor,
            "BRSSActorScript",
            "Wait",
            [](RE::BSScript::Variable) {},
            false
        );
    }
    ImGuiMCP::SetItemTooltip("Wait");
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("T", actionButtonSizeVec)) {
        g_pendingTravelActor = actor;
        g_pendingUseActor = nullptr;
        g_pendingPatrolActor = nullptr;
        g_pendingAttackActor = nullptr;
        UI::MarkerSelector::Open();
    }
    ImGuiMCP::SetItemTooltip("Travel");
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("F", actionButtonSizeVec)) {
        Papyrus::Call(
            actor,
            "BRSSActorScript",
            "Follow",
            [](RE::BSScript::Variable) {},
            static_cast<RE::TESObjectREFR*>(RE::PlayerCharacter::GetSingleton()),
            false
        );
    }
    ImGuiMCP::SetItemTooltip("Follow player");
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("U", actionButtonSizeVec)) {
        g_pendingUseActor = actor;
        g_pendingTravelActor = nullptr;
        g_pendingPatrolActor = nullptr;
        g_pendingAttackActor = nullptr;
        UI::MarkerSelector::Open("1024");
    }
    ImGuiMCP::SetItemTooltip("Use");
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("P", actionButtonSizeVec)) {
        g_pendingPatrolActor = actor;
        g_pendingTravelActor = nullptr;
        g_pendingUseActor = nullptr;
        g_pendingAttackActor = nullptr;
        UI::MarkerSelector::Open("1024");
    }
    ImGuiMCP::SetItemTooltip("Patrol");
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("A", actionButtonSizeVec)) {
        g_pendingAttackActor = actor;
        g_pendingTravelActor = nullptr;
        g_pendingUseActor = nullptr;
        g_pendingPatrolActor = nullptr;
        UI::ActorSelector::Open("slave 1024", true);
    }
    ImGuiMCP::SetItemTooltip("Attack");
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("O", actionButtonSizeVec)) {
        ImGuiMCP::ImGuiIO* clickIo = ImGuiMCP::GetIO();
        const bool ctrlHeld = clickIo && (clickIo->KeyCtrl || clickIo->KeySuper);
        RequestOutfitPopup(actor, ctrlHeld);
    }
    ImGuiMCP::SetItemTooltip("Outfit  |  Ctrl+click: all visible");
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("C", actionButtonSizeVec)) {
        RequestCollectResourcePopup(actor);
    }
    ImGuiMCP::SetItemTooltip("Collect resources");
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("X", actionButtonSizeVec)) {
        Utility::SetHealth(actor, 1.0f);
        Papyrus::Call(
            actor,
            "BRSSActorScript",
            "RemoveFromBRSS",
            [](RE::BSScript::Variable) {}
        );
    }
    ImGuiMCP::PopID();

    ImGuiMCP::TableSetColumnIndex(0);
    ImGuiMCP::PushID(static_cast<int>(actor->GetFormID()));
    ImGuiMCP::Selectable(
        "##ActorRowDnD",
        false,
        ImGuiMCP::ImGuiSelectableFlags_SpanAllColumns | ImGuiMCP::ImGuiSelectableFlags_AllowOverlap);
    if (ImGuiMCP::BeginDragDropSource(ImGuiMCP::ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGuiMCP::SetDragDropPayload(kActorRowDragDropPayloadType, &actor, sizeof(RE::Actor*));
        ImGuiMCP::EndDragDropSource();
    }
    AcceptActorRowDragDropAtTarget(actor);
    ImGuiMCP::PopID();
}

static void CollectOutfitVisibleFollowSubtree(
    RE::Actor* actor,
    const FollowChildrenMap& followChildrenByParent,
    std::size_t& count) {
    if (count < kMaxOutfitVisibleActors) {
        g_outfitAppliedActors[count++] = actor;
    }
    const auto childrenIt = followChildrenByParent.find(actor);
    if (childrenIt == followChildrenByParent.end()) {
        return;
    }
    for (RE::Actor* child : childrenIt->second) {
        CollectOutfitVisibleFollowSubtree(child, followChildrenByParent, count);
    }
}

static void CollectOutfitVisibleActors(
    JC::ObjectId actorDb,
    const FollowChildrenMap& followChildrenByParent,
    const FollowChildSet& followChildSet,
    bool applyFilter,
    const FilterParseResult& parseResult,
    bool fillExpensiveFields,
    std::string& taskBuffer,
    std::string& resourceBuffer,
    ActorTableRow& row) {
    g_outfitAppliedActorCount = 0;
    RE::TESForm* currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, nullptr, nullptr);
    while (currentForm) {
        RE::Actor* currentActor = currentForm->As<RE::Actor>();
        if (currentActor && followChildSet.find(currentActor) == followChildSet.end()) {
            if (!applyFilter ||
                FollowSubtreeMatchesFilter(currentActor, followChildrenByParent, parseResult, fillExpensiveFields, taskBuffer, resourceBuffer, row)) {
                CollectOutfitVisibleFollowSubtree(currentActor, followChildrenByParent, g_outfitAppliedActorCount);
            }
        }
        currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
    }
}

static void RenderFollowSubtree(
    RE::Actor* actor,
    int followDepth,
    const FollowChildrenMap& followChildrenByParent,
    bool fillExpensiveFields,
    std::string& taskBuffer,
    std::string& resourceBuffer,
    ActorTableRow& row) {
    PopulateActorTableRow(actor, nullptr, row);

    FormatActorResourceText(actor, resourceBuffer);

    std::string_view taskDisplay = row.task;
    if (followDepth == 0) {
        taskBuffer.clear();
        Utility::CreateTaskDescription(actor, taskBuffer);
        taskDisplay = taskBuffer;
    }

    const bool hideTaskColumn = followDepth > 0;
    RenderActorTableRow(row, resourceBuffer, taskDisplay, actor, followDepth, hideTaskColumn);

    const auto childrenIt = followChildrenByParent.find(actor);
    if (childrenIt == followChildrenByParent.end()) {
        return;
    }

    for (RE::Actor* child : childrenIt->second) {
        RenderFollowSubtree(child, followDepth + 1, followChildrenByParent, fillExpensiveFields, taskBuffer, resourceBuffer, row);
    }
}

void __stdcall UI::ActorListView::Render() {
    static char filterBuffer[256] = {};
    static char lastTokenizedFilter[256] = {};
    static FilterTokenizeResult tokenizeResult = {};
    static FilterParseResult parseResult = {};
    static bool filterUsesExpensiveField = false;
    static std::string taskBuffer = [] {
        std::string s;
        s.reserve(128);
        return s;
    }();
    static std::string resourceBuffer = [] {
        std::string s;
        s.reserve(64);
        return s;
    }();

    JC::ObjectId actorDb = JC::GetActorDb();
    if (actorDb == 0) {
        ImGuiMCP::Text("No actor database found");
        return;
    }

    RE::TESDataHandler* dh = RE::TESDataHandler::GetSingleton();
    if (!dh) {
        return;
    }

    static auto* BRSS_PackageKeyword1 = dh->LookupForm<RE::BGSKeyword>(0xAA0F, "SkyrimSlavery.esp");
    if (!BRSS_PackageKeyword1) {
        return;
    }

    ImGuiMCP::ImGuiIO* io = ImGuiMCP::GetIO();
    constexpr ImGuiMCP::ImGuiInputFlags kFilterShortcutRoute =
        ImGuiMCP::ImGuiInputFlags_RouteFocused | ImGuiMCP::ImGuiInputFlags_RouteOverActive;
    const bool focusFilterShortcut = io && io->KeyCtrl && !io->KeyAlt && !io->KeySuper &&
        ImGuiMCP::IsWindowFocused(ImGuiMCP::ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_L, false);
    if (focusFilterShortcut) {
        ImGuiMCP::SetKeyboardFocusHere();
    }
    if (ImGuiMCP::Shortcut(ImGuiMCP::ImGuiMod_Ctrl | ImGuiMCP::ImGuiKey_D, kFilterShortcutRoute)) {
        FilterToggleTrailingDistance(filterBuffer, sizeof(filterBuffer));
    }
    ImGuiMCP::SetNextItemWidth(-1.0f);
    ImGuiMCP::InputTextWithHint("##ActorListFilter", "Filter...", filterBuffer, sizeof(filterBuffer));
    AcceptActorRowDragDropAtTarget(nullptr);
    ImGuiMCP::TextDisabled(
        "Ctrl+L: focus filter  |  Ctrl+D: distance toggle  |  e.g. lydia 1000 or name ct lydia and ag ct child  |  not, ==, contains(ct), startswith(sw), endswith(ew), <, >  |  a or b and c = a or (b and c)");
    AcceptActorRowDragDropAtTarget(nullptr);
    if (std::strcmp(filterBuffer, lastTokenizedFilter) != 0) {
        strncpy_s(lastTokenizedFilter, filterBuffer, sizeof(lastTokenizedFilter));
        lastTokenizedFilter[sizeof(lastTokenizedFilter) - 1] = '\0';
        FilterTokenize(filterBuffer, tokenizeResult);
        FilterParseExpression(filterBuffer, tokenizeResult, parseResult, kActorFilterSchema);
        filterUsesExpensiveField = FilterExpressionUsesExpensiveField(parseResult, kActorFilterSchema);
    }
    if (!parseResult.ok && parseResult.error[0] != '\0') {
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", parseResult.error);
    }
    ImGuiMCP::Spacing();
    AcceptActorRowDragDropAtTarget(nullptr);

    const bool applyFilter = parseResult.ok && parseResult.hasExpression;
    const bool fillExpensiveFields = applyFilter && filterUsesExpensiveField;

    alignas(std::max_align_t) static std::array<std::byte, 2 * 1024 * 1024> followGraphBuffer;
    std::pmr::monotonic_buffer_resource followGraphPool{
        followGraphBuffer.data(),
        followGraphBuffer.size(),
        std::pmr::null_memory_resource()
    };
    FollowChildrenMap followChildrenByParent{ &followGraphPool };
    FollowChildSet followChildSet{ &followGraphPool };
    followChildrenByParent.reserve(4096);
    followChildSet.reserve(4096);

    RE::TESForm* currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, nullptr, nullptr);
    while (currentForm) {
        RE::Actor* currentActor = currentForm->As<RE::Actor>();
        if (currentActor) {
            RE::Actor* parent = GetFollowParentInActorDb(currentActor, BRSS_PackageKeyword1, actorDb);
            if (parent) {
                followChildrenByParent[parent].push_back(currentActor);
                followChildSet.insert(currentActor);
            }
        }
        currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
    }

    ActorTableRow row = {};

    ImGuiMCP::ImVec2 tableAvail{};
    ImGuiMCP::GetContentRegionAvail(&tableAvail);

    if (ImGuiMCP::BeginChild("##ActorListTable", ImGuiMCP::ImVec2{0.0f, tableAvail.y}, 0)) {
        constexpr ImGuiMCP::ImGuiTableFlags tableFlags = ImGuiMCP::ImGuiTableFlags_Resizable |
                                                         ImGuiMCP::ImGuiTableFlags_RowBg |
                                                         ImGuiMCP::ImGuiTableFlags_SizingFixedFit |
                                                         ImGuiMCP::ImGuiTableFlags_ScrollY;
        const float actionButtonSize = ImGuiMCP::GetFrameHeight();
        const float actionsColumnWidth = actionButtonSize * static_cast<float>(kActionButtonCount) +
                                         ImGuiMCP::GetStyle()->ItemSpacing.x * static_cast<float>(kActionButtonCount - 1) +
                                         ImGuiMCP::GetStyle()->CellPadding.x * 2.0f;
        if (ImGuiMCP::BeginTable("ActorListTable", 11, tableFlags)) {
            ImGuiMCP::TableSetupColumn("Name", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 200.0f);
            ImGuiMCP::TableSetupColumn("ID");
            ImGuiMCP::TableSetupColumn("Type");
            ImGuiMCP::TableSetupColumn("Weapon");
            ImGuiMCP::TableSetupColumn("Age");
            ImGuiMCP::TableSetupColumn("Status");
            ImGuiMCP::TableSetupColumn("Location");
            ImGuiMCP::TableSetupColumn("Distance");
            ImGuiMCP::TableSetupColumn("Resource");
            ImGuiMCP::TableSetupColumn("Task", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
            ImGuiMCP::TableSetupColumn("", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, actionsColumnWidth);
            ImGuiMCP::TableSetupScrollFreeze(0, 1);
            ImGuiMCP::TableHeadersRow();
            ImGuiMCP::TableSetColumnIndex(0);
            ImGuiMCP::Selectable(
                "##ActorTableHeaderDnD",
                false,
                ImGuiMCP::ImGuiSelectableFlags_SpanAllColumns | ImGuiMCP::ImGuiSelectableFlags_AllowOverlap);
            AcceptActorRowDragDropAtTarget(nullptr);

            currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, nullptr, nullptr);
            while (currentForm) {
                RE::Actor* currentActor = currentForm->As<RE::Actor>();
                if (currentActor && followChildSet.find(currentActor) == followChildSet.end()) {
                    if (!applyFilter ||
                        FollowSubtreeMatchesFilter(currentActor, followChildrenByParent, parseResult, fillExpensiveFields, taskBuffer, resourceBuffer, row)) {
                        RenderFollowSubtree(currentActor, 0, followChildrenByParent, fillExpensiveFields, taskBuffer, resourceBuffer, row);
                    }
                }
                currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
            }

            if (g_outfitDeferPopupUntilVisibleCollected) {
                CollectOutfitVisibleActors(
                    actorDb,
                    followChildrenByParent,
                    followChildSet,
                    applyFilter,
                    parseResult,
                    fillExpensiveFields,
                    taskBuffer,
                    resourceBuffer,
                    row);
                g_outfitDeferPopupUntilVisibleCollected = false;
                g_outfitPopupRequested = true;
            }

            ImGuiMCP::EndTable();
        }
        AcceptActorRowDragDropAtTarget(nullptr);
        ImGuiMCP::EndChild();
    }

    RenderCollectResourcePopup();
    RenderOutfitPopup();

    if (MarkerSelector::Window && !MarkerSelector::Window->IsOpen &&
        (g_pendingTravelActor || g_pendingUseActor || g_pendingPatrolActor)) {
        RE::Actor* pendingActor = g_pendingTravelActor
            ? g_pendingTravelActor
            : (g_pendingUseActor ? g_pendingUseActor : g_pendingPatrolActor);
        const bool pendingUse = g_pendingUseActor != nullptr;
        const bool pendingPatrol = g_pendingPatrolActor != nullptr;
        g_pendingTravelActor = nullptr;
        g_pendingUseActor = nullptr;
        g_pendingPatrolActor = nullptr;

        std::vector<RE::TESObjectREFR*> selectedMarkers;
        if (MarkerSelector::ConsumeSelectedMarkers(selectedMarkers) && !selectedMarkers.empty()) {
            if (pendingPatrol) {
                if (selectedMarkers.size() >= 2) {
                    Papyrus::Call(
                        pendingActor,
                        "BRSSActorScript",
                        "Patrol",
                        [](RE::BSScript::Variable) {},
                        static_cast<RE::TESObjectREFR*>(selectedMarkers[0]),
                        static_cast<RE::TESObjectREFR*>(selectedMarkers[1]),
                        false
                    );
                }
            } else if (pendingUse) {
                Papyrus::Call(
                    pendingActor,
                    "BRSSActorScript",
                    "Use",
                    [](RE::BSScript::Variable) {},
                    static_cast<RE::TESObjectREFR*>(selectedMarkers[0]),
                    static_cast<RE::TESObjectREFR*>(nullptr),
                    RE::BSFixedString(""),
                    false
                );
            } else {
                Papyrus::Call(
                    pendingActor,
                    "BRSSActorScript",
                    "Travel",
                    [](RE::BSScript::Variable) {},
                    static_cast<RE::TESObjectREFR*>(selectedMarkers[0]),
                    false
                );
            }
        }
    }

    if (g_pendingAttackActor && ActorSelector::Window && !ActorSelector::Window->IsOpen) {
        RE::Actor* pendingActor = g_pendingAttackActor;
        g_pendingAttackActor = nullptr;

        std::vector<RE::Actor*> selectedActors;
        if (ActorSelector::ConsumeSelectedActors(selectedActors) && !selectedActors.empty()) {
            Papyrus::Call(
                pendingActor,
                "BRSSActorScript",
                "Attack",
                [](RE::BSScript::Variable) {},
                static_cast<RE::Actor*>(selectedActors[0]),
                false
            );
        }
    }
}
