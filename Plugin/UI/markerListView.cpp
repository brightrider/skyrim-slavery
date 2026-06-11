#include "markerListView.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory_resource>
#include <vector>

#include "../SKSEMenuFramework.h"

#include "common/builders.h"
#include "common/filter.h"

#include "../JCAPI.h"
#include "../PAPI.h"
#include "formSelector.h"

struct RevLinks {
    std::array<RE::TESObjectREFR*, 128> refs;
    std::uint16_t count = 0;
};

enum class MarkerListFilterField : std::uint8_t {
    MName = 1,
    MId,
    MType,
    MDesc,
    MLocation,
    MDistance,
    AName = 16,
    AId,
    AType,
    AWeapon,
    AAge,
    AStatus,
    ALocation,
    ADistance,
    ATask,
};

static constexpr const char* kMNameAliases[] = { "mname", "mna" };
static constexpr const char* kMIdAliases[] = { "mid" };
static constexpr const char* kMTypeAliases[] = { "mtype", "mty" };
static constexpr const char* kMDescAliases[] = { "mdesc", "mde" };
static constexpr const char* kMLocationAliases[] = { "mloc", "mlo" };
static constexpr const char* kMDistanceAliases[] = { "mdist", "mdi" };
static constexpr const char* kNameAliases[] = { "name", "na" };
static constexpr const char* kIdAliases[] = { "id" };
static constexpr const char* kTypeAliases[] = { "type", "ty" };
static constexpr const char* kWeaponAliases[] = { "weapon", "we" };
static constexpr const char* kAgeAliases[] = { "age", "ag" };
static constexpr const char* kStatusAliases[] = { "status", "st" };
static constexpr const char* kLocationAliases[] = { "location", "lo" };
static constexpr const char* kDistanceAliases[] = { "distance", "di" };
static constexpr const char* kTaskAliases[] = { "task", "ta" };

static constexpr FilterFieldSpec kMarkerListFilterFields[] = {
    { static_cast<std::uint8_t>(MarkerListFilterField::MName), FilterFieldKind::Text, kMNameAliases, std::size(kMNameAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::MId), FilterFieldKind::Text, kMIdAliases, std::size(kMIdAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::MType), FilterFieldKind::Text, kMTypeAliases, std::size(kMTypeAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::MDesc), FilterFieldKind::Text, kMDescAliases, std::size(kMDescAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::MLocation), FilterFieldKind::Text, kMLocationAliases, std::size(kMLocationAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::MDistance), FilterFieldKind::Number, kMDistanceAliases, std::size(kMDistanceAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::AName), FilterFieldKind::Text, kNameAliases, std::size(kNameAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::AId), FilterFieldKind::Text, kIdAliases, std::size(kIdAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::AType), FilterFieldKind::Text, kTypeAliases, std::size(kTypeAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::AWeapon), FilterFieldKind::Text, kWeaponAliases, std::size(kWeaponAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::AAge), FilterFieldKind::Text, kAgeAliases, std::size(kAgeAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::AStatus), FilterFieldKind::Text, kStatusAliases, std::size(kStatusAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::ALocation), FilterFieldKind::Text, kLocationAliases, std::size(kLocationAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::ADistance), FilterFieldKind::Number, kDistanceAliases, std::size(kDistanceAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::ATask), FilterFieldKind::Text, kTaskAliases, std::size(kTaskAliases), true },
};

static constexpr std::uint8_t kMarkerTextShorthandFieldIds[] = {
    static_cast<std::uint8_t>(MarkerListFilterField::MName),
    static_cast<std::uint8_t>(MarkerListFilterField::MId),
    static_cast<std::uint8_t>(MarkerListFilterField::MType),
    static_cast<std::uint8_t>(MarkerListFilterField::MDesc),
    static_cast<std::uint8_t>(MarkerListFilterField::MLocation),
};

static constexpr std::uint8_t kMarkerActorTextShorthandFieldIds[] = {
    static_cast<std::uint8_t>(MarkerListFilterField::AName),
    static_cast<std::uint8_t>(MarkerListFilterField::AId),
    static_cast<std::uint8_t>(MarkerListFilterField::AType),
    static_cast<std::uint8_t>(MarkerListFilterField::AWeapon),
    static_cast<std::uint8_t>(MarkerListFilterField::AAge),
    static_cast<std::uint8_t>(MarkerListFilterField::AStatus),
    static_cast<std::uint8_t>(MarkerListFilterField::ALocation),
    static_cast<std::uint8_t>(MarkerListFilterField::ATask),
};

static constexpr FilterShorthandConfig kMarkerListFilterShorthand{
    static_cast<std::uint8_t>(MarkerListFilterField::MDistance),
    { kMarkerTextShorthandFieldIds, std::size(kMarkerTextShorthandFieldIds) },
    { kMarkerActorTextShorthandFieldIds, std::size(kMarkerActorTextShorthandFieldIds) },
};

static constexpr FilterSchema kMarkerListFilterSchema{
    kMarkerListFilterFields,
    std::size(kMarkerListFilterFields),
    kMarkerListFilterShorthand,
};

static constexpr std::uint8_t kMarkerListFilterActorFieldMin = static_cast<std::uint8_t>(MarkerListFilterField::AName);

static bool IsMarkerListFilterMarkerField(std::uint8_t fieldId) {
    return fieldId < kMarkerListFilterActorFieldMin;
}

static std::string_view MarkerFilterRowText(const void* rowContext, std::uint8_t fieldId);
static bool MarkerFilterRowBool(const void* rowContext, std::uint8_t fieldId);
static float MarkerFilterRowNumber(const void* rowContext, std::uint8_t fieldId);

static std::string_view MarkerListActorFilterRowText(const void* rowContext, std::uint8_t fieldId);
static bool MarkerListActorFilterRowBool(const void* rowContext, std::uint8_t fieldId);
static float MarkerListActorFilterRowNumber(const void* rowContext, std::uint8_t fieldId);

static const FilterRowAccess kMarkerFilterRowAccess{
    MarkerFilterRowText,
    MarkerFilterRowBool,
    MarkerFilterRowNumber,
};

static const FilterRowAccess kMarkerListActorFilterRowAccess{
    MarkerListActorFilterRowText,
    MarkerListActorFilterRowBool,
    MarkerListActorFilterRowNumber,
};

static std::string_view MarkerFilterRowText(const void* rowContext, std::uint8_t fieldId) {
    const auto& row = *static_cast<const MarkerTableRow*>(rowContext);
    switch (static_cast<MarkerListFilterField>(fieldId)) {
    case MarkerListFilterField::MName:
        return MarkerTableRowName(row);
    case MarkerListFilterField::MId:
        return MarkerTableRowIdHex(row);
    case MarkerListFilterField::MType:
        return row.type;
    case MarkerListFilterField::MDesc:
        return row.description;
    case MarkerListFilterField::MLocation:
        return row.location;
    default:
        return {};
    }
}

static bool MarkerFilterRowBool(const void* rowContext, std::uint8_t fieldId) {
    (void)rowContext;
    (void)fieldId;
    return false;
}

static float MarkerFilterRowNumber(const void* rowContext, std::uint8_t fieldId) {
    const auto& row = *static_cast<const MarkerTableRow*>(rowContext);
    switch (static_cast<MarkerListFilterField>(fieldId)) {
    case MarkerListFilterField::MDistance:
        return row.distance;
    default:
        return 0.0f;
    }
}

static std::string_view MarkerListActorFilterRowText(const void* rowContext, std::uint8_t fieldId) {
    const auto& row = *static_cast<const ActorTableRow*>(rowContext);
    switch (static_cast<MarkerListFilterField>(fieldId)) {
    case MarkerListFilterField::AName:
        return row.name;
    case MarkerListFilterField::AId:
        return ActorTableRowIdHex(row);
    case MarkerListFilterField::AType:
        return row.type;
    case MarkerListFilterField::AWeapon:
        return row.weapon;
    case MarkerListFilterField::AAge:
        return row.age;
    case MarkerListFilterField::AStatus:
        return row.status;
    case MarkerListFilterField::ALocation:
        return row.location;
    case MarkerListFilterField::ATask:
        return row.task;
    default:
        return {};
    }
}

static bool MarkerListActorFilterRowBool(const void* rowContext, std::uint8_t fieldId) {
    (void)rowContext;
    (void)fieldId;
    return false;
}

static float MarkerListActorFilterRowNumber(const void* rowContext, std::uint8_t fieldId) {
    const auto& row = *static_cast<const ActorTableRow*>(rowContext);
    switch (static_cast<MarkerListFilterField>(fieldId)) {
    case MarkerListFilterField::ADistance:
        return row.distance;
    default:
        return 0.0f;
    }
}

static bool MarkerShorthandGroupHasNegatedWord(const FilterShorthandGroup& group) {
    for (std::size_t w = 0; w < group.wordCount; ++w) {
        if (group.words[w].negated) {
            return true;
        }
    }
    return false;
}

static bool MarkerShorthandSingleWordSatisfiesPositive(const FilterShorthandWord& word, const MarkerTableRow& markerRow,
    const RevLinks* links, ActorTableRow& actorRow, std::string* actorTaskBuf, bool fillActorExpensive) {
    FilterShorthandGroup single = {};
    single.words[0] = word;
    single.wordCount = 1;

    if (FilterMatchesShorthandTextWords(single, &markerRow, kMarkerListFilterSchema.shorthand.primaryText,
            kMarkerListFilterSchema, kMarkerFilterRowAccess)) {
        return true;
    }

    if (!links || links->count == 0) {
        return false;
    }

    for (std::uint16_t linkIndex = 0; linkIndex < links->count; ++linkIndex) {
        RE::TESObjectREFR* actorRef = links->refs[linkIndex];
        RE::Actor* actor = actorRef ? actorRef->As<RE::Actor>() : nullptr;
        if (!actor) {
            continue;
        }

        PopulateActorTableRow(actor, fillActorExpensive ? actorTaskBuf : nullptr, actorRow);
        if (FilterMatchesShorthandTextWords(single, &actorRow, kMarkerListFilterSchema.shorthand.secondaryText,
                kMarkerListFilterSchema, kMarkerListActorFilterRowAccess)) {
            return true;
        }
    }

    return false;
}

static bool MarkerShorthandSingleWordSatisfiesNegated(const FilterShorthandWord& word, const MarkerTableRow& markerRow,
    const RevLinks* links, ActorTableRow& actorRow, std::string* actorTaskBuf, bool fillActorExpensive) {
    FilterShorthandGroup single = {};
    single.words[0] = word;
    single.wordCount = 1;

    if (!FilterMatchesShorthandTextWords(single, &markerRow, kMarkerListFilterSchema.shorthand.primaryText,
            kMarkerListFilterSchema, kMarkerFilterRowAccess)) {
        return false;
    }

    if (!links || links->count == 0) {
        return true;
    }

    for (std::uint16_t linkIndex = 0; linkIndex < links->count; ++linkIndex) {
        RE::TESObjectREFR* actorRef = links->refs[linkIndex];
        RE::Actor* actor = actorRef ? actorRef->As<RE::Actor>() : nullptr;
        if (!actor) {
            continue;
        }

        PopulateActorTableRow(actor, fillActorExpensive ? actorTaskBuf : nullptr, actorRow);
        if (!FilterMatchesShorthandTextWords(single, &actorRow, kMarkerListFilterSchema.shorthand.secondaryText,
                kMarkerListFilterSchema, kMarkerListActorFilterRowAccess)) {
            return false;
        }
    }

    return true;
}

static bool MarkerFilterMatchesShorthandGroupWithNegation(const FilterShorthandGroup& group, const MarkerTableRow& markerRow,
    const RevLinks* links, ActorTableRow& actorRow, std::string* actorTaskBuf, bool fillActorExpensive) {
    for (std::size_t w = 0; w < group.wordCount; ++w) {
        const FilterShorthandWord& word = group.words[w];
        if (word.negated) {
            if (!MarkerShorthandSingleWordSatisfiesNegated(word, markerRow, links, actorRow, actorTaskBuf, fillActorExpensive)) {
                return false;
            }
        } else if (!MarkerShorthandSingleWordSatisfiesPositive(word, markerRow, links, actorRow, actorTaskBuf,
                       fillActorExpensive)) {
            return false;
        }
    }
    return true;
}

static bool MarkerFilterAndGroupMatches(const FilterAndGroup& group, const MarkerTableRow& markerRow, const RevLinks* links,
    ActorTableRow& actorRow, std::string* actorTaskBuf, bool fillActorExpensive) {
    bool hasPositiveActorPredicate = false;
    bool hasNegativeActorPredicate = false;

    for (std::size_t i = 0; i < group.predicateCount; ++i) {
        const FilterPredicate& pred = group.predicates[i];
        if (IsMarkerListFilterMarkerField(pred.field)) {
            if (!FilterMatchPredicate(pred, &markerRow, kMarkerListFilterSchema, kMarkerFilterRowAccess)) {
                return false;
            }
        } else if (pred.negated) {
            hasNegativeActorPredicate = true;
        } else {
            hasPositiveActorPredicate = true;
        }
    }

    if (!hasPositiveActorPredicate && !hasNegativeActorPredicate) {
        return true;
    }

    if (hasNegativeActorPredicate && links && links->count > 0) {
        for (std::uint16_t linkIndex = 0; linkIndex < links->count; ++linkIndex) {
            RE::TESObjectREFR* actorRef = links->refs[linkIndex];
            RE::Actor* actor = actorRef ? actorRef->As<RE::Actor>() : nullptr;
            if (!actor) {
                continue;
            }

            PopulateActorTableRow(actor, fillActorExpensive ? actorTaskBuf : nullptr, actorRow);

            for (std::size_t i = 0; i < group.predicateCount; ++i) {
                const FilterPredicate& pred = group.predicates[i];
                if (IsMarkerListFilterMarkerField(pred.field) || !pred.negated) {
                    continue;
                }
                if (!FilterMatchPredicate(pred, &actorRow, kMarkerListFilterSchema, kMarkerListActorFilterRowAccess)) {
                    return false;
                }
            }
        }
    }

    if (!hasPositiveActorPredicate) {
        return true;
    }

    if (!links || links->count == 0) {
        return false;
    }

    for (std::uint16_t linkIndex = 0; linkIndex < links->count; ++linkIndex) {
        RE::TESObjectREFR* actorRef = links->refs[linkIndex];
        RE::Actor* actor = actorRef ? actorRef->As<RE::Actor>() : nullptr;
        if (!actor) {
            continue;
        }

        PopulateActorTableRow(actor, fillActorExpensive ? actorTaskBuf : nullptr, actorRow);

        bool actorMatchesAllPositive = true;
        for (std::size_t i = 0; i < group.predicateCount; ++i) {
            const FilterPredicate& pred = group.predicates[i];
            if (IsMarkerListFilterMarkerField(pred.field) || pred.negated) {
                continue;
            }
            if (!FilterMatchPredicate(pred, &actorRow, kMarkerListFilterSchema, kMarkerListActorFilterRowAccess)) {
                actorMatchesAllPositive = false;
                break;
            }
        }

        if (actorMatchesAllPositive) {
            return true;
        }
    }

    return false;
}

static bool MarkerFilterMatchesShorthandGroup(const FilterShorthandGroup& group, const MarkerTableRow& markerRow,
    const RevLinks* links, ActorTableRow& actorRow, std::string* actorTaskBuf, bool fillActorExpensive) {
    if (!FilterMatchesShorthandLessBound(group, &markerRow, kMarkerListFilterSchema, kMarkerFilterRowAccess)) {
        return false;
    }

    if (MarkerShorthandGroupHasNegatedWord(group)) {
        return MarkerFilterMatchesShorthandGroupWithNegation(group, markerRow, links, actorRow, actorTaskBuf,
            fillActorExpensive);
    }

    if (FilterMatchesShorthandTextWords(group, &markerRow, kMarkerListFilterSchema.shorthand.primaryText,
            kMarkerListFilterSchema, kMarkerFilterRowAccess)) {
        return true;
    }

    if (!links || links->count == 0) {
        return false;
    }

    for (std::uint16_t linkIndex = 0; linkIndex < links->count; ++linkIndex) {
        RE::TESObjectREFR* actorRef = links->refs[linkIndex];
        RE::Actor* actor = actorRef ? actorRef->As<RE::Actor>() : nullptr;
        if (!actor) {
            continue;
        }

        PopulateActorTableRow(actor, fillActorExpensive ? actorTaskBuf : nullptr, actorRow);
        if (FilterMatchesShorthandTextWords(group, &actorRow, kMarkerListFilterSchema.shorthand.secondaryText,
                kMarkerListFilterSchema, kMarkerListActorFilterRowAccess)) {
            return true;
        }
    }

    return false;
}

static bool MarkerFilterMatchesShorthandText(const FilterParseResult& expr, const MarkerTableRow& markerRow, const RevLinks* links,
    ActorTableRow& actorRow, std::string* actorTaskBuf, bool fillActorExpensive) {
    for (std::size_t g = 0; g < expr.shorthandGroupCount; ++g) {
        if (MarkerFilterMatchesShorthandGroup(expr.shorthandGroups[g], markerRow, links, actorRow, actorTaskBuf,
                fillActorExpensive)) {
            return true;
        }
    }

    return false;
}

static bool MarkerFilterMatchesExpression(const FilterParseResult& expr, const MarkerTableRow& markerRow, const RevLinks* links,
    ActorTableRow& actorRow, std::string* actorTaskBuf, bool fillActorExpensive) {
    if (!expr.hasExpression) {
        return true;
    }

    if (expr.shorthand == FilterShorthandKind::TextWords) {
        return MarkerFilterMatchesShorthandText(expr, markerRow, links, actorRow, actorTaskBuf, fillActorExpensive);
    }

    for (std::size_t g = 0; g < expr.andGroupCount; ++g) {
        if (MarkerFilterAndGroupMatches(expr.andGroups[g], markerRow, links, actorRow, actorTaskBuf, fillActorExpensive)) {
            return true;
        }
    }

    return false;
}

static const char* ActorTableRowTextOrEmpty(std::string_view text) {
    return text.empty() ? "" : text.data();
}

static void RenderActorTableRow(const ActorTableRow& row, RE::Actor* actor) {
    ImGuiMCP::TableNextRow();
    ImGuiMCP::TableSetColumnIndex(0);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.name));
    ImGuiMCP::TableSetColumnIndex(1);
    ImGuiMCP::Text("%08X", actor ? actor->GetFormID() : 0u);
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
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.task));
}

static void RenderLinkedActors(const RevLinks* links) {
    if (!links || links->count == 0) {
        ImGuiMCP::TextDisabled("No linked actors");
        return;
    }

    static std::string taskBuffer = [] {
        std::string s;
        s.reserve(128);
        return s;
    }();

    constexpr ImGuiMCP::ImGuiTableFlags tableFlags = ImGuiMCP::ImGuiTableFlags_Resizable |
                                                     ImGuiMCP::ImGuiTableFlags_RowBg |
                                                     ImGuiMCP::ImGuiTableFlags_SizingFixedFit;
    if (ImGuiMCP::BeginTable("LinkedActors", 9, tableFlags)) {
        ImGuiMCP::TableSetupColumn("Name");
        ImGuiMCP::TableSetupColumn("ID");
        ImGuiMCP::TableSetupColumn("Type");
        ImGuiMCP::TableSetupColumn("Weapon");
        ImGuiMCP::TableSetupColumn("Age");
        ImGuiMCP::TableSetupColumn("Status");
        ImGuiMCP::TableSetupColumn("Location");
        ImGuiMCP::TableSetupColumn("Distance");
        ImGuiMCP::TableSetupColumn("Task", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
        ImGuiMCP::TableHeadersRow();

        for (std::uint16_t i = 0; i < links->count; ++i) {
            RE::TESObjectREFR* actorRef = links->refs[i];
            RE::Actor* actor = actorRef ? actorRef->As<RE::Actor>() : nullptr;
            if (!actor) {
                continue;
            }

            ActorTableRow row = {};
            PopulateActorTableRow(actor, &taskBuffer, row);
            RenderActorTableRow(row, actor);
        }

        ImGuiMCP::EndTable();
    }
}

enum class MarkerHeaderOpenForce : std::int8_t {
    None = 0,
    Expand,
    Collapse,
};

static bool MarkerHasLinkedChildren(const RevLinks* links) {
    return links && links->count > 0;
}

static char markerRenameBuffer[128] = {};
static std::string markerRenameOldName = [] {
    std::string s;
    s.reserve(128);
    return s;
}();
static bool markerRenamePopupRequested = false;

static bool MarkerNameHasNonWhitespace(const char* text) {
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

static void RequestMarkerRename(std::string_view markerName) {
    const char* src = markerName.empty() ? "" : markerName.data();
    strncpy_s(markerRenameBuffer, sizeof(markerRenameBuffer), src, _TRUNCATE);
    markerRenameOldName.assign(src);
    markerRenamePopupRequested = true;
}

static void CommitMarkerRename(RE::TESQuest* markerControllerScript) {
    if (!MarkerNameHasNonWhitespace(markerRenameBuffer)) {
        return;
    }
    if (markerRenameOldName == markerRenameBuffer) {
        return;
    }
    Papyrus::Call(
        markerControllerScript,
        "BRSSMarkerControllerScript",
        "Rename",
        [](RE::BSScript::Variable) {},
        std::string(markerRenameOldName),
        std::string(markerRenameBuffer)
    );
}

static void RenderMarkerRenamePopup(RE::TESQuest* markerControllerScript) {
    if (markerRenamePopupRequested) {
        ImGuiMCP::OpenPopup("Rename marker");
        markerRenamePopupRequested = false;
    }

    constexpr ImGuiMCP::ImGuiWindowFlags popupFlags = ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize;
    if (!ImGuiMCP::BeginPopupModal("Rename marker", nullptr, popupFlags)) {
        return;
    }

    ImGuiMCP::SetNextItemWidth(320.0f);
    const bool nameEdited = ImGuiMCP::InputText(
        "##MarkerRenameName",
        markerRenameBuffer,
        sizeof(markerRenameBuffer),
        ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGuiMCP::IsWindowAppearing()) {
        ImGuiMCP::SetKeyboardFocusHere(-1);
    }
    const bool nameEnter = nameEdited &&
        (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter, false) ||
            ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_KeypadEnter, false));

    bool closePopup = false;
    if (ImGuiMCP::Button("OK") || nameEnter) {
        if (MarkerNameHasNonWhitespace(markerRenameBuffer)) {
            CommitMarkerRename(markerControllerScript);
            closePopup = true;
        }
    }
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Cancel")) {
        closePopup = true;
    }

    if (closePopup) {
        ImGuiMCP::CloseCurrentPopup();
    }

    ImGuiMCP::EndPopup();
}

struct MarkerGridConfig {
    std::string name;
    std::int32_t width = 2;
    std::vector<std::int32_t> rotations;
    std::int32_t offX = 128;
    std::int32_t offY = 384;
    RE::TESForm* form = nullptr;
};

static char markerGridNameBuffer[128] = {};
static bool markerGridPopupRequested = false;
static bool markerGridFormSelectorPending = false;
static RE::TESForm* markerGridSelectedForm = nullptr;
static std::int32_t markerGridWidth = 2;
static std::int32_t markerGridRows = 2;
static constexpr std::int32_t kMarkerGridDefaultOffX = 128;
static constexpr std::int32_t kMarkerGridDefaultOffY = 384;
static std::int32_t markerGridOffX = kMarkerGridDefaultOffX;
static std::int32_t markerGridOffY = kMarkerGridDefaultOffY;
static std::vector<std::int32_t> markerGridRotations(4, 0);

static void SyncMarkerGridRotations() {
    markerGridWidth = std::max(1, markerGridWidth);
    markerGridRows = std::max(1, markerGridRows);
    const std::size_t count =
        static_cast<std::size_t>(markerGridWidth) * static_cast<std::size_t>(markerGridRows);
    if (markerGridRotations.size() < count) {
        markerGridRotations.resize(count, 0);
    } else if (markerGridRotations.size() > count) {
        markerGridRotations.resize(count);
    }
}

static void RequestMarkerGridPopup(const char* name, RE::TESForm* form = nullptr) {
    strncpy_s(markerGridNameBuffer, sizeof(markerGridNameBuffer), name ? name : "", _TRUNCATE);
    markerGridWidth = 2;
    markerGridRows = 2;
    markerGridOffX = kMarkerGridDefaultOffX;
    markerGridOffY = kMarkerGridDefaultOffY;
    markerGridRotations.assign(4, 0);
    markerGridSelectedForm = form;
    markerGridPopupRequested = true;
}

static bool IsMarkerGridConfigValid(
    const char* name, std::int32_t width, std::int32_t rows, std::int32_t offX, std::int32_t offY) {
    if (!MarkerNameHasNonWhitespace(name)) {
        return false;
    }
    if (width < 1 || rows < 1) {
        return false;
    }
    if (offX < 1 || offY < 1) {
        return false;
    }
    return true;
}

static void CycleMarkerGridRotation(std::int32_t& rotation) {
    rotation += 45;
    if (rotation >= 360) {
        rotation = 0;
    }
}

static void CycleMarkerGridRowRotation(std::int32_t width, std::int32_t row, std::vector<std::int32_t>& rotations) {
    const std::int32_t rowStart = row * width;
    for (std::int32_t col = 0; col < width; ++col) {
        CycleMarkerGridRotation(rotations[static_cast<std::size_t>(rowStart + col)]);
    }
}

static void CycleMarkerGridColumnRotation(
    std::int32_t width, std::int32_t rows, std::int32_t col, std::vector<std::int32_t>& rotations) {
    for (std::int32_t row = 0; row < rows; ++row) {
        CycleMarkerGridRotation(rotations[static_cast<std::size_t>(row * width + col)]);
    }
}

static void ResetMarkerGridRotations(std::vector<std::int32_t>& rotations) {
    for (std::int32_t& rotation : rotations) {
        rotation = 0;
    }
}

static void DrawMarkerGridDirectionArrow(ImGuiMCP::ImDrawList* drawList, ImGuiMCP::ImVec2 center, float radius,
    int rotationDeg, ImGuiMCP::ImU32 color) {
    constexpr float kPi = 3.14159265f;
    const float rad = (static_cast<float>(rotationDeg) - 90.0f) * kPi / 180.0f;
    const ImGuiMCP::ImVec2 tip{
        center.x + std::cos(rad) * radius,
        center.y + std::sin(rad) * radius,
    };
    const float backRad1 = rad + 2.4f;
    const float backRad2 = rad - 2.4f;
    const float tail = radius * 0.55f;
    const ImGuiMCP::ImVec2 tail1{
        center.x + std::cos(backRad1) * tail,
        center.y + std::sin(backRad1) * tail,
    };
    const ImGuiMCP::ImVec2 tail2{
        center.x + std::cos(backRad2) * tail,
        center.y + std::sin(backRad2) * tail,
    };
    ImGuiMCP::ImDrawListManager::AddTriangleFilled(drawList, tip, tail1, tail2, color);
    ImGuiMCP::ImDrawListManager::AddCircleFilled(drawList, center, radius * 0.15f, color, 12);
}

static void OnMarkerGridConfirmed(RE::TESQuest* markerControllerScript, MarkerGridConfig config) {
    Papyrus::Call(
        markerControllerScript,
        "BRSSMarkerControllerScript",
        "CreateGrid",
        [](RE::BSScript::Variable) {},
        std::move(config.name),
        std::move(config.rotations),
        config.width,
        static_cast<RE::TESForm*>(config.form),
        config.offX,
        config.offY
    );
}

static void RenderMarkerGridLabeledInputInt(float baseX, float baseY, const char* label, const char* id, int* value,
    float labelWidth, float inputWidth, int step, int stepFast) {
    ImGuiMCP::SetCursorPos(ImGuiMCP::ImVec2{ baseX, baseY });
    ImGuiMCP::AlignTextToFramePadding();
    ImGuiMCP::TextUnformatted(label);
    ImGuiMCP::SameLine(baseX + labelWidth);
    ImGuiMCP::SetNextItemWidth(inputWidth);
    ImGuiMCP::InputInt(id, value, step, stepFast);
}

static void RenderMarkerGridOffsetInputInt(float baseX, float baseY, const char* label, const char* inputId,
    const char* defaultBtnId, int* value, int defaultValue, float labelWidth, float inputWidth, int step, int stepFast) {
    const float defaultBtnSize = ImGuiMCP::GetFrameHeight();
    const float fieldInputW = inputWidth - defaultBtnSize - 4.0f;

    ImGuiMCP::SetCursorPos(ImGuiMCP::ImVec2{ baseX, baseY });
    ImGuiMCP::AlignTextToFramePadding();
    ImGuiMCP::TextUnformatted(label);
    ImGuiMCP::SameLine(baseX + labelWidth);
    ImGuiMCP::SetNextItemWidth(fieldInputW);
    ImGuiMCP::InputInt(inputId, value, step, stepFast);
    ImGuiMCP::SameLine(0.0f, 4.0f);
    if (ImGuiMCP::Button(defaultBtnId, ImGuiMCP::ImVec2{ defaultBtnSize, defaultBtnSize })) {
        *value = defaultValue;
    }
}

static void RenderMarkerGridPopup(char* newMarkerNameBuffer, RE::TESQuest* markerControllerScript) {
    constexpr float kPopupW = 960.0f;
    constexpr float kPopupH = 870.0f;
    constexpr float kMinPopupW = 780.0f;
    constexpr float kMinPopupH = 630.0f;
    constexpr float kArrowRadiusPx = 22.0f;
    constexpr float kArrowHitPadPx = 10.0f;
    constexpr float kRowStartX = 12.0f;
    constexpr float kLabelWidth = 80.0f;
    constexpr float kInputWidth = 200.0f;
    constexpr float kColumnGap = 24.0f;
    constexpr float kSecondPairX = kRowStartX + kLabelWidth + kInputWidth + kColumnGap;
    constexpr float kGridPad = 6.0f;
    constexpr float kMinPreviewH = 180.0f;

    if (markerGridPopupRequested) {
        ImGuiMCP::SetNextWindowSize(ImGuiMCP::ImVec2{ kPopupW, kPopupH }, ImGuiMCP::ImGuiCond_Appearing);
        ImGuiMCP::SetNextWindowSizeConstraints(
            ImGuiMCP::ImVec2{ kMinPopupW, kMinPopupH }, ImGuiMCP::ImVec2{ FLT_MAX, FLT_MAX });
        ImGuiMCP::OpenPopup("New marker grid");
        markerGridPopupRequested = false;
    }

    if (!ImGuiMCP::BeginPopupModal("New marker grid", nullptr, 0)) {
        return;
    }

    SyncMarkerGridRotations();

    ImGuiMCP::Text("Name: %s", markerGridNameBuffer);
    ImGuiMCP::Spacing();

    const float dimRowY = ImGuiMCP::GetCursorPosY();
    const float rowStep = ImGuiMCP::GetFrameHeightWithSpacing();
    RenderMarkerGridLabeledInputInt(
        kRowStartX, dimRowY, "Columns", "##MarkerGridCols", &markerGridWidth, kLabelWidth, kInputWidth, 1, 1);
    RenderMarkerGridLabeledInputInt(
        kSecondPairX, dimRowY, "Rows", "##MarkerGridRows", &markerGridRows, kLabelWidth, kInputWidth, 1, 1);
    SyncMarkerGridRotations();

    const float offsetRowY = dimRowY + rowStep;
    RenderMarkerGridOffsetInputInt(kRowStartX, offsetRowY, "Offset X", "##MarkerGridOffX", "D##MarkerGridOffXDefault",
        &markerGridOffX, kMarkerGridDefaultOffX, kLabelWidth, kInputWidth, 8, 64);
    RenderMarkerGridOffsetInputInt(kSecondPairX, offsetRowY, "Offset Y", "##MarkerGridOffY", "D##MarkerGridOffYDefault",
        &markerGridOffY, kMarkerGridDefaultOffY, kLabelWidth, kInputWidth, 8, 64);
    ImGuiMCP::SetCursorPosY(offsetRowY + rowStep);
    ImGuiMCP::TextDisabled("Ctrl+click: row  |  Alt+click: column  |  Ctrl+R: reset rotations");

    ImGuiMCP::Spacing();

    ImGuiMCP::ImVec2 previewRegionAvail{};
    ImGuiMCP::GetContentRegionAvail(&previewRegionAvail);
    const float previewChildH = std::max(kMinPreviewH, previewRegionAvail.y);

    if (ImGuiMCP::BeginChild(
            "##MarkerGridPreview", ImGuiMCP::ImVec2{ -1.0f, previewChildH }, ImGuiMCP::ImGuiChildFlags_Border)) {
        ImGuiMCP::ImVec2 canvasAvail{};
        ImGuiMCP::GetContentRegionAvail(&canvasAvail);
        const float canvasW = std::max(1.0f, canvasAvail.x);
        const float canvasH = std::max(1.0f, canvasAvail.y);
        constexpr float kPixelPerUnit = 0.45f;
        const float edgeMargin = kArrowRadiusPx + 6.0f;
        const float usableW = std::max(1.0f, canvasW - kGridPad * 2.0f - edgeMargin * 2.0f);
        const float usableH = std::max(1.0f, canvasH - kGridPad * 2.0f - edgeMargin * 2.0f);

        float spacingX = static_cast<float>(markerGridOffX) * kPixelPerUnit;
        float spacingY = static_cast<float>(markerGridOffY) * kPixelPerUnit;
        float drawW = markerGridWidth > 1 ? static_cast<float>(markerGridWidth - 1) * spacingX : 0.0f;
        float drawH = markerGridRows > 1 ? static_cast<float>(markerGridRows - 1) * spacingY : 0.0f;

        float overflowScale = 1.0f;
        if (drawW > usableW) {
            overflowScale = std::min(overflowScale, usableW / drawW);
        }
        if (drawH > usableH) {
            overflowScale = std::min(overflowScale, usableH / drawH);
        }
        spacingX *= overflowScale;
        spacingY *= overflowScale;
        drawW *= overflowScale;
        drawH *= overflowScale;

        ImGuiMCP::InvisibleButton("##MarkerGridCanvas", ImGuiMCP::ImVec2{ canvasW, canvasH });

        ImGuiMCP::ImVec2 canvasMin{};
        ImGuiMCP::GetItemRectMin(&canvasMin);
        const float firstMarkerX = canvasMin.x + kGridPad + edgeMargin + (usableW - drawW) * 0.5f;
        const float firstMarkerY = canvasMin.y + kGridPad + edgeMargin + (usableH - drawH) * 0.5f;

        const auto markerCenter = [&](int col, int row) {
            return ImGuiMCP::ImVec2{
                firstMarkerX + static_cast<float>(col) * spacingX,
                firstMarkerY + static_cast<float>(markerGridRows - 1 - row) * spacingY,
            };
        };

        if (ImGuiMCP::IsItemClicked()) {
            ImGuiMCP::ImGuiIO* clickIo = ImGuiMCP::GetIO();
            const bool ctrlHeld = clickIo && (clickIo->KeyCtrl || clickIo->KeySuper);
            const bool altHeld = clickIo && clickIo->KeyAlt;

            ImGuiMCP::ImVec2 mousePos{};
            ImGuiMCP::GetMousePos(&mousePos);
            const float hitRadius = kArrowRadiusPx + kArrowHitPadPx;
            const float hitRadiusSq = hitRadius * hitRadius;
            int bestIndex = -1;
            float bestDistSq = hitRadiusSq;
            for (int row = 0; row < markerGridRows; ++row) {
                for (int col = 0; col < markerGridWidth; ++col) {
                    const ImGuiMCP::ImVec2 center = markerCenter(col, row);
                    const float dx = mousePos.x - center.x;
                    const float dy = mousePos.y - center.y;
                    const float distSq = dx * dx + dy * dy;
                    if (distSq <= bestDistSq) {
                        bestDistSq = distSq;
                        bestIndex = row * markerGridWidth + col;
                    }
                }
            }
            if (bestIndex >= 0) {
                const int col = bestIndex % markerGridWidth;
                const int row = bestIndex / markerGridWidth;
                if (altHeld && !ctrlHeld) {
                    CycleMarkerGridColumnRotation(markerGridWidth, markerGridRows, col, markerGridRotations);
                } else if (ctrlHeld && !altHeld) {
                    CycleMarkerGridRowRotation(markerGridWidth, row, markerGridRotations);
                } else {
                    CycleMarkerGridRotation(markerGridRotations[static_cast<std::size_t>(bestIndex)]);
                }
            }
        }

        ImGuiMCP::ImDrawList* drawList = ImGuiMCP::GetWindowDrawList();
        const ImGuiMCP::ImU32 arrowCol = IM_COL32(120, 200, 255, 255);
        const ImGuiMCP::ImU32 indexCol = IM_COL32(180, 180, 180, 200);

        for (int row = 0; row < markerGridRows; ++row) {
            for (int col = 0; col < markerGridWidth; ++col) {
                const int index = row * markerGridWidth + col;
                const ImGuiMCP::ImVec2 center = markerCenter(col, row);
                DrawMarkerGridDirectionArrow(
                    drawList, center, kArrowRadiusPx, markerGridRotations[static_cast<std::size_t>(index)], arrowCol);

                char indexLabel[16] = {};
                std::snprintf(indexLabel, sizeof(indexLabel), "%d", index);
                ImGuiMCP::ImDrawListManager::AddText(
                    drawList,
                    ImGuiMCP::ImVec2{ center.x + kArrowRadiusPx + 4.0f, center.y - kArrowRadiusPx },
                    indexCol,
                    indexLabel);
            }
        }

        ImGuiMCP::EndChild();
    }

    ImGuiMCP::ImGuiIO* io = ImGuiMCP::GetIO();
    const bool ctrlShortcut = io && io->KeyCtrl && !io->KeyAlt && !io->KeySuper;
    const bool cancelShortcut = ctrlShortcut && ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Q, false);
    const bool resetRotationsShortcut = ctrlShortcut && ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_R, false);
    const bool confirmPressed =
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter, false) ||
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_KeypadEnter, false);

    if (resetRotationsShortcut) {
        ResetMarkerGridRotations(markerGridRotations);
    }

    bool closePopup = false;
    if (cancelShortcut) {
        closePopup = true;
    } else if (confirmPressed &&
        IsMarkerGridConfigValid(
            markerGridNameBuffer, markerGridWidth, markerGridRows, markerGridOffX, markerGridOffY)) {
        MarkerGridConfig config;
        config.name = markerGridNameBuffer;
        config.width = markerGridWidth;
        config.rotations = markerGridRotations;
        config.offX = markerGridOffX;
        config.offY = markerGridOffY;
        config.form = markerGridSelectedForm;
        OnMarkerGridConfirmed(markerControllerScript, std::move(config));
        markerGridSelectedForm = nullptr;
        if (newMarkerNameBuffer) {
            newMarkerNameBuffer[0] = '\0';
        }
        closePopup = true;
    }

    if (closePopup) {
        markerGridSelectedForm = nullptr;
        ImGuiMCP::CloseCurrentPopup();
    }

    ImGuiMCP::EndPopup();
}

static void CollapsedHeader(const char* label, const void* uniqueId, std::string_view markerName,
    RE::TESQuest* markerControllerScript, const RevLinks* links, MarkerHeaderOpenForce openForce) {
    ImGuiMCP::PushID(uniqueId);

    const ImGuiMCP::ImGuiStyle* style = ImGuiMCP::GetStyle();
    const float itemSpacing = style->ItemSpacing.x;
    const float actionButtonSize = ImGuiMCP::GetFrameHeight();
    const ImGuiMCP::ImVec2 actionButtonSizeVec{ actionButtonSize, actionButtonSize };
    const float actionButtonsWidth = actionButtonSize * 2.0f + itemSpacing + style->CellPadding.x * 2.0f;

    if (openForce == MarkerHeaderOpenForce::Expand) {
        ImGuiMCP::SetNextItemOpen(true, ImGuiMCP::ImGuiCond_Always);
    } else if (openForce == MarkerHeaderOpenForce::Collapse) {
        ImGuiMCP::SetNextItemOpen(false, ImGuiMCP::ImGuiCond_Always);
    }

    constexpr ImGuiMCP::ImGuiTableFlags headerTableFlags = ImGuiMCP::ImGuiTableFlags_SizingFixedFit |
                                                          ImGuiMCP::ImGuiTableFlags_NoBordersInBody |
                                                          ImGuiMCP::ImGuiTableFlags_NoPadOuterX;
    bool headerOpen = false;
    if (ImGuiMCP::BeginTable("##MarkerHeaderRow", 2, headerTableFlags)) {
        ImGuiMCP::TableSetupColumn("##MarkerLabel", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
        ImGuiMCP::TableSetupColumn("##MarkerActions", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, actionButtonsWidth);
        ImGuiMCP::TableNextRow();

        ImGuiMCP::TableSetColumnIndex(0);
        headerOpen = ImGuiMCP::CollapsingHeader(label);

        ImGuiMCP::TableSetColumnIndex(1);
        if (ImGuiMCP::Button("R", actionButtonSizeVec)) {
            RequestMarkerRename(markerName);
        }
        ImGuiMCP::SameLine();
        const bool hasLinkedChildren = MarkerHasLinkedChildren(links);
        if (hasLinkedChildren) {
            ImGuiMCP::BeginDisabled();
        }
        if (ImGuiMCP::Button("X", actionButtonSizeVec) && !hasLinkedChildren) {
            Papyrus::Call(
                markerControllerScript,
                "BRSSMarkerControllerScript",
                "Remove",
                [](RE::BSScript::Variable) {},
                std::string(markerName)
            );
        }
        if (hasLinkedChildren) {
            ImGuiMCP::EndDisabled();
        }

        ImGuiMCP::EndTable();
    }

    if (headerOpen) {
        ImGuiMCP::Indent();
        RenderLinkedActors(links);
        ImGuiMCP::Unindent();
    }
    ImGuiMCP::PopID();
}

void __stdcall UI::MarkerListView::Render() {
    static char filterBuffer[256] = {};
    static char lastTokenizedFilter[256] = {};
    static FilterTokenizeResult tokenizeResult = {};
    static FilterParseResult parseResult = {};
    static bool filterUsesExpensiveField = false;
    static std::string header = [] {
        std::string s;
        s.reserve(128);
        return s;
    }();
    static std::string filterActorTaskBuffer = [] {
        std::string s;
        s.reserve(128);
        return s;
    }();
    static char newMarkerNameBuffer[128] = {};

    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return;
    }

    JC::ObjectId actorDb = JC::GetActorDb();
    if (actorDb == 0) {
        ImGuiMCP::Text("No actor database found");
        return;
    }

    JC::ObjectId markerDb = JC::GetMarkerDb();
    if (markerDb == 0) {
        ImGuiMCP::Text("No marker database found");
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
    ImGuiMCP::InputTextWithHint("##MarkerListFilter", "Filter...", filterBuffer, sizeof(filterBuffer));
    ImGuiMCP::TextDisabled(
        "Ctrl+L: focus filter  |  Ctrl+D: distance toggle  |  Ctrl+M: focus marker name  |  e.g. todo");
    if (std::strcmp(filterBuffer, lastTokenizedFilter) != 0) {
        strncpy_s(lastTokenizedFilter, filterBuffer, sizeof(lastTokenizedFilter));
        lastTokenizedFilter[sizeof(lastTokenizedFilter) - 1] = '\0';
        FilterTokenize(filterBuffer, tokenizeResult);
        FilterParseExpression(filterBuffer, tokenizeResult, parseResult, kMarkerListFilterSchema);
        filterUsesExpensiveField = FilterExpressionUsesExpensiveField(parseResult, kMarkerListFilterSchema);
    }
    if (!parseResult.ok && parseResult.error[0] != '\0') {
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", parseResult.error);
    }
    ImGuiMCP::Spacing();

    MarkerHeaderOpenForce headerOpenForce = MarkerHeaderOpenForce::None;
    if (ImGuiMCP::Button("Expand all")) {
        headerOpenForce = MarkerHeaderOpenForce::Expand;
    }
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Collapse all")) {
        headerOpenForce = MarkerHeaderOpenForce::Collapse;
    }
    ImGuiMCP::Spacing();

    const bool applyFilter = parseResult.ok && parseResult.hasExpression;
    const bool fillActorExpensive = applyFilter && filterUsesExpensiveField;

    RE::TESDataHandler* dh = RE::TESDataHandler::GetSingleton();
    if (!dh) {
        return;
    }

    static auto* BRSS_PackageKeyword1 = dh->LookupForm<RE::BGSKeyword>(0xAA0F, "SkyrimSlavery.esp");
    static auto* BRSS_PackageKeyword2 = dh->LookupForm<RE::BGSKeyword>(0xAA10, "SkyrimSlavery.esp");
    static auto* BRSS_MarkerControllerScript = dh->LookupForm<RE::TESQuest>(0x00047627, "SkyrimSlavery.esp");
    if (!BRSS_PackageKeyword1 || !BRSS_PackageKeyword2 || !BRSS_MarkerControllerScript) {
        return;
    }

    alignas(std::max_align_t) static std::array<std::byte, 8 * 1024 * 1024> revLinkBuffer;
    std::pmr::monotonic_buffer_resource revLinkPool{
        revLinkBuffer.data(),
        revLinkBuffer.size(),
        std::pmr::null_memory_resource()
    };
    std::pmr::unordered_map<RE::TESObjectREFR*, RevLinks> revLink{ &revLinkPool };
    revLink.reserve(4096);

    auto addRevLink = [&](RE::TESObjectREFR* marker, RE::TESObjectREFR* actor) {
        auto& links = revLink[marker];
        if (links.count < links.refs.size()) {
            links.refs[links.count++] = actor;
        }
    };

    RE::TESForm* currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, nullptr, nullptr);
    while (currentForm) {
        RE::Actor* currentActor = currentForm->As<RE::Actor>();
        if (currentActor) {
            RE::TESObjectREFR* link1 = currentActor->GetLinkedRef(BRSS_PackageKeyword1);
            RE::TESObjectREFR* link2 = currentActor->GetLinkedRef(BRSS_PackageKeyword2);

            if (link1) {
                addRevLink(link1, currentActor);
            }

            if (link2 && link2 != link1) {
                addRevLink(link2, currentActor);
            }
        }

        currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
    }

    ActorTableRow filterActorRow = {};

    const float bottomPanelHeight = ImGuiMCP::GetFrameHeightWithSpacing() * 2.0f + ImGuiMCP::GetStyle()->ItemSpacing.y * 2.0f;
    ImGuiMCP::ImVec2 listAvail{};
    ImGuiMCP::GetContentRegionAvail(&listAvail);
    const float listHeight = std::max(0.0f, listAvail.y - bottomPanelHeight);

    if (ImGuiMCP::BeginChild("##MarkerListScroll", ImGuiMCP::ImVec2{0.0f, listHeight}, 0)) {
        currentForm = JC::jFormMapNextKey(JC::Domain, markerDb, nullptr, nullptr);
        while (currentForm) {
            RE::TESObjectREFR* currentMarker = currentForm->As<RE::TESObjectREFR>();
            if (!currentMarker) {
                currentForm = JC::jFormMapNextKey(JC::Domain, markerDb, currentForm, nullptr);
                continue;
            }

            const float markerDistance = currentMarker->GetPosition().GetDistance(player->GetPosition());

            MarkerTableRow markerRow = {};
            PopulateMarkerTableRow(currentMarker, markerDistance, markerRow);

            const RevLinks* links = nullptr;
            if (const auto it = revLink.find(currentMarker); it != revLink.end()) {
                links = &it->second;
            }

            if (applyFilter &&
                !MarkerFilterMatchesExpression(parseResult, markerRow, links, filterActorRow,
                    fillActorExpensive ? &filterActorTaskBuffer : nullptr, fillActorExpensive)) {
                currentForm = JC::jFormMapNextKey(JC::Domain, markerDb, currentForm, nullptr);
                continue;
            }

            FormatMarkerDescription(markerRow, header);
            CollapsedHeader(header.c_str(), currentMarker, MarkerTableRowName(markerRow), BRSS_MarkerControllerScript, links, headerOpenForce);

            currentForm = JC::jFormMapNextKey(JC::Domain, markerDb, currentForm, nullptr);
        }
        ImGuiMCP::EndChild();
    }

    ImGuiMCP::Separator();
    ImGuiMCP::Spacing();
    const bool focusNewMarkerNameShortcut = io && io->KeyCtrl && !io->KeyAlt && !io->KeySuper &&
        ImGuiMCP::IsWindowFocused(ImGuiMCP::ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_M, false);
    if (focusNewMarkerNameShortcut) {
        ImGuiMCP::SetKeyboardFocusHere();
    }
    ImGuiMCP::SetNextItemWidth(420.0f);
    const bool newMarkerNameEdited = ImGuiMCP::InputTextWithHint(
        "##NewMarkerName",
        "Marker name...",
        newMarkerNameBuffer,
        sizeof(newMarkerNameBuffer),
        ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue);
    const bool newMarkerNameEnter = newMarkerNameEdited &&
        (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter, false) ||
            ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_KeypadEnter, false));
    const auto addNewMarker = [&](RE::TESForm* form) {
        const std::string_view newMarkerName = newMarkerNameBuffer;
        for (char c : newMarkerName) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                Papyrus::Call(
                    BRSS_MarkerControllerScript,
                    "BRSSMarkerControllerScript",
                    "Add",
                    [](RE::BSScript::Variable) {},
                    std::string(newMarkerName),
                    static_cast<RE::TESForm*>(form)
                );
                break;
            }
        }

        newMarkerNameBuffer[0] = '\0';
    };

    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Add new marker") || newMarkerNameEnter) {
        addNewMarker(nullptr);
    }

    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Add new marker with form [DEBUG]")) {
        if (MarkerNameHasNonWhitespace(newMarkerNameBuffer)) {
            markerGridFormSelectorPending = false;
            FormSelector::Open();
        }
    }

    if (FormSelector::Window && !FormSelector::Window->IsOpen) {
        RE::TESForm* selectedForm = nullptr;
        const bool hasSelection = FormSelector::ConsumeSelectedForm(selectedForm);
        if (markerGridFormSelectorPending) {
            markerGridFormSelectorPending = false;
            if (hasSelection) {
                RequestMarkerGridPopup(newMarkerNameBuffer, selectedForm);
            }
        } else if (hasSelection) {
            addNewMarker(selectedForm);
        }
    }

    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Add new marker grid")) {
        if (MarkerNameHasNonWhitespace(newMarkerNameBuffer)) {
            RequestMarkerGridPopup(newMarkerNameBuffer);
        }
    }
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Add new marker grid with form [DEBUG]")) {
        if (MarkerNameHasNonWhitespace(newMarkerNameBuffer)) {
            markerGridFormSelectorPending = true;
            FormSelector::Open();
        }
    }

    RenderMarkerGridPopup(newMarkerNameBuffer, BRSS_MarkerControllerScript);
    RenderMarkerRenamePopup(BRSS_MarkerControllerScript);
}
