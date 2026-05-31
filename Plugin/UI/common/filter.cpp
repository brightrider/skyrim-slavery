#include "filter.h"

static bool FilterMatchTextOp(FilterOp op, std::string_view fieldText, std::string_view needle);
static bool FilterMatchFloatOp(FilterOp op, float value, double threshold);

static bool FilterCharEqualsIgnoreCase(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

static bool FilterSchemaValid(const FilterSchema& schema) {
    return schema.fieldCount == 0 || schema.fields != nullptr;
}

static bool FilterStringEqualsIgnoreCase(std::string_view word, const char* literal) {
    if (literal == nullptr) {
        return false;
    }
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

static const FilterFieldSpec* FilterFindFieldSpec(std::string_view ident, const FilterSchema& schema) {
    if (!FilterSchemaValid(schema)) {
        return nullptr;
    }
    for (std::size_t s = 0; s < schema.fieldCount; ++s) {
        const FilterFieldSpec& spec = schema.fields[s];
        if (spec.aliases == nullptr) {
            continue;
        }
        for (std::size_t a = 0; a < spec.aliasCount; ++a) {
            if (FilterStringEqualsIgnoreCase(ident, spec.aliases[a])) {
                return &spec;
            }
        }
    }
    return nullptr;
}

static const FilterFieldSpec* FilterFindFieldSpecById(std::uint8_t fieldId, const FilterSchema& schema) {
    if (!FilterSchemaValid(schema)) {
        return nullptr;
    }
    for (std::size_t s = 0; s < schema.fieldCount; ++s) {
        if (schema.fields[s].fieldId == fieldId) {
            return &schema.fields[s];
        }
    }
    return nullptr;
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

bool FilterTokenize(const char* input, FilterTokenizeResult& result) {
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
    result.partial = false;
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

static bool FilterIsNumericComparisonOp(FilterOp op) {
    return op == FilterOp::Less || op == FilterOp::Greater || op == FilterOp::LessEqual || op == FilterOp::GreaterEqual;
}

static bool FilterIsTextComparisonOp(FilterOp op) {
    return op == FilterOp::Equals || op == FilterOp::Contains || op == FilterOp::StartsWith || op == FilterOp::EndsWith;
}

static bool FilterParseComparisonOperator(const FilterTokenizeResult& tokens, std::size_t& ioIndex, FilterOp& outOp,
    FilterParseResult& err) {
    if (ioIndex >= tokens.count) {
        FilterSetParseError(err, ioIndex, "Expected operator");
        return false;
    }

    const FilterToken& opTok = tokens.tokens[ioIndex];
    if (opTok.kind == FilterTokenKind::Equals) {
        outOp = FilterOp::Equals;
        ++ioIndex;
        return true;
    }
    if (opTok.kind == FilterTokenKind::Less) {
        outOp = FilterOp::Less;
        ++ioIndex;
        return true;
    }
    if (opTok.kind == FilterTokenKind::Greater) {
        outOp = FilterOp::Greater;
        ++ioIndex;
        return true;
    }
    if (opTok.kind == FilterTokenKind::LessEqual) {
        outOp = FilterOp::LessEqual;
        ++ioIndex;
        return true;
    }
    if (opTok.kind == FilterTokenKind::GreaterEqual) {
        outOp = FilterOp::GreaterEqual;
        ++ioIndex;
        return true;
    }
    if (opTok.kind == FilterTokenKind::Identifier) {
        if (!FilterIsOpKeyword(opTok.lexeme, outOp)) {
            FilterSetParseError(err, ioIndex, "Unknown operator");
            return false;
        }
        ++ioIndex;
        return true;
    }

    FilterSetParseError(err, ioIndex, "Expected operator");
    return false;
}

static bool FilterParsePredicateWithSchema(const FilterTokenizeResult& tokens, std::size_t startIndex, FilterPredicate& outPred,
    std::size_t& outNextIndex, FilterParseResult& err, const FilterSchema& schema) {
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

    if (i >= tokens.count || tokens.tokens[i].kind != FilterTokenKind::Identifier) {
        FilterSetParseError(err, i, "Expected identifier");
        return false;
    }

    const FilterFieldSpec* const fieldSpec = FilterFindFieldSpec(tokens.tokens[i].lexeme, schema);
    if (fieldSpec == nullptr) {
        FilterSetParseError(err, i, "Unknown field");
        return false;
    }
    ++i;

    if (fieldSpec->kind == FilterFieldKind::Bool) {
        outPred.field = fieldSpec->fieldId;
        outPred.op = FilterOp::IsTrue;
        outPred.negated = negated;
        outPred.valueKind = FilterValueKind::None;
        outNextIndex = i;
        return true;
    }

    FilterOp op = FilterOp::Equals;
    if (!FilterParseComparisonOperator(tokens, i, op, err)) {
        return false;
    }

    if (i >= tokens.count) {
        FilterSetParseError(err, i, "Expected value");
        return false;
    }

    if (fieldSpec->kind == FilterFieldKind::Number) {
        if (!FilterIsNumericComparisonOp(op)) {
            FilterSetParseError(err, i - 1, "Invalid operator for number field");
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

        outPred.field = fieldSpec->fieldId;
        outPred.op = op;
        outPred.negated = negated;
        outPred.valueKind = FilterValueKind::Number;
        outPred.numberValue = value;
        outNextIndex = i;
        return true;
    }

    if (!FilterIsTextComparisonOp(op)) {
        FilterSetParseError(err, i - 1, "Invalid operator for text field");
        return false;
    }
    if (tokens.tokens[i].kind != FilterTokenKind::Identifier && tokens.tokens[i].kind != FilterTokenKind::String &&
        tokens.tokens[i].kind != FilterTokenKind::Number) {
        FilterSetParseError(err, i, "Expected text");
        return false;
    }
    const std::string_view valueLex = tokens.tokens[i].lexeme;
    ++i;

    outPred.field = fieldSpec->fieldId;
    outPred.op = op;
    outPred.negated = negated;
    outPred.valueKind = FilterValueKind::Text;
    outPred.textValue = valueLex;
    outNextIndex = i;
    return true;
}

static bool FilterParseAndGroup(const FilterTokenizeResult& tokens, std::size_t& ioIndex, FilterAndGroup& group,
    FilterParseResult& err, const FilterSchema& schema) {
    group.predicateCount = 0;

    FilterPredicate pred = {};
    if (!FilterParsePredicateWithSchema(tokens, ioIndex, pred, ioIndex, err, schema)) {
        return false;
    }
    if (group.predicateCount >= std::size(group.predicates)) {
        FilterSetParseError(err, ioIndex, "Too many predicates");
        return false;
    }
    group.predicates[group.predicateCount++] = pred;

    while (ioIndex < tokens.count && tokens.tokens[ioIndex].kind == FilterTokenKind::And) {
        ++ioIndex;
        if (!FilterParsePredicateWithSchema(tokens, ioIndex, pred, ioIndex, err, schema)) {
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
    FilterParseResult& err, const FilterSchema& schema) {
    group.predicateCount = 0;

    FilterPredicate pred = {};
    if (!FilterParsePredicateWithSchema(tokens, ioIndex, pred, ioIndex, err, schema)) {
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
        if (!FilterParsePredicateWithSchema(tokens, ioIndex, pred, ioIndex, err, schema)) {
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

static bool FilterParseExpressionStrict(const FilterTokenizeResult& tokens, FilterParseResult& out, const FilterSchema& schema) {
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
    if (!FilterParseAndGroup(tokens, i, out.andGroups[0], out, schema)) {
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
        if (!FilterParseAndGroup(tokens, i, out.andGroups[out.andGroupCount], out, schema)) {
            return false;
        }
        ++out.andGroupCount;
    }

    out.hasExpression = true;
    return true;
}

static bool FilterParseExpressionPartial(const FilterTokenizeResult& tokens, FilterParseResult& out, const FilterSchema& schema) {
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
    if (!FilterParseAndGroupPartial(tokens, i, firstGroup, scratch, schema) || firstGroup.predicateCount == 0) {
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
        if (!FilterParseAndGroupPartial(tokens, i, nextGroup, scratch, schema) || nextGroup.predicateCount == 0) {
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

bool FilterParseExpression(const FilterTokenizeResult& tokens, FilterParseResult& out, const FilterSchema& schema) {
    if (!FilterSchemaValid(schema)) {
        out.ok = false;
        out.partial = false;
        out.hasExpression = false;
        out.andGroupCount = 0;
        std::snprintf(out.error, sizeof(out.error), "Invalid filter schema");
        return false;
    }

    if (FilterParseExpressionStrict(tokens, out, schema)) {
        return true;
    }

    const FilterParseResult strictFailure = out;
    if (FilterParseExpressionPartial(tokens, out, schema)) {
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

bool FilterExpressionUsesExpensiveField(const FilterParseResult& expr, const FilterSchema& schema) {
    if (!expr.hasExpression || !FilterSchemaValid(schema)) {
        return false;
    }
    for (std::size_t g = 0; g < expr.andGroupCount; ++g) {
        const FilterAndGroup& group = expr.andGroups[g];
        for (std::size_t p = 0; p < group.predicateCount; ++p) {
            const FilterFieldSpec* const fieldSpec = FilterFindFieldSpecById(group.predicates[p].field, schema);
            if (fieldSpec != nullptr && fieldSpec->expensive) {
                return true;
            }
        }
    }
    return false;
}

bool FilterMatchPredicate(const FilterPredicate& pred, const void* rowContext, const FilterSchema& schema,
    const FilterRowAccess& access) {
    if (access.getText == nullptr || access.getBool == nullptr || access.getNumber == nullptr) {
        return pred.negated;
    }

    const FilterFieldSpec* const fieldSpec = FilterFindFieldSpecById(pred.field, schema);
    if (fieldSpec == nullptr) {
        return pred.negated;
    }

    bool matched = false;

    switch (fieldSpec->kind) {
    case FilterFieldKind::Bool:
        if (pred.op == FilterOp::IsTrue) {
            matched = access.getBool(rowContext, pred.field);
        }
        break;
    case FilterFieldKind::Number:
        matched = FilterMatchFloatOp(pred.op, access.getNumber(rowContext, pred.field), pred.numberValue);
        break;
    case FilterFieldKind::Text:
        if (pred.valueKind == FilterValueKind::Text) {
            matched = FilterMatchTextOp(pred.op, access.getText(rowContext, pred.field), pred.textValue);
        }
        break;
    }

    return pred.negated ? !matched : matched;
}

static bool FilterMatchesAndGroup(const FilterAndGroup& group, const void* rowContext, const FilterSchema& schema,
    const FilterRowAccess& access) {
    for (std::size_t i = 0; i < group.predicateCount; ++i) {
        if (!FilterMatchPredicate(group.predicates[i], rowContext, schema, access)) {
            return false;
        }
    }
    return true;
}

bool FilterMatchesExpression(const FilterParseResult& expr, const void* rowContext, const FilterSchema& schema,
    const FilterRowAccess& access) {
    if (!expr.hasExpression) {
        return true;
    }
    if (!FilterSchemaValid(schema)) {
        return false;
    }
    for (std::size_t g = 0; g < expr.andGroupCount; ++g) {
        if (FilterMatchesAndGroup(expr.andGroups[g], rowContext, schema, access)) {
            return true;
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

static bool FilterMatchFloatOp(FilterOp op, float value, double threshold) {
    switch (op) {
    case FilterOp::Less:
        return value < threshold;
    case FilterOp::Greater:
        return value > threshold;
    case FilterOp::LessEqual:
        return value <= threshold;
    case FilterOp::GreaterEqual:
        return value >= threshold;
    default:
        return false;
    }
}
