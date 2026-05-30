#include "actorListView.h"

#include "../SKSEMenuFramework.h"

#include "common/filter.h"

#include "../JCAPI.h"
#include "../utility.h"

enum class FilterField : std::uint8_t {
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

static FilterField FilterParseField(std::string_view ident) {
    if (FilterStringEqualsIgnoreCase(ident, "name") || FilterStringEqualsIgnoreCase(ident, "na")) {
        return FilterField::Name;
    }
    if (FilterStringEqualsIgnoreCase(ident, "id")) {
        return FilterField::Id;
    }
    if (FilterStringEqualsIgnoreCase(ident, "type") || FilterStringEqualsIgnoreCase(ident, "ty")) {
        return FilterField::Type;
    }
    if (FilterStringEqualsIgnoreCase(ident, "child") || FilterStringEqualsIgnoreCase(ident, "ch")) {
        return FilterField::Child;
    }
    if (FilterStringEqualsIgnoreCase(ident, "status") || FilterStringEqualsIgnoreCase(ident, "st")) {
        return FilterField::Status;
    }
    if (FilterStringEqualsIgnoreCase(ident, "location") || FilterStringEqualsIgnoreCase(ident, "lo")) {
        return FilterField::Location;
    }
    if (FilterStringEqualsIgnoreCase(ident, "distance") || FilterStringEqualsIgnoreCase(ident, "di")) {
        return FilterField::Distance;
    }
    if (FilterStringEqualsIgnoreCase(ident, "task") || FilterStringEqualsIgnoreCase(ident, "ta")) {
        return FilterField::Task;
    }
    return FilterField::Unknown;
}

static bool FilterParsePredicateAt(const FilterTokenizeResult& tokens, std::size_t startIndex, FilterPredicate& outPred,
    std::size_t& outNextIndex, FilterParseResult& err) {
    outPred = {};

    std::size_t i = startIndex;
    bool negated = false;
    if (i < tokens.count && tokens.tokens[i].kind == FilterTokenKind::Not) {
        negated = true;
        ++i;
        if (i >= tokens.count) {
            FilterSetParseError(err, i, "Expected identifier after not");
            return false;
        }
    }

    if (tokens.tokens[i].kind != FilterTokenKind::Identifier) {
        FilterSetParseError(err, i, "Expected identifier");
        return false;
    }

    const FilterField field = FilterParseField(tokens.tokens[i].lexeme);
    if (field == FilterField::Unknown) {
        FilterSetParseError(err, i, "Unknown field");
        return false;
    }
    ++i;

    if (field == FilterField::Child) {
        outPred.field = static_cast<std::uint8_t>(FilterField::Child);
        outPred.op = FilterOp::IsTrue;
        outPred.negated = negated;
        outPred.valueKind = FilterValueKind::None;
        outNextIndex = i;
        return true;
    }

    if (i >= tokens.count) {
        FilterSetParseError(err, i, "Expected operator");
        return false;
    }

    FilterOp op = FilterOp::Equals;
    const FilterToken& opTok = tokens.tokens[i];
    if (opTok.kind == FilterTokenKind::Equals) {
        op = FilterOp::Equals;
        ++i;
    } else if (opTok.kind == FilterTokenKind::Less) {
        op = FilterOp::Less;
        ++i;
    } else if (opTok.kind == FilterTokenKind::Greater) {
        op = FilterOp::Greater;
        ++i;
    } else if (opTok.kind == FilterTokenKind::LessEqual) {
        op = FilterOp::LessEqual;
        ++i;
    } else if (opTok.kind == FilterTokenKind::GreaterEqual) {
        op = FilterOp::GreaterEqual;
        ++i;
    } else if (opTok.kind == FilterTokenKind::Identifier) {
        if (!FilterIsOpKeyword(opTok.lexeme, op)) {
            FilterSetParseError(err, i, "Unknown operator");
            return false;
        }
        ++i;
    } else {
        FilterSetParseError(err, i, "Expected operator");
        return false;
    }

    if (i >= tokens.count) {
        FilterSetParseError(err, i, "Expected value");
        return false;
    }

    if (field == FilterField::Distance) {
        const bool numericOp =
            (op == FilterOp::Less || op == FilterOp::Greater || op == FilterOp::LessEqual || op == FilterOp::GreaterEqual);
        if (!numericOp) {
            FilterSetParseError(err, i - 1, "Invalid operator for distance");
            return false;
        }
        if (tokens.tokens[i].kind != FilterTokenKind::Number) {
            FilterSetParseError(err, i, "Expected number");
            return false;
        }
        double value = 0.0;
        if (!FilterTryParseDouble(tokens.tokens[i].lexeme, value)) {
            FilterSetParseError(err, i, "Invalid number");
            return false;
        }
        ++i;

        outPred.field = static_cast<std::uint8_t>(field);
        outPred.op = op;
        outPred.negated = negated;
        outPred.valueKind = FilterValueKind::Number;
        outPred.numberValue = value;
        outNextIndex = i;
        return true;
    }

    const bool textOp = (op == FilterOp::Equals || op == FilterOp::Contains || op == FilterOp::StartsWith || op == FilterOp::EndsWith);
    if (!textOp) {
        FilterSetParseError(err, i - 1, "Invalid operator for text field");
        return false;
    }
    if (tokens.tokens[i].kind != FilterTokenKind::Identifier && tokens.tokens[i].kind != FilterTokenKind::String) {
        FilterSetParseError(err, i, "Expected text");
        return false;
    }
    const std::string_view valueLex = tokens.tokens[i].lexeme;
    ++i;

    outPred.field = static_cast<std::uint8_t>(field);
    outPred.op = op;
    outPred.negated = negated;
    outPred.valueKind = FilterValueKind::Text;
    outPred.textValue = valueLex;
    outNextIndex = i;
    return true;
}

struct ActorTableRow {
    std::string_view name = {};
    std::string_view idHex = {};
    std::string_view type = {};
    std::string_view status = {};
    std::string_view location = {};
    std::string_view task = {};
    bool isChild = false;
    float distance = 0.0f;
};

static const char* ActorTableRowTextOrEmpty(std::string_view text) {
    return text.empty() ? "" : text.data();
}

static void PopulateActorTableRow(RE::Actor* actor, RE::PlayerCharacter* player, char* idHexBuf, std::size_t idHexBufSize,
    std::string* taskBuf, ActorTableRow& out) {
    out = {};

    if (const char* displayName = actor->GetDisplayFullName()) {
        out.name = displayName;
    }

    std::snprintf(idHexBuf, idHexBufSize, "%08X", actor->GetFormID());
    out.idHex = idHexBuf;

    if (const RE::TESBoundObject* base = actor->GetBaseObject()) {
        if (const char* typeName = base->GetName()) {
            out.type = typeName;
        }
    }

    out.isChild = actor->IsChild();
    out.status = actor->IsDead() ? "Dead" : "Alive";

    if (const RE::BGSLocation* location = actor->GetCurrentLocation()) {
        if (const char* locName = location->GetName()) {
            out.location = locName;
        }
    } else {
        out.location = "Unknown";
    }

    out.distance = actor->GetPosition().GetDistance(player->GetPosition());

    if (taskBuf != nullptr) {
        taskBuf->clear();
        Utility::CreateTaskDescription(actor, *taskBuf);
        out.task = *taskBuf;
    }
}

static bool FilterMatchesPredicate(const ActorTableRow& values, const FilterPredicate& pred) {
    bool matched = false;
    const auto predField = static_cast<FilterField>(pred.field);

    if (pred.op == FilterOp::IsTrue) {
        if (predField == FilterField::Child) {
            matched = values.isChild;
        } else {
            matched = false;
        }
    } else if (predField == FilterField::Distance) {
        matched = FilterMatchFloatOp(pred.op, values.distance, pred.numberValue);
    } else if (pred.valueKind == FilterValueKind::Text) {
        std::string_view fieldText = {};
        switch (predField) {
        case FilterField::Name:
            fieldText = values.name;
            break;
        case FilterField::Id:
            fieldText = values.idHex;
            break;
        case FilterField::Type:
            fieldText = values.type;
            break;
        case FilterField::Status:
            fieldText = values.status;
            break;
        case FilterField::Location:
            fieldText = values.location;
            break;
        case FilterField::Task:
            fieldText = values.task;
            break;
        default:
            fieldText = {};
            break;
        }
        matched = FilterMatchTextOp(pred.op, fieldText, pred.textValue);
    }

    return pred.negated ? !matched : matched;
}

static bool FilterMatchesAndGroup(const ActorTableRow& values, const FilterAndGroup& group) {
    for (std::size_t i = 0; i < group.predicateCount; ++i) {
        if (!FilterMatchesPredicate(values, group.predicates[i])) {
            return false;
        }
    }
    return true;
}

static bool FilterMatchesExpression(const FilterParseResult& expr, const ActorTableRow& values) {
    for (std::size_t g = 0; g < expr.andGroupCount; ++g) {
        if (FilterMatchesAndGroup(values, expr.andGroups[g])) {
            return true;
        }
    }
    return false;
}

static void RenderActorTableRow(const ActorTableRow& row, const char* taskText) {
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
    ImGuiMCP::TextUnformatted(taskText != nullptr ? taskText : "");
}

void __stdcall UI::RenderActorListView() {
    static char filterBuffer[256] = {};
    static char lastTokenizedFilter[256] = {};
    static FilterTokenizeResult tokenizeResult = {};
    static FilterParseResult parseResult = {};
    static bool filterUsesTask = false;
    static char filterIdHexBuf[16] = {};
    static std::string taskBuffer = [] {
        std::string s;
        s.reserve(128);
        return s;
    }();

    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return;
    }

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
        FilterParseExpression(tokenizeResult, parseResult, FilterParsePredicateAt);
        filterUsesTask = FilterExpressionUsesField(parseResult, static_cast<std::uint8_t>(FilterField::Task));
    }
    if (!parseResult.ok && parseResult.error[0] != '\0') {
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", parseResult.error);
    }
    ImGuiMCP::Spacing();

    const bool applyFilter = parseResult.hasExpression;

    constexpr ImGuiMCP::ImGuiTableFlags tableFlags = ImGuiMCP::ImGuiTableFlags_Resizable |
                                                     ImGuiMCP::ImGuiTableFlags_RowBg |
                                                     ImGuiMCP::ImGuiTableFlags_SizingFixedFit;
    if (ImGuiMCP::BeginTable("ActorListTable", 8, tableFlags)) {
        ImGuiMCP::TableSetupColumn("Name");
        ImGuiMCP::TableSetupColumn("ID");
        ImGuiMCP::TableSetupColumn("Type");
        ImGuiMCP::TableSetupColumn("Child");
        ImGuiMCP::TableSetupColumn("Status");
        ImGuiMCP::TableSetupColumn("Location");
        ImGuiMCP::TableSetupColumn("Distance");
        ImGuiMCP::TableSetupColumn("Task", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
        ImGuiMCP::TableHeadersRow();

        RE::TESForm* currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, nullptr, nullptr);
        while (currentForm) {
            RE::Actor* currentActor = currentForm->As<RE::Actor>();
            if (!currentActor) {
                currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
                continue;
            }

            ActorTableRow row = {};
            const bool rowTaskFilledForFilter = applyFilter && filterUsesTask;
            PopulateActorTableRow(currentActor, player, filterIdHexBuf, sizeof(filterIdHexBuf),
                rowTaskFilledForFilter ? &taskBuffer : nullptr, row);

            if (applyFilter && !FilterMatchesExpression(parseResult, row)) {
                currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
                continue;
            }

            if (!rowTaskFilledForFilter) {
                taskBuffer.clear();
                Utility::CreateTaskDescription(currentActor, taskBuffer);
            }
            RenderActorTableRow(row, taskBuffer.c_str());
            if (!rowTaskFilledForFilter) {
                taskBuffer.clear();
            }

            currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
        }
        ImGuiMCP::EndTable();
    }
}
