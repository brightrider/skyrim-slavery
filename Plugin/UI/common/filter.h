#pragma once

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

enum class FilterValueKind : std::uint8_t {
    None,
    Text,
    Number,
};

struct FilterPredicate {
    std::uint8_t field = 0;
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

using FilterParsePredicateFn = bool (*)(const FilterTokenizeResult& tokens, std::size_t startIndex,
    FilterPredicate& outPred, std::size_t& outNextIndex, FilterParseResult& err);

bool FilterTokenize(const char* input, FilterTokenizeResult& result);
bool FilterParseExpression(const FilterTokenizeResult& tokens, FilterParseResult& out,
    FilterParsePredicateFn parsePredicate);
bool FilterExpressionUsesField(const FilterParseResult& expr, std::uint8_t field);

bool FilterIsOpKeyword(std::string_view ident, FilterOp& outOp);
bool FilterMatchTextOp(FilterOp op, std::string_view fieldText, std::string_view needle);
bool FilterMatchFloatOp(FilterOp op, float value, double threshold);

bool FilterStringEqualsIgnoreCase(std::string_view word, const char* literal);
bool FilterTryParseDouble(std::string_view sv, double& out);

void FilterSetParseError(FilterParseResult& result, std::size_t tokenIndex, const char* message);
