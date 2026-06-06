#include "actorListView.h"

#include <array>
#include <cctype>
#include <cstring>
#include <memory_resource>
#include <unordered_map>
#include <unordered_set>

#include "../SKSEMenuFramework.h"

#include "common/builders.h"
#include "common/filter.h"

#include "../JCAPI.h"
#include "../PAPI.h"
#include "../utility.h"

enum class ActorFilterField : std::uint8_t {
    Unknown = 0,
    Name,
    Id,
    Type,
    Age,
    Status,
    Location,
    Distance,
    Task,
};

static constexpr const char* kNameAliases[] = { "name", "na" };
static constexpr const char* kIdAliases[] = { "id" };
static constexpr const char* kTypeAliases[] = { "type", "ty" };
static constexpr const char* kAgeAliases[] = { "age", "ag" };
static constexpr const char* kStatusAliases[] = { "status", "st" };
static constexpr const char* kLocationAliases[] = { "location", "lo" };
static constexpr const char* kDistanceAliases[] = { "distance", "di" };
static constexpr const char* kTaskAliases[] = { "task", "ta" };

static constexpr FilterFieldSpec kActorFilterFields[] = {
    { static_cast<std::uint8_t>(ActorFilterField::Name), FilterFieldKind::Text, kNameAliases, std::size(kNameAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Id), FilterFieldKind::Text, kIdAliases, std::size(kIdAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Type), FilterFieldKind::Text, kTypeAliases, std::size(kTypeAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Age), FilterFieldKind::Text, kAgeAliases, std::size(kAgeAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Status), FilterFieldKind::Text, kStatusAliases, std::size(kStatusAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Location), FilterFieldKind::Text, kLocationAliases, std::size(kLocationAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Distance), FilterFieldKind::Number, kDistanceAliases, std::size(kDistanceAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Task), FilterFieldKind::Text, kTaskAliases, std::size(kTaskAliases), true },
};

static constexpr std::uint8_t kActorTextShorthandFieldIds[] = {
    static_cast<std::uint8_t>(ActorFilterField::Name),
    static_cast<std::uint8_t>(ActorFilterField::Id),
    static_cast<std::uint8_t>(ActorFilterField::Type),
    static_cast<std::uint8_t>(ActorFilterField::Age),
    static_cast<std::uint8_t>(ActorFilterField::Status),
    static_cast<std::uint8_t>(ActorFilterField::Location),
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
    case ActorFilterField::Age:
        return row.age;
    case ActorFilterField::Status:
        return row.status;
    case ActorFilterField::Location:
        return row.location;
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
    ActorTableRow& row) {
    PopulateActorTableRow(actor, fillExpensiveFields ? &taskBuffer : nullptr, row);
    if (FilterMatchesExpression(parseResult, &row, kActorFilterSchema, kActorFilterRowAccess)) {
        return true;
    }

    const auto childrenIt = followChildrenByParent.find(actor);
    if (childrenIt == followChildrenByParent.end()) {
        return false;
    }

    for (RE::Actor* child : childrenIt->second) {
        if (FollowSubtreeMatchesFilter(child, followChildrenByParent, parseResult, fillExpensiveFields, taskBuffer, row)) {
            return true;
        }
    }

    return false;
}

static void RenderActorTableRow(
    const ActorTableRow& row,
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
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.age));
    ImGuiMCP::TableSetColumnIndex(4);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.status));
    ImGuiMCP::TableSetColumnIndex(5);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.location));
    ImGuiMCP::TableSetColumnIndex(6);
    ImGuiMCP::Text("%.2f", row.distance);
    ImGuiMCP::TableSetColumnIndex(7);
    if (!hideTaskColumn) {
        ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(taskText));
    }
    ImGuiMCP::TableSetColumnIndex(8);
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
    if (ImGuiMCP::Button("X", actionButtonSizeVec)) {
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

static void RenderFollowSubtree(
    RE::Actor* actor,
    int followDepth,
    const FollowChildrenMap& followChildrenByParent,
    bool fillExpensiveFields,
    std::string& taskBuffer,
    ActorTableRow& row) {
    PopulateActorTableRow(actor, nullptr, row);

    std::string_view taskDisplay = row.task;
    if (followDepth == 0) {
        taskBuffer.clear();
        Utility::CreateTaskDescription(actor, taskBuffer);
        taskDisplay = taskBuffer;
    }

    const bool hideTaskColumn = followDepth > 0;
    RenderActorTableRow(row, taskDisplay, actor, followDepth, hideTaskColumn);

    const auto childrenIt = followChildrenByParent.find(actor);
    if (childrenIt == followChildrenByParent.end()) {
        return;
    }

    for (RE::Actor* child : childrenIt->second) {
        RenderFollowSubtree(child, followDepth + 1, followChildrenByParent, fillExpensiveFields, taskBuffer, row);
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
    const bool focusFilterShortcut = io && io->KeyCtrl && !io->KeyAlt && !io->KeySuper &&
        ImGuiMCP::IsWindowFocused(ImGuiMCP::ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_L, false);
    if (focusFilterShortcut) {
        ImGuiMCP::SetKeyboardFocusHere();
    }
    ImGuiMCP::SetNextItemWidth(-1.0f);
    ImGuiMCP::InputTextWithHint("##ActorListFilter", "Filter...", filterBuffer, sizeof(filterBuffer));
    AcceptActorRowDragDropAtTarget(nullptr);
    ImGuiMCP::TextDisabled(
        "Ctrl+L: focus filter  |  e.g. lydia 1000 or name ct lydia and ag ct child  |  not, ==, contains(ct), startswith(sw), endswith(ew), <, >  |  a or b and c = a or (b and c)  |  \"quotes for spaces\"");
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
        const float actionsColumnWidth = actionButtonSize * 3.0f + ImGuiMCP::GetStyle()->ItemSpacing.x * 2.0f +
                                         ImGuiMCP::GetStyle()->CellPadding.x * 2.0f;
        if (ImGuiMCP::BeginTable("ActorListTable", 9, tableFlags)) {
            ImGuiMCP::TableSetupColumn("Name", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 300.0f);
            ImGuiMCP::TableSetupColumn("ID");
            ImGuiMCP::TableSetupColumn("Type");
            ImGuiMCP::TableSetupColumn("Age");
            ImGuiMCP::TableSetupColumn("Status");
            ImGuiMCP::TableSetupColumn("Location");
            ImGuiMCP::TableSetupColumn("Distance");
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
                        FollowSubtreeMatchesFilter(currentActor, followChildrenByParent, parseResult, fillExpensiveFields, taskBuffer, row)) {
                        RenderFollowSubtree(currentActor, 0, followChildrenByParent, fillExpensiveFields, taskBuffer, row);
                    }
                }
                currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
            }

            ImGuiMCP::EndTable();
        }
        AcceptActorRowDragDropAtTarget(nullptr);
        ImGuiMCP::EndChild();
    }
}
