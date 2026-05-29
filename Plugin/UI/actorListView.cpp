#include "actorListView.h"

#include "../SKSEMenuFramework.h"

#include "../JCAPI.h"
#include "../utility.h"

enum class FilterTokenKind : std::uint8_t {
    Identifier,
    String,
    Number,
    Equals,
    Less,
    Greater,
    LessEqual,
    GreaterEqual,
    And,
    Or,
    Not,
};

struct FilterToken {
    FilterTokenKind kind;
    std::string_view lexeme;
};

struct FilterTokenizeResult {
    bool ok = true;
    char error[128] = {};
    std::size_t count = 0;
    FilterToken tokens[64] = {};
};

enum class FilterField : std::uint8_t {
    Name,
    Id,
    Type,
    Child,
    Status,
    Location,
    Distance,
    Task,
    Unknown,
};

enum class FilterOp : std::uint8_t {
    Equals,
    Contains,
    StartsWith,
    EndsWith,
    Less,
    Greater,
    LessEqual,
    GreaterEqual,
    IsTrue,
};

enum class FilterValueKind : std::uint8_t {
    None,
    Text,
    Number,
};

struct FilterPredicate {
    FilterField field = FilterField::Unknown;
    FilterOp op = FilterOp::Equals;
    bool negated = false;

    FilterValueKind valueKind = FilterValueKind::None;
    std::string_view textValue = {};
    double numberValue = 0.0;
};

constexpr std::size_t FilterMaxPredicatesPerAndGroup = 16;
constexpr std::size_t FilterMaxAndGroups = 8;

struct FilterAndGroup {
    FilterPredicate predicates[FilterMaxPredicatesPerAndGroup] = {};
    std::size_t predicateCount = 0;
};

struct FilterParseResult {
    bool ok = true;
    bool partial = false;
    bool hasExpression = false;
    char error[128] = {};
    FilterAndGroup andGroups[FilterMaxAndGroups] = {};
    std::size_t andGroupCount = 0;
};

static bool FilterCharEqualsIgnoreCase(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

static bool FilterStringEqualsIgnoreCase(std::string_view word, const char* literal) {
    const std::size_t literalLen = std::strlen(literal);
    if (word.size() != literalLen) {
        return false;
    }
    for (std::size_t i = 0; i < word.size(); ++i) {
        if (!FilterCharEqualsIgnoreCase(word[i], literal[i])) {
            return false;
        }
    }
    return true;
}

static bool FilterIsIdentifierStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

static bool FilterIsIdentifierChar(char c) {
    return FilterIsIdentifierStart(c) || std::isdigit(static_cast<unsigned char>(c));
}

static FilterTokenKind FilterClassifyKeyword(std::string_view word) {
    if (FilterStringEqualsIgnoreCase(word, "and")) {
        return FilterTokenKind::And;
    }
    if (FilterStringEqualsIgnoreCase(word, "or")) {
        return FilterTokenKind::Or;
    }
    if (FilterStringEqualsIgnoreCase(word, "not")) {
        return FilterTokenKind::Not;
    }
    return FilterTokenKind::Identifier;
}

static void FilterSetTokenizeError(FilterTokenizeResult& result, std::size_t position, const char* message) {
    result.ok = false;
    result.count = 0;
    std::snprintf(result.error, sizeof(result.error), "%s at position %zu", message, position);
}

static bool FilterTryPushToken(FilterTokenizeResult& result, FilterTokenKind kind, std::string_view lexeme) {
    if (result.count >= std::size(result.tokens)) {
        FilterSetTokenizeError(result, 0, "Too many tokens");
        return false;
    }
    result.tokens[result.count++] = FilterToken{ kind, lexeme };
    return true;
}

static bool FilterTokenize(const char* input, FilterTokenizeResult& result) {
    result.ok = true;
    result.count = 0;
    result.error[0] = '\0';

    if (input == nullptr || input[0] == '\0') {
        return true;
    }

    const char* cursor = input;
    while (*cursor != '\0') {
        const std::size_t position = static_cast<std::size_t>(cursor - input);

        if (std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
            continue;
        }

        if (*cursor == '"') {
            const char* start = cursor + 1;
            cursor = start;
            while (*cursor != '\0' && *cursor != '"') {
                ++cursor;
            }
            if (*cursor != '"') {
                FilterSetTokenizeError(result, position, "Unterminated string");
                return false;
            }
            if (!FilterTryPushToken(result, FilterTokenKind::String, std::string_view(start, static_cast<std::size_t>(cursor - start)))) {
                return false;
            }
            ++cursor;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(*cursor))) {
            const char* start = cursor;
            ++cursor;
            while (std::isdigit(static_cast<unsigned char>(*cursor))) {
                ++cursor;
            }
            if (*cursor == '.') {
                ++cursor;
                if (!std::isdigit(static_cast<unsigned char>(*cursor))) {
                    FilterSetTokenizeError(result, position, "Invalid number");
                    return false;
                }
                while (std::isdigit(static_cast<unsigned char>(*cursor))) {
                    ++cursor;
                }
            }
            if (FilterIsIdentifierChar(*cursor)) {
                FilterSetTokenizeError(result, position, "Invalid number");
                return false;
            }
            if (!FilterTryPushToken(result, FilterTokenKind::Number,
                    std::string_view(start, static_cast<std::size_t>(cursor - start)))) {
                return false;
            }
            continue;
        }

        if (FilterIsIdentifierStart(*cursor)) {
            const char* start = cursor;
            ++cursor;
            while (FilterIsIdentifierChar(*cursor)) {
                ++cursor;
            }
            const std::string_view word(start, static_cast<std::size_t>(cursor - start));
            if (!FilterTryPushToken(result, FilterClassifyKeyword(word), word)) {
                return false;
            }
            continue;
        }

        if (*cursor == '=') {
            if (*(cursor + 1) != '=') {
                const char* after = cursor + 1;
                while (*after != '\0' && std::isspace(static_cast<unsigned char>(*after))) {
                    ++after;
                }
                if (*after == '\0') {
                    break;
                }
                FilterSetTokenizeError(result, position, "Expected ==");
                return false;
            }
            if (!FilterTryPushToken(result, FilterTokenKind::Equals, std::string_view("==", 2))) {
                return false;
            }
            cursor += 2;
            continue;
        }

        if (*cursor == '<') {
            if (*(cursor + 1) == '=') {
                if (!FilterTryPushToken(result, FilterTokenKind::LessEqual, std::string_view("<=", 2))) {
                    return false;
                }
                cursor += 2;
            } else {
                if (!FilterTryPushToken(result, FilterTokenKind::Less, std::string_view("<", 1))) {
                    return false;
                }
                ++cursor;
            }
            continue;
        }

        if (*cursor == '>') {
            if (*(cursor + 1) == '=') {
                if (!FilterTryPushToken(result, FilterTokenKind::GreaterEqual, std::string_view(">=", 2))) {
                    return false;
                }
                cursor += 2;
            } else {
                if (!FilterTryPushToken(result, FilterTokenKind::Greater, std::string_view(">", 1))) {
                    return false;
                }
                ++cursor;
            }
            continue;
        }

        FilterSetTokenizeError(result, position, "Unexpected character");
        return false;
    }

    return true;
}

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

static bool FilterTryParseDouble(std::string_view sv, double& out) {
    if (sv.empty()) {
        return false;
    }

    char buf[64] = {};
    const std::size_t n = (sv.size() < (sizeof(buf) - 1)) ? sv.size() : (sizeof(buf) - 1);
    std::memcpy(buf, sv.data(), n);
    buf[n] = '\0';

    char* end = nullptr;
    out = std::strtod(buf, &end);
    if (end == buf) {
        return false;
    }
    while (*end != '\0') {
        if (!std::isspace(static_cast<unsigned char>(*end))) {
            return false;
        }
        ++end;
    }
    return true;
}

static void FilterSetParseError(FilterParseResult& result, std::size_t tokenIndex, const char* message) {
    result.ok = false;
    result.hasExpression = false;
    result.andGroupCount = 0;
    std::snprintf(result.error, sizeof(result.error), "%s at token %zu", message, tokenIndex + 1);
}

static bool FilterIsOpKeyword(std::string_view ident, FilterOp& outOp) {
    if (FilterStringEqualsIgnoreCase(ident, "contains") || FilterStringEqualsIgnoreCase(ident, "ct")) {
        outOp = FilterOp::Contains;
        return true;
    }
    if (FilterStringEqualsIgnoreCase(ident, "startswith") || FilterStringEqualsIgnoreCase(ident, "sw")) {
        outOp = FilterOp::StartsWith;
        return true;
    }
    if (FilterStringEqualsIgnoreCase(ident, "endswith") || FilterStringEqualsIgnoreCase(ident, "ew")) {
        outOp = FilterOp::EndsWith;
        return true;
    }
    return false;
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
        outPred.field = FilterField::Child;
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

        outPred.field = field;
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

    outPred.field = field;
    outPred.op = op;
    outPred.negated = negated;
    outPred.valueKind = FilterValueKind::Text;
    outPred.textValue = valueLex;
    outNextIndex = i;
    return true;
}

static bool FilterParseAndGroup(const FilterTokenizeResult& tokens, std::size_t& ioIndex, FilterAndGroup& group, FilterParseResult& err) {
    group.predicateCount = 0;

    FilterPredicate pred = {};
    if (!FilterParsePredicateAt(tokens, ioIndex, pred, ioIndex, err)) {
        return false;
    }
    if (group.predicateCount >= std::size(group.predicates)) {
        FilterSetParseError(err, ioIndex, "Too many predicates");
        return false;
    }
    group.predicates[group.predicateCount++] = pred;

    while (ioIndex < tokens.count && tokens.tokens[ioIndex].kind == FilterTokenKind::And) {
        ++ioIndex;
        if (!FilterParsePredicateAt(tokens, ioIndex, pred, ioIndex, err)) {
            return false;
        }
        if (group.predicateCount >= std::size(group.predicates)) {
            FilterSetParseError(err, ioIndex, "Too many predicates");
            return false;
        }
        group.predicates[group.predicateCount++] = pred;
    }

    return true;
}

static bool FilterParseAndGroupPartial(const FilterTokenizeResult& tokens, std::size_t& ioIndex, FilterAndGroup& group,
    FilterParseResult& err) {
    group.predicateCount = 0;

    FilterPredicate pred = {};
    if (!FilterParsePredicateAt(tokens, ioIndex, pred, ioIndex, err)) {
        return false;
    }
    if (group.predicateCount >= std::size(group.predicates)) {
        FilterSetParseError(err, ioIndex, "Too many predicates");
        return false;
    }
    group.predicates[group.predicateCount++] = pred;

    while (ioIndex < tokens.count && tokens.tokens[ioIndex].kind == FilterTokenKind::And) {
        const std::size_t andTokenIndex = ioIndex;
        ++ioIndex;
        if (!FilterParsePredicateAt(tokens, ioIndex, pred, ioIndex, err)) {
            ioIndex = andTokenIndex;
            return true;
        }
        if (group.predicateCount >= std::size(group.predicates)) {
            FilterSetParseError(err, ioIndex, "Too many predicates");
            return false;
        }
        group.predicates[group.predicateCount++] = pred;
    }

    return true;
}

static bool FilterParseExpressionStrict(const FilterTokenizeResult& tokens, FilterParseResult& out) {
    out.ok = true;
    out.partial = false;
    out.hasExpression = false;
    out.error[0] = '\0';
    out.andGroupCount = 0;

    if (!tokens.ok) {
        out.ok = false;
        std::snprintf(out.error, sizeof(out.error), "%s", tokens.error);
        return false;
    }

    if (tokens.count == 0) {
        return true;
    }

    std::size_t i = 0;
    if (!FilterParseAndGroup(tokens, i, out.andGroups[0], out)) {
        return false;
    }
    out.andGroupCount = 1;

    while (i < tokens.count) {
        if (tokens.tokens[i].kind != FilterTokenKind::Or) {
            FilterSetParseError(out, i, "Expected or");
            return false;
        }
        ++i;
        if (out.andGroupCount >= std::size(out.andGroups)) {
            FilterSetParseError(out, i, "Too many or-groups");
            return false;
        }
        if (!FilterParseAndGroup(tokens, i, out.andGroups[out.andGroupCount], out)) {
            return false;
        }
        ++out.andGroupCount;
    }

    out.hasExpression = true;
    return true;
}

static bool FilterParseExpressionPartial(const FilterTokenizeResult& tokens, FilterParseResult& out) {
    out.ok = false;
    out.partial = true;
    out.hasExpression = false;
    out.error[0] = '\0';
    out.andGroupCount = 0;

    if (!tokens.ok || tokens.count == 0) {
        return false;
    }

    FilterParseResult scratch = {};
    std::size_t i = 0;

    FilterAndGroup firstGroup = {};
    if (!FilterParseAndGroupPartial(tokens, i, firstGroup, scratch) || firstGroup.predicateCount == 0) {
        return false;
    }
    out.andGroups[0] = firstGroup;
    out.andGroupCount = 1;
    out.hasExpression = true;

    while (i < tokens.count) {
        if (tokens.tokens[i].kind != FilterTokenKind::Or) {
            break;
        }
        const std::size_t orTokenIndex = i;
        ++i;

        FilterAndGroup nextGroup = {};
        if (!FilterParseAndGroupPartial(tokens, i, nextGroup, scratch) || nextGroup.predicateCount == 0) {
            i = orTokenIndex;
            break;
        }
        if (out.andGroupCount >= std::size(out.andGroups)) {
            i = orTokenIndex;
            break;
        }
        out.andGroups[out.andGroupCount++] = nextGroup;
    }

    return true;
}

static bool FilterParseExpression(const FilterTokenizeResult& tokens, FilterParseResult& out) {
    if (FilterParseExpressionStrict(tokens, out)) {
        return true;
    }

    const FilterParseResult strictFailure = out;
    if (FilterParseExpressionPartial(tokens, out)) {
        if (strictFailure.error[0] != '\0') {
            std::snprintf(out.error, sizeof(out.error), "%s", strictFailure.error);
        } else {
            out.error[0] = '\0';
        }
        return true;
    }

    out = strictFailure;
    out.partial = false;
    return false;
}

static bool FilterExpressionUsesField(const FilterParseResult& expr, FilterField field) {
    if (!expr.hasExpression) {
        return false;
    }
    for (std::size_t g = 0; g < expr.andGroupCount; ++g) {
        const FilterAndGroup& group = expr.andGroups[g];
        for (std::size_t p = 0; p < group.predicateCount; ++p) {
            if (group.predicates[p].field == field) {
                return true;
            }
        }
    }
    return false;
}

static char FilterToLowerAscii(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

static bool FilterEqualsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (FilterToLowerAscii(a[i]) != FilterToLowerAscii(b[i])) {
            return false;
        }
    }
    return true;
}

static bool FilterStartsWithIgnoreCase(std::string_view text, std::string_view prefix) {
    if (prefix.size() > text.size()) {
        return false;
    }
    return FilterEqualsIgnoreCase(text.substr(0, prefix.size()), prefix);
}

static bool FilterEndsWithIgnoreCase(std::string_view text, std::string_view suffix) {
    if (suffix.size() > text.size()) {
        return false;
    }
    return FilterEqualsIgnoreCase(text.substr(text.size() - suffix.size()), suffix);
}

static bool FilterContainsIgnoreCase(std::string_view text, std::string_view needle) {
    if (needle.empty()) {
        return true;
    }
    if (needle.size() > text.size()) {
        return false;
    }
    for (std::size_t i = 0; i <= text.size() - needle.size(); ++i) {
        if (FilterEqualsIgnoreCase(text.substr(i, needle.size()), needle)) {
            return true;
        }
    }
    return false;
}

static bool FilterMatchTextOp(FilterOp op, std::string_view fieldText, std::string_view needle) {
    switch (op) {
    case FilterOp::Equals:
        return FilterEqualsIgnoreCase(fieldText, needle);
    case FilterOp::Contains:
        return FilterContainsIgnoreCase(fieldText, needle);
    case FilterOp::StartsWith:
        return FilterStartsWithIgnoreCase(fieldText, needle);
    case FilterOp::EndsWith:
        return FilterEndsWithIgnoreCase(fieldText, needle);
    default:
        return false;
    }
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

static bool FilterMatchDistanceOp(FilterOp op, float distance, double threshold) {
    switch (op) {
    case FilterOp::Less:
        return distance < threshold;
    case FilterOp::Greater:
        return distance > threshold;
    case FilterOp::LessEqual:
        return distance <= threshold;
    case FilterOp::GreaterEqual:
        return distance >= threshold;
    default:
        return false;
    }
}

static bool FilterMatchesPredicate(const ActorTableRow& values, const FilterPredicate& pred) {
    bool matched = false;

    if (pred.op == FilterOp::IsTrue) {
        if (pred.field == FilterField::Child) {
            matched = values.isChild;
        } else {
            matched = false;
        }
    } else if (pred.field == FilterField::Distance) {
        matched = FilterMatchDistanceOp(pred.op, values.distance, pred.numberValue);
    } else if (pred.valueKind == FilterValueKind::Text) {
        std::string_view fieldText = {};
        switch (pred.field) {
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
        std::strncpy(lastTokenizedFilter, filterBuffer, sizeof(lastTokenizedFilter));
        lastTokenizedFilter[sizeof(lastTokenizedFilter) - 1] = '\0';
        FilterTokenize(filterBuffer, tokenizeResult);
        FilterParseExpression(tokenizeResult, parseResult);
        filterUsesTask = FilterExpressionUsesField(parseResult, FilterField::Task);
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
