#include "actorListView.h"

#include "../SKSEMenuFramework.h"

#include "common/builders.h"
#include "common/filter.h"

#include "../JCAPI.h"
#include "../PAPI.h"
#include "../utility.h"

#include <cctype>

enum class ActorFilterField : std::uint8_t {
    Unknown = 0,
    Name,
    Id,
    Type,
    Child,
    Status,
    Location,
    Distance,
    Task,
};

static constexpr const char* kNameAliases[] = { "name", "na" };
static constexpr const char* kIdAliases[] = { "id" };
static constexpr const char* kTypeAliases[] = { "type", "ty" };
static constexpr const char* kChildAliases[] = { "child", "ch" };
static constexpr const char* kStatusAliases[] = { "status", "st" };
static constexpr const char* kLocationAliases[] = { "location", "lo" };
static constexpr const char* kDistanceAliases[] = { "distance", "di" };
static constexpr const char* kTaskAliases[] = { "task", "ta" };

static constexpr FilterFieldSpec kActorFilterFields[] = {
    { static_cast<std::uint8_t>(ActorFilterField::Name), FilterFieldKind::Text, kNameAliases, std::size(kNameAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Id), FilterFieldKind::Text, kIdAliases, std::size(kIdAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Type), FilterFieldKind::Text, kTypeAliases, std::size(kTypeAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Child), FilterFieldKind::Bool, kChildAliases, std::size(kChildAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Status), FilterFieldKind::Text, kStatusAliases, std::size(kStatusAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Location), FilterFieldKind::Text, kLocationAliases, std::size(kLocationAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Distance), FilterFieldKind::Number, kDistanceAliases, std::size(kDistanceAliases), false },
    { static_cast<std::uint8_t>(ActorFilterField::Task), FilterFieldKind::Text, kTaskAliases, std::size(kTaskAliases), true },
};

static constexpr FilterSchema kActorFilterSchema{ kActorFilterFields, std::size(kActorFilterFields) };

static std::string_view ActorFilterRowText(const void* rowContext, std::uint8_t fieldId);
static bool ActorFilterRowBool(const void* rowContext, std::uint8_t fieldId);
static float ActorFilterRowNumber(const void* rowContext, std::uint8_t fieldId);

static const FilterRowAccess kActorFilterRowAccess{
    ActorFilterRowText,
    ActorFilterRowBool,
    ActorFilterRowNumber,
};

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
        return row.idHex;
    case ActorFilterField::Type:
        return row.type;
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
    const auto& row = *static_cast<const ActorTableRow*>(rowContext);
    switch (static_cast<ActorFilterField>(fieldId)) {
    case ActorFilterField::Child:
        return row.isChild;
    default:
        return false;
    }
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

static void RenderActorTableRow(const ActorTableRow& row, std::string_view taskText, RE::Actor* actor) {
    static char nameEditBuffer[256] = {};

    ImGuiMCP::TableNextRow();
    ImGuiMCP::TableSetColumnIndex(0);
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
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(taskText));
    ImGuiMCP::TableSetColumnIndex(8);
    ImGuiMCP::PushID(static_cast<int>(actor->GetFormID()));
    const float deleteButtonSize = ImGuiMCP::GetFrameHeight();
    if (ImGuiMCP::Button("X", ImGuiMCP::ImVec2{ deleteButtonSize, deleteButtonSize })) {
        Papyrus::Call(
            actor,
            "BRSSActorScript",
            "RemoveFromBRSS",
            [](RE::BSScript::Variable) {}
        );
    }
    ImGuiMCP::PopID();
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

    if (ImGuiMCP::IsWindowFocused(ImGuiMCP::ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGuiMCP::IsAnyItemActive() && !ImGuiMCP::IsMouseClicked(ImGuiMCP::ImGuiMouseButton_Left)) {
        ImGuiMCP::SetKeyboardFocusHere();
    }
    ImGuiMCP::SetNextItemWidth(-1.0f);
    ImGuiMCP::InputTextWithHint("##ActorListFilter", "Filter...", filterBuffer, sizeof(filterBuffer));
    ImGuiMCP::TextDisabled(
        "e.g. name contains lydia and di < 1000 or not child  |  not, ==, contains(ct), startswith(sw), endswith(ew), <, >  |  a or b and c = a or (b and c)  |  \"quotes for spaces\"");
    if (std::strcmp(filterBuffer, lastTokenizedFilter) != 0) {
        strncpy_s(lastTokenizedFilter, filterBuffer, sizeof(lastTokenizedFilter));
        lastTokenizedFilter[sizeof(lastTokenizedFilter) - 1] = '\0';
        FilterTokenize(filterBuffer, tokenizeResult);
        FilterParseExpression(tokenizeResult, parseResult, kActorFilterSchema);
        filterUsesExpensiveField = FilterExpressionUsesExpensiveField(parseResult, kActorFilterSchema);
    }
    if (!parseResult.ok && parseResult.error[0] != '\0') {
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", parseResult.error);
    }
    ImGuiMCP::Spacing();

    const bool applyFilter = tokenizeResult.ok && parseResult.hasExpression;

    constexpr ImGuiMCP::ImGuiTableFlags tableFlags = ImGuiMCP::ImGuiTableFlags_Resizable |
                                                     ImGuiMCP::ImGuiTableFlags_RowBg |
                                                     ImGuiMCP::ImGuiTableFlags_SizingFixedFit;
    const float deleteColumnWidth = ImGuiMCP::GetFrameHeight() + ImGuiMCP::GetStyle()->CellPadding.x * 2.0f;
    if (ImGuiMCP::BeginTable("ActorListTable", 9, tableFlags)) {
        ImGuiMCP::TableSetupColumn("Name", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 300.0f);
        ImGuiMCP::TableSetupColumn("ID");
        ImGuiMCP::TableSetupColumn("Type");
        ImGuiMCP::TableSetupColumn("Child");
        ImGuiMCP::TableSetupColumn("Status");
        ImGuiMCP::TableSetupColumn("Location");
        ImGuiMCP::TableSetupColumn("Distance");
        ImGuiMCP::TableSetupColumn("Task", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
        ImGuiMCP::TableSetupColumn("", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, deleteColumnWidth);
        ImGuiMCP::TableHeadersRow();

        RE::TESForm* currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, nullptr, nullptr);
        while (currentForm) {
            RE::Actor* currentActor = currentForm->As<RE::Actor>();
            if (!currentActor) {
                currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
                continue;
            }

            ActorTableRow row = {};
            const bool fillExpensiveFields = applyFilter && filterUsesExpensiveField;
            PopulateActorTableRow(currentActor, fillExpensiveFields ? &taskBuffer : nullptr, row);

            if (applyFilter && !FilterMatchesExpression(parseResult, &row, kActorFilterSchema, kActorFilterRowAccess)) {
                currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
                continue;
            }

            std::string_view taskDisplay = row.task;
            if (!fillExpensiveFields) {
                taskBuffer.clear();
                Utility::CreateTaskDescription(currentActor, taskBuffer);
                taskDisplay = taskBuffer;
            }
            RenderActorTableRow(row, taskDisplay, currentActor);

            currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
        }
        ImGuiMCP::EndTable();
    }
}
