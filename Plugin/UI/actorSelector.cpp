#include "actorSelector.h"

#include <cstring>
#include <unordered_set>
#include <vector>

#include "../SKSEMenuFramework.h"

#include "common/builders.h"
#include "common/filter.h"

#include "../JCAPI.h"
#include "../utility.h"

enum class ActorSelectorFilterField : std::uint8_t {
    Unknown = 0,
    Name,
    Id,
    Type,
    Age,
    Status,
    Location,
    Distance,
};

static constexpr const char* kNameAliases[] = { "name", "na" };
static constexpr const char* kIdAliases[] = { "id" };
static constexpr const char* kTypeAliases[] = { "type", "ty" };
static constexpr const char* kAgeAliases[] = { "age", "ag" };
static constexpr const char* kStatusAliases[] = { "status", "st" };
static constexpr const char* kLocationAliases[] = { "location", "lo" };
static constexpr const char* kDistanceAliases[] = { "distance", "di" };

static constexpr FilterFieldSpec kActorSelectorFilterFields[] = {
    { static_cast<std::uint8_t>(ActorSelectorFilterField::Name), FilterFieldKind::Text, kNameAliases, std::size(kNameAliases), false },
    { static_cast<std::uint8_t>(ActorSelectorFilterField::Id), FilterFieldKind::Text, kIdAliases, std::size(kIdAliases), false },
    { static_cast<std::uint8_t>(ActorSelectorFilterField::Type), FilterFieldKind::Text, kTypeAliases, std::size(kTypeAliases), false },
    { static_cast<std::uint8_t>(ActorSelectorFilterField::Age), FilterFieldKind::Text, kAgeAliases, std::size(kAgeAliases), false },
    { static_cast<std::uint8_t>(ActorSelectorFilterField::Status), FilterFieldKind::Text, kStatusAliases, std::size(kStatusAliases), false },
    { static_cast<std::uint8_t>(ActorSelectorFilterField::Location), FilterFieldKind::Text, kLocationAliases, std::size(kLocationAliases), false },
    { static_cast<std::uint8_t>(ActorSelectorFilterField::Distance), FilterFieldKind::Number, kDistanceAliases, std::size(kDistanceAliases), false },
};

static constexpr std::uint8_t kActorSelectorTextShorthandFieldIds[] = {
    static_cast<std::uint8_t>(ActorSelectorFilterField::Name),
    static_cast<std::uint8_t>(ActorSelectorFilterField::Id),
    static_cast<std::uint8_t>(ActorSelectorFilterField::Type),
    static_cast<std::uint8_t>(ActorSelectorFilterField::Age),
    static_cast<std::uint8_t>(ActorSelectorFilterField::Status),
    static_cast<std::uint8_t>(ActorSelectorFilterField::Location),
};

static constexpr FilterShorthandConfig kActorSelectorFilterShorthand{
    static_cast<std::uint8_t>(ActorSelectorFilterField::Distance),
    { kActorSelectorTextShorthandFieldIds, std::size(kActorSelectorTextShorthandFieldIds) },
    {},
};

static constexpr FilterSchema kActorSelectorFilterSchema{
    kActorSelectorFilterFields,
    std::size(kActorSelectorFilterFields),
    kActorSelectorFilterShorthand,
};

static std::unordered_set<RE::Actor*> g_pendingSelection;
static std::vector<RE::Actor*> g_confirmedActors;
static bool g_hasConfirmedSelection = false;
static char g_filterBuffer[256] = {};
static char g_lastTokenizedFilter[256] = {};
static FilterTokenizeResult g_tokenizeResult = {};
static FilterParseResult g_parseResult = {};
static bool g_focusFilterOnOpen = false;
static bool g_closeOnDoubleClick = false;
static RE::Actor* g_navFocusedActor = nullptr;

struct ActorSelectorCachedEntry {
    RE::Actor* actor = nullptr;
    ActorTableRow row{};
};

static std::vector<ActorSelectorCachedEntry> g_cachedActors;
static float g_lastReferenceSearchRadius = -1.0f;

static constexpr float kDefaultReferenceSearchRadius = 1024.0f;
static constexpr const char kNearActorType[] = "Near";

static void AddActorToCacheIfNew(RE::Actor* actor, std::unordered_set<RE::FormID>& seen, bool fromNearbySearch = false) {
    if (!actor) {
        return;
    }

    const RE::FormID formId = actor->GetFormID();
    if (!seen.insert(formId).second) {
        return;
    }

    ActorSelectorCachedEntry entry;
    entry.actor = actor;
    PopulateActorTableRow(actor, nullptr, entry.row);
    if (fromNearbySearch) {
        entry.row.type = kNearActorType;
    }
    g_cachedActors.push_back(entry);
}

static void RebuildActorSelectorCache(float referenceSearchRadius) {
    g_cachedActors.clear();

    std::unordered_set<RE::FormID> seen;
    seen.reserve(256);
    g_cachedActors.reserve(256);

    const JC::ObjectId actorDb = JC::GetActorDb();
    if (actorDb != 0) {
        RE::TESForm* currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, nullptr, nullptr);
        while (currentForm) {
            RE::Actor* currentActor = currentForm->As<RE::Actor>();
            AddActorToCacheIfNew(currentActor, seen);
            currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
        }
    }

    const auto player = RE::PlayerCharacter::GetSingleton();
    if (player) {
        const std::vector<RE::TESObjectREFR*> nearbyRefs = Utility::FindAllReferencesOfFormTypes(
            player, { static_cast<std::uint32_t>(RE::FormType::NPC) }, referenceSearchRadius);
        for (RE::TESObjectREFR* ref : nearbyRefs) {
            AddActorToCacheIfNew(ref ? ref->As<RE::Actor>() : nullptr, seen, true);
        }
    }
}

static const char* ActorTableRowTextOrEmpty(std::string_view text) {
    return text.empty() ? "" : text.data();
}

static std::string_view ActorSelectorFilterRowText(const void* rowContext, std::uint8_t fieldId) {
    const auto& row = *static_cast<const ActorTableRow*>(rowContext);
    switch (static_cast<ActorSelectorFilterField>(fieldId)) {
    case ActorSelectorFilterField::Name:
        return row.name;
    case ActorSelectorFilterField::Id:
        return ActorTableRowIdHex(row);
    case ActorSelectorFilterField::Type:
        return row.type;
    case ActorSelectorFilterField::Age:
        return row.age;
    case ActorSelectorFilterField::Status:
        return row.status;
    case ActorSelectorFilterField::Location:
        return row.location;
    default:
        return {};
    }
}

static bool ActorSelectorFilterRowBool(const void* rowContext, std::uint8_t fieldId) {
    (void)rowContext;
    (void)fieldId;
    return false;
}

static float ActorSelectorFilterRowNumber(const void* rowContext, std::uint8_t fieldId) {
    const auto& row = *static_cast<const ActorTableRow*>(rowContext);
    switch (static_cast<ActorSelectorFilterField>(fieldId)) {
    case ActorSelectorFilterField::Distance:
        return row.distance;
    default:
        return 0.0f;
    }
}

static const FilterRowAccess kActorSelectorFilterRowAccess{
    ActorSelectorFilterRowText,
    ActorSelectorFilterRowBool,
    ActorSelectorFilterRowNumber,
};

static bool CachedEntryMatchesFilter(const ActorSelectorCachedEntry& entry, const FilterParseResult& parseResult) {
    return FilterMatchesExpression(
        parseResult, &entry.row, kActorSelectorFilterSchema, kActorSelectorFilterRowAccess);
}

static bool ActorSelectorFilterIsNonEmpty() {
    return g_filterBuffer[0] != '\0';
}

static void CollectVisibleActors(std::vector<RE::Actor*>& outActors, bool applyFilter) {
    outActors.clear();
    outActors.reserve(g_cachedActors.size());
    for (const ActorSelectorCachedEntry& entry : g_cachedActors) {
        if (!applyFilter || CachedEntryMatchesFilter(entry, g_parseResult)) {
            outActors.push_back(entry.actor);
        }
    }
}

static void UpdateSelectionOnRowClick(RE::Actor* actor) {
    const bool wasSelected = g_pendingSelection.contains(actor);
    ImGuiMCP::ImGuiIO* io = ImGuiMCP::GetIO();
    const bool ctrlHeld = io && (io->KeyCtrl || io->KeySuper);

    if (ctrlHeld) {
        if (wasSelected) {
            g_pendingSelection.erase(actor);
        } else {
            g_pendingSelection.insert(actor);
        }
        return;
    }

    g_pendingSelection.clear();
    if (!wasSelected) {
        g_pendingSelection.insert(actor);
    }
}

static void RenderActorSelectorTableRow(const ActorTableRow& row, RE::Actor* actor) {
    const bool selected = g_pendingSelection.contains(actor);

    ImGuiMCP::TableNextRow();
    ImGuiMCP::PushID(static_cast<int>(actor->GetFormID()));

    ImGuiMCP::TableSetColumnIndex(0);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.name));
    ImGuiMCP::TableSetColumnIndex(1);
    ImGuiMCP::Text("%08X", actor->GetFormID());
    ImGuiMCP::TableSetColumnIndex(2);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.type));
    ImGuiMCP::TableSetColumnIndex(3);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.age));
    ImGuiMCP::TableSetColumnIndex(4);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.status));
    ImGuiMCP::TableSetColumnIndex(5);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.location));
    ImGuiMCP::TableSetColumnIndex(6);
    ImGuiMCP::Text("%.2f", row.distance);

    ImGuiMCP::TableSetColumnIndex(0);
    constexpr ImGuiMCP::ImGuiSelectableFlags selectableFlags =
        ImGuiMCP::ImGuiSelectableFlags_SpanAllColumns | ImGuiMCP::ImGuiSelectableFlags_AllowOverlap |
        ImGuiMCP::ImGuiSelectableFlags_AllowDoubleClick |
        static_cast<ImGuiMCP::ImGuiSelectableFlags>(ImGuiMCP::ImGuiSelectableFlags_NoSetKeyOwner);
    if (ImGuiMCP::Selectable("##ActorSelectorRow", selected, selectableFlags)) {
        if (ImGuiMCP::IsMouseDoubleClicked(ImGuiMCP::ImGuiMouseButton_Left)) {
            g_confirmedActors.clear();
            g_confirmedActors.push_back(actor);
            g_hasConfirmedSelection = true;
            g_closeOnDoubleClick = true;
        } else {
            const bool enterActivatingRow = ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter, false) ||
                ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_KeypadEnter, false);
            if (!enterActivatingRow) {
                UpdateSelectionOnRowClick(actor);
            }
        }
    }
    if (ImGuiMCP::IsItemFocused()) {
        g_navFocusedActor = actor;
    }

    ImGuiMCP::PopID();
}

void UI::ActorSelector::Open() {
    g_hasConfirmedSelection = false;
    g_confirmedActors.clear();
    g_pendingSelection.clear();
    g_focusFilterOnOpen = true;
    if (g_lastReferenceSearchRadius < 0.0f) {
        g_lastReferenceSearchRadius = kDefaultReferenceSearchRadius;
    }
    RebuildActorSelectorCache(g_lastReferenceSearchRadius);
    if (Window) {
        Window->IsOpen = true;
    }
}

bool UI::ActorSelector::ConsumeSelectedActors(std::vector<RE::Actor*>& outActors) {
    if (!g_hasConfirmedSelection) {
        outActors.clear();
        return false;
    }

    outActors = g_confirmedActors;
    g_confirmedActors.clear();
    g_hasConfirmedSelection = false;
    return true;
}

void __stdcall UI::ActorSelector::Render() {
    auto viewport = ImGuiMCP::GetMainViewport();
    const ImGuiMCP::ImVec2 center{
        viewport->Pos.x + viewport->Size.x * 0.5f,
        viewport->Pos.y + viewport->Size.y * 0.5f
    };
    ImGuiMCP::SetNextWindowPos(center, ImGuiMCP::ImGuiCond_Appearing, ImGuiMCP::ImVec2{0.5f, 0.5f});
    ImGuiMCP::SetNextWindowSize(
        ImGuiMCP::ImVec2{viewport->Size.x * 0.5f, viewport->Size.y * 0.5f},
        ImGuiMCP::ImGuiCond_Appearing);
    ImGuiMCP::SetNextWindowBgAlpha(1.0f);
    ImGuiMCP::Begin("Actor selector##SkyrimSlavery", nullptr, 0);

    g_closeOnDoubleClick = false;

    if (g_cachedActors.empty()) {
        ImGuiMCP::Text("No actors found");
        ImGuiMCP::End();
        return;
    }

    const bool focusFilterShortcut = ImGuiMCP::Shortcut(
        ImGuiMCP::ImGuiMod_Ctrl | ImGuiMCP::ImGuiKey_L,
        ImGuiMCP::ImGuiInputFlags_RouteFocused | ImGuiMCP::ImGuiInputFlags_RouteOverActive);
    ImGuiMCP::ImGuiIO* io = ImGuiMCP::GetIO();
    const bool cancelShortcut = io && io->KeyCtrl && !io->KeyAlt && !io->KeySuper &&
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Q, false);

    if (g_focusFilterOnOpen || focusFilterShortcut) {
        ImGuiMCP::SetKeyboardFocusHere();
        g_focusFilterOnOpen = false;
    }
    ImGuiMCP::SetNextItemWidth(-1.0f);
    ImGuiMCP::InputTextWithHint("##ActorSelectorFilter", "Filter...", g_filterBuffer, sizeof(g_filterBuffer));
    ImGuiMCP::TextDisabled("Ctrl+L: focus filter | Ctrl+Enter: select all visible | Ctrl+Q: cancel");
    if (std::strcmp(g_filterBuffer, g_lastTokenizedFilter) != 0) {
        strncpy_s(g_lastTokenizedFilter, g_filterBuffer, sizeof(g_lastTokenizedFilter));
        g_lastTokenizedFilter[sizeof(g_lastTokenizedFilter) - 1] = '\0';
        FilterTokenize(g_filterBuffer, g_tokenizeResult);
        FilterParseExpression(g_filterBuffer, g_tokenizeResult, g_parseResult, kActorSelectorFilterSchema);

        const float referenceSearchRadius = FilterGetLessUpperBound(
            g_parseResult,
            static_cast<std::uint8_t>(ActorSelectorFilterField::Distance),
            kDefaultReferenceSearchRadius);
        if (referenceSearchRadius != g_lastReferenceSearchRadius) {
            g_lastReferenceSearchRadius = referenceSearchRadius;
            RebuildActorSelectorCache(referenceSearchRadius);
        }
    }
    if (!g_parseResult.ok && g_parseResult.error[0] != '\0') {
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", g_parseResult.error);
    }
    ImGuiMCP::Spacing();
    ImGuiMCP::TextDisabled("Selected: %zu", g_pendingSelection.size());
    ImGuiMCP::Spacing();

    const bool applyFilter = g_parseResult.ok && g_parseResult.hasExpression;

    ImGuiMCP::ImVec2 tableAvail{};
    ImGuiMCP::GetContentRegionAvail(&tableAvail);

    bool closeWindow = false;
    std::size_t visibleRowCount = 0;
    RE::Actor* singleVisibleActor = nullptr;

    if (ImGuiMCP::BeginChild("##ActorSelectorTable", ImGuiMCP::ImVec2{0.0f, tableAvail.y}, 0)) {
        constexpr ImGuiMCP::ImGuiTableFlags tableFlags = ImGuiMCP::ImGuiTableFlags_Resizable |
                                                         ImGuiMCP::ImGuiTableFlags_RowBg |
                                                         ImGuiMCP::ImGuiTableFlags_SizingFixedFit |
                                                         ImGuiMCP::ImGuiTableFlags_ScrollY;
        if (ImGuiMCP::BeginTable("ActorSelectorTable", 7, tableFlags)) {
            ImGuiMCP::TableSetupColumn("Name", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 300.0f);
            ImGuiMCP::TableSetupColumn("ID");
            ImGuiMCP::TableSetupColumn("Type");
            ImGuiMCP::TableSetupColumn("Age");
            ImGuiMCP::TableSetupColumn("Status");
            ImGuiMCP::TableSetupColumn("Location");
            ImGuiMCP::TableSetupColumn("Distance");
            ImGuiMCP::TableSetupScrollFreeze(0, 1);
            ImGuiMCP::TableHeadersRow();

            g_navFocusedActor = nullptr;
            for (const ActorSelectorCachedEntry& entry : g_cachedActors) {
                if (!applyFilter || CachedEntryMatchesFilter(entry, g_parseResult)) {
                    ++visibleRowCount;
                    singleVisibleActor = entry.actor;
                    RenderActorSelectorTableRow(entry.row, entry.actor);
                }
            }

            ImGuiMCP::EndTable();
        }
        ImGuiMCP::EndChild();
    }

    constexpr ImGuiMCP::ImGuiInputFlags kConfirmInputRoute = ImGuiMCP::ImGuiInputFlags_RouteGlobal;
    const bool confirmAllVisiblePressed =
        ImGuiMCP::Shortcut(ImGuiMCP::ImGuiMod_Ctrl | ImGuiMCP::ImGuiKey_Enter, kConfirmInputRoute) ||
        ImGuiMCP::Shortcut(ImGuiMCP::ImGuiMod_Ctrl | ImGuiMCP::ImGuiKey_KeypadEnter, kConfirmInputRoute);
    const bool confirmPressed = !confirmAllVisiblePressed && !(io && io->KeyCtrl) &&
        (ImGuiMCP::Shortcut(ImGuiMCP::ImGuiKey_Enter, kConfirmInputRoute) ||
            ImGuiMCP::Shortcut(ImGuiMCP::ImGuiKey_KeypadEnter, kConfirmInputRoute));
    const bool escPressed = ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Escape, false);

    if (confirmAllVisiblePressed && ActorSelectorFilterIsNonEmpty()) {
        CollectVisibleActors(g_confirmedActors, applyFilter);
        g_hasConfirmedSelection = true;
        closeWindow = true;
    } else if (confirmPressed) {
        if (!g_pendingSelection.empty()) {
            g_confirmedActors.assign(g_pendingSelection.begin(), g_pendingSelection.end());
        } else if (applyFilter && visibleRowCount == 1 && singleVisibleActor) {
            g_confirmedActors.clear();
            g_confirmedActors.push_back(singleVisibleActor);
        } else if (g_navFocusedActor) {
            g_confirmedActors.clear();
            g_confirmedActors.push_back(g_navFocusedActor);
        } else {
            g_confirmedActors.clear();
        }
        g_hasConfirmedSelection = true;
        closeWindow = true;
    } else if (escPressed || cancelShortcut) {
        g_hasConfirmedSelection = false;
        g_confirmedActors.clear();
        closeWindow = true;
    }

    if ((closeWindow || g_closeOnDoubleClick) && Window) {
        Window->IsOpen = false;
        g_cachedActors.clear();
    }

    ImGuiMCP::End();
}
