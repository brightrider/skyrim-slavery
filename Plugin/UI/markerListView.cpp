#include "markerListView.h"

#include <cctype>
#include <cstring>
#include <memory_resource>

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
    MDesc,
    MLocation,
    MDistance,
    AName = 16,
    AId,
    AType,
    AChild,
    AStatus,
    ALocation,
    ADistance,
    ATask,
};

static constexpr const char* kMNameAliases[] = { "mname", "mna" };
static constexpr const char* kMIdAliases[] = { "mid" };
static constexpr const char* kMDescAliases[] = { "mdesc", "mde" };
static constexpr const char* kMLocationAliases[] = { "mloc", "mlo" };
static constexpr const char* kMDistanceAliases[] = { "mdist", "mdi" };
static constexpr const char* kNameAliases[] = { "name", "na" };
static constexpr const char* kIdAliases[] = { "id" };
static constexpr const char* kTypeAliases[] = { "type", "ty" };
static constexpr const char* kChildAliases[] = { "child", "ch" };
static constexpr const char* kStatusAliases[] = { "status", "st" };
static constexpr const char* kLocationAliases[] = { "location", "lo" };
static constexpr const char* kDistanceAliases[] = { "distance", "di" };
static constexpr const char* kTaskAliases[] = { "task", "ta" };

static constexpr FilterFieldSpec kMarkerListFilterFields[] = {
    { static_cast<std::uint8_t>(MarkerListFilterField::MName), FilterFieldKind::Text, kMNameAliases, std::size(kMNameAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::MId), FilterFieldKind::Text, kMIdAliases, std::size(kMIdAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::MDesc), FilterFieldKind::Text, kMDescAliases, std::size(kMDescAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::MLocation), FilterFieldKind::Text, kMLocationAliases, std::size(kMLocationAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::MDistance), FilterFieldKind::Number, kMDistanceAliases, std::size(kMDistanceAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::AName), FilterFieldKind::Text, kNameAliases, std::size(kNameAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::AId), FilterFieldKind::Text, kIdAliases, std::size(kIdAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::AType), FilterFieldKind::Text, kTypeAliases, std::size(kTypeAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::AChild), FilterFieldKind::Bool, kChildAliases, std::size(kChildAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::AStatus), FilterFieldKind::Text, kStatusAliases, std::size(kStatusAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::ALocation), FilterFieldKind::Text, kLocationAliases, std::size(kLocationAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::ADistance), FilterFieldKind::Number, kDistanceAliases, std::size(kDistanceAliases), false },
    { static_cast<std::uint8_t>(MarkerListFilterField::ATask), FilterFieldKind::Text, kTaskAliases, std::size(kTaskAliases), true },
};

static constexpr FilterSchema kMarkerListFilterSchema{ kMarkerListFilterFields, std::size(kMarkerListFilterFields) };

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
        return row.jcName;
    case MarkerListFilterField::MId:
        return row.idHex;
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
        return row.idHex;
    case MarkerListFilterField::AType:
        return row.type;
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
    const auto& row = *static_cast<const ActorTableRow*>(rowContext);
    switch (static_cast<MarkerListFilterField>(fieldId)) {
    case MarkerListFilterField::AChild:
        return row.isChild;
    default:
        return false;
    }
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

static bool MarkerFilterAndGroupMatches(const FilterAndGroup& group, const MarkerTableRow& markerRow, const RevLinks* links,
    ActorTableRow& actorRow, std::string* actorTaskBuf, bool fillActorExpensive) {
    bool hasActorPredicate = false;

    for (std::size_t i = 0; i < group.predicateCount; ++i) {
        const FilterPredicate& pred = group.predicates[i];
        if (IsMarkerListFilterMarkerField(pred.field)) {
            if (!FilterMatchPredicate(pred, &markerRow, kMarkerListFilterSchema, kMarkerFilterRowAccess)) {
                return false;
            }
        } else {
            hasActorPredicate = true;
        }
    }

    if (!hasActorPredicate) {
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

        bool actorMatchesAll = true;
        for (std::size_t i = 0; i < group.predicateCount; ++i) {
            const FilterPredicate& pred = group.predicates[i];
            if (IsMarkerListFilterMarkerField(pred.field)) {
                continue;
            }
            if (!FilterMatchPredicate(pred, &actorRow, kMarkerListFilterSchema, kMarkerListActorFilterRowAccess)) {
                actorMatchesAll = false;
                break;
            }
        }

        if (actorMatchesAll) {
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

static void RenderActorTableRow(const ActorTableRow& row) {
    ImGuiMCP::TableNextRow();
    ImGuiMCP::TableSetColumnIndex(0);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.name));
    ImGuiMCP::TableSetColumnIndex(1);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.idHex));
    ImGuiMCP::TableSetColumnIndex(2);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.type));
    ImGuiMCP::TableSetColumnIndex(3);
    ImGuiMCP::TextUnformatted(row.isChild ? "Yes" : "No");
    ImGuiMCP::TableSetColumnIndex(4);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.status));
    ImGuiMCP::TableSetColumnIndex(5);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.location));
    ImGuiMCP::TableSetColumnIndex(6);
    ImGuiMCP::Text("%.2f", row.distance);
    ImGuiMCP::TableSetColumnIndex(7);
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
    if (ImGuiMCP::BeginTable("LinkedActors", 8, tableFlags)) {
        ImGuiMCP::TableSetupColumn("Name");
        ImGuiMCP::TableSetupColumn("ID");
        ImGuiMCP::TableSetupColumn("Type");
        ImGuiMCP::TableSetupColumn("Child");
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
            RenderActorTableRow(row);
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

static void CollapsedHeader(const char* label, const void* uniqueId, std::string_view markerName,
    RE::TESQuest* markerControllerScript, const RevLinks* links, MarkerHeaderOpenForce openForce) {
    ImGuiMCP::PushID(uniqueId);

    const float rowY = ImGuiMCP::GetCursorPosY();
    const float lineStartX = ImGuiMCP::GetCursorPosX();
    const float contentRight = ImGuiMCP::GetWindowContentRegionMax().x;
    const float rowWidth = contentRight - lineStartX;
    const float actionButtonSize = ImGuiMCP::GetFrameHeight();
    const ImGuiMCP::ImVec2 actionButtonSizeVec{ actionButtonSize, actionButtonSize };
    const float columnsSpacing = ImGuiMCP::GetStyle()->ColumnsMinSpacing;
    const float actionButtonsWidth = actionButtonSize * 2.0f + columnsSpacing;

    ImGuiMCP::Columns(2, nullptr, false);
    ImGuiMCP::SetColumnWidth(0, rowWidth - actionButtonsWidth - columnsSpacing);

    if (openForce == MarkerHeaderOpenForce::Expand) {
        ImGuiMCP::SetNextItemOpen(true, ImGuiMCP::ImGuiCond_Always);
    } else if (openForce == MarkerHeaderOpenForce::Collapse) {
        ImGuiMCP::SetNextItemOpen(false, ImGuiMCP::ImGuiCond_Always);
    }
    const bool headerOpen = ImGuiMCP::CollapsingHeader(label);

    ImGuiMCP::NextColumn();
    ImGuiMCP::SetCursorPosY(rowY);

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

    ImGuiMCP::Columns(1);

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

    if (ImGuiMCP::GetTopMostPopupModal() == nullptr &&
        ImGuiMCP::IsWindowFocused(ImGuiMCP::ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGuiMCP::IsAnyItemActive() && !ImGuiMCP::IsMouseClicked(ImGuiMCP::ImGuiMouseButton_Left)) {
        ImGuiMCP::SetKeyboardFocusHere();
    }
    ImGuiMCP::SetNextItemWidth(-1.0f);
    ImGuiMCP::InputTextWithHint("##MarkerListFilter", "Filter...", filterBuffer, sizeof(filterBuffer));
    ImGuiMCP::TextDisabled(
        "e.g. todo");
    if (std::strcmp(filterBuffer, lastTokenizedFilter) != 0) {
        strncpy_s(lastTokenizedFilter, filterBuffer, sizeof(lastTokenizedFilter));
        lastTokenizedFilter[sizeof(lastTokenizedFilter) - 1] = '\0';
        FilterTokenize(filterBuffer, tokenizeResult);
        FilterParseExpression(tokenizeResult, parseResult, kMarkerListFilterSchema);
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

    const bool applyFilter = tokenizeResult.ok && parseResult.hasExpression;
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
        CollapsedHeader(header.c_str(), currentMarker, markerRow.jcName, BRSS_MarkerControllerScript, links, headerOpenForce);

        currentForm = JC::jFormMapNextKey(JC::Domain, markerDb, currentForm, nullptr);
    }

    ImGuiMCP::Separator();
    ImGuiMCP::Spacing();
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
    if (ImGuiMCP::Button("Add new marker with form")) {
        if (MarkerNameHasNonWhitespace(newMarkerNameBuffer)) {
            FormSelector::Open();
        }
    }

    if (FormSelector::Window && !FormSelector::Window->IsOpen) {
        RE::TESForm* selectedForm = nullptr;
        if (FormSelector::ConsumeSelectedForm(selectedForm)) {
            addNewMarker(selectedForm);
        }
    }

    RenderMarkerRenamePopup(BRSS_MarkerControllerScript);
}
