#pragma once

enum class FilterFieldKind : std::uint8_t {
    Text,
    Bool,
    Number,
};

struct FilterFieldSpec {
    std::uint8_t fieldId = 0;
    FilterFieldKind kind = FilterFieldKind::Text;
    const char* const* aliases = nullptr;
    std::size_t aliasCount = 0;
    bool expensive = false;
};

struct FilterSchema {
    const FilterFieldSpec* fields = nullptr;
    std::size_t fieldCount = 0;
};

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

using FilterRowTextFn = std::string_view (*)(const void* rowContext, std::uint8_t fieldId);
using FilterRowBoolFn = bool (*)(const void* rowContext, std::uint8_t fieldId);
using FilterRowNumberFn = float (*)(const void* rowContext, std::uint8_t fieldId);

struct FilterRowAccess {
    FilterRowTextFn getText = nullptr;
    FilterRowBoolFn getBool = nullptr;
    FilterRowNumberFn getNumber = nullptr;
};

bool FilterTokenize(const char* input, FilterTokenizeResult& result);
bool FilterParseExpression(const FilterTokenizeResult& tokens, FilterParseResult& out, const FilterSchema& schema);
bool FilterExpressionUsesExpensiveField(const FilterParseResult& expr, const FilterSchema& schema);
bool FilterMatchPredicate(const FilterPredicate& pred, const void* rowContext, const FilterSchema& schema,
    const FilterRowAccess& access);
bool FilterMatchesExpression(const FilterParseResult& expr, const void* rowContext, const FilterSchema& schema,
    const FilterRowAccess& access);
float FilterGetLessUpperBound(const FilterParseResult& parseResult, std::uint8_t numberFieldId, float defaultValue);
