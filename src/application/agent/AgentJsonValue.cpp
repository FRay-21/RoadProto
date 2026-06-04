#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include "application/agent/AgentJsonValue.h"

#include <codecvt>
#include <cstdlib>
#include <cwctype>
#include <locale>
#include <sstream>
#include <utility>

namespace roadproto::application::agent {

bool AgentJsonValue::isObject() const
{
    return type == Type::Object;
}

bool AgentJsonValue::isArray() const
{
    return type == Type::Array;
}

const AgentJsonValue* AgentJsonValue::find(const std::wstring& key) const
{
    if (type != Type::Object) {
        return nullptr;
    }
    const auto item = objectValue.find(key);
    return item == objectValue.end() ? nullptr : &item->second;
}

namespace {

class Parser {
public:
    explicit Parser(std::string text)
        : text_(std::move(text))
    {
    }

    bool parse(AgentJsonValue& value, std::wstring& error)
    {
        skipWhitespace();
        if (!parseValue(value, error)) {
            return false;
        }
        skipWhitespace();
        if (position_ != text_.size()) {
            error = L"Unexpected trailing JSON content.";
            return false;
        }
        return true;
    }

private:
    bool parseValue(AgentJsonValue& value, std::wstring& error);
    bool parseObject(AgentJsonValue& value, std::wstring& error);
    bool parseArray(AgentJsonValue& value, std::wstring& error);
    bool parseString(std::wstring& value, std::wstring& error);
    bool parseNumber(AgentJsonValue& value, std::wstring& error);
    bool consume(char expected);
    void skipWhitespace();

    std::string text_;
    std::size_t position_ = 0;
};

} // namespace

bool Parser::parseValue(AgentJsonValue& value, std::wstring& error)
{
    skipWhitespace();
    if (position_ >= text_.size()) {
        error = L"Unexpected end of JSON.";
        return false;
    }

    const auto ch = text_[position_];
    if (ch == '{') {
        return parseObject(value, error);
    }
    if (ch == '[') {
        return parseArray(value, error);
    }
    if (ch == '"') {
        value.type = AgentJsonValue::Type::String;
        return parseString(value.stringValue, error);
    }
    if (ch == '-' || (ch >= '0' && ch <= '9')) {
        return parseNumber(value, error);
    }
    if (text_.compare(position_, 4, "true") == 0) {
        position_ += 4;
        value.type = AgentJsonValue::Type::Boolean;
        value.booleanValue = true;
        return true;
    }
    if (text_.compare(position_, 5, "false") == 0) {
        position_ += 5;
        value.type = AgentJsonValue::Type::Boolean;
        value.booleanValue = false;
        return true;
    }
    if (text_.compare(position_, 4, "null") == 0) {
        position_ += 4;
        value.type = AgentJsonValue::Type::Null;
        return true;
    }

    error = L"Unexpected JSON token.";
    return false;
}

bool Parser::parseObject(AgentJsonValue& value, std::wstring& error)
{
    if (!consume('{')) {
        error = L"Expected JSON object.";
        return false;
    }

    value.type = AgentJsonValue::Type::Object;
    skipWhitespace();
    if (consume('}')) {
        return true;
    }

    while (position_ < text_.size()) {
        std::wstring key;
        if (!parseString(key, error)) {
            return false;
        }
        skipWhitespace();
        if (!consume(':')) {
            error = L"Expected ':' after JSON object key.";
            return false;
        }

        AgentJsonValue child;
        if (!parseValue(child, error)) {
            return false;
        }
        value.objectValue[key] = child;

        skipWhitespace();
        if (consume('}')) {
            return true;
        }
        if (!consume(',')) {
            error = L"Expected ',' or '}' in JSON object.";
            return false;
        }
        skipWhitespace();
    }

    error = L"Unterminated JSON object.";
    return false;
}

bool Parser::parseArray(AgentJsonValue& value, std::wstring& error)
{
    if (!consume('[')) {
        error = L"Expected JSON array.";
        return false;
    }

    value.type = AgentJsonValue::Type::Array;
    skipWhitespace();
    if (consume(']')) {
        return true;
    }

    while (position_ < text_.size()) {
        AgentJsonValue child;
        if (!parseValue(child, error)) {
            return false;
        }
        value.arrayValue.push_back(child);

        skipWhitespace();
        if (consume(']')) {
            return true;
        }
        if (!consume(',')) {
            error = L"Expected ',' or ']' in JSON array.";
            return false;
        }
        skipWhitespace();
    }

    error = L"Unterminated JSON array.";
    return false;
}

bool Parser::parseString(std::wstring& value, std::wstring& error)
{
    if (!consume('"')) {
        error = L"Expected JSON string.";
        return false;
    }

    std::string utf8;
    while (position_ < text_.size()) {
        const auto ch = text_[position_++];
        if (ch == '"') {
            try {
                std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                value = converter.from_bytes(utf8);
                return true;
            } catch (...) {
                error = L"Invalid UTF-8 in JSON string.";
                return false;
            }
        }
        if (ch == '\\') {
            if (position_ >= text_.size()) {
                error = L"Invalid JSON string escape.";
                return false;
            }
            const auto escaped = text_[position_++];
            switch (escaped) {
            case '"':
            case '\\':
            case '/':
                utf8.push_back(escaped);
                break;
            case 'b':
                utf8.push_back('\b');
                break;
            case 'f':
                utf8.push_back('\f');
                break;
            case 'n':
                utf8.push_back('\n');
                break;
            case 'r':
                utf8.push_back('\r');
                break;
            case 't':
                utf8.push_back('\t');
                break;
            default:
                error = L"Unsupported JSON string escape.";
                return false;
            }
            continue;
        }
        utf8.push_back(ch);
    }

    error = L"Unterminated JSON string.";
    return false;
}

bool Parser::parseNumber(AgentJsonValue& value, std::wstring& error)
{
    const auto start = position_;
    if (text_[position_] == '-') {
        ++position_;
    }
    while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
        ++position_;
    }
    if (position_ < text_.size() && text_[position_] == '.') {
        ++position_;
        while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
            ++position_;
        }
    }
    if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
        ++position_;
        if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) {
            ++position_;
        }
        while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
            ++position_;
        }
    }

    const auto numberText = text_.substr(start, position_ - start);
    char* end = nullptr;
    const auto parsed = std::strtod(numberText.c_str(), &end);
    if (end == numberText.c_str() || *end != '\0') {
        error = L"Invalid JSON number.";
        return false;
    }

    value.type = AgentJsonValue::Type::Number;
    value.numberValue = parsed;
    return true;
}

bool Parser::consume(char expected)
{
    skipWhitespace();
    if (position_ < text_.size() && text_[position_] == expected) {
        ++position_;
        return true;
    }
    return false;
}

void Parser::skipWhitespace()
{
    while (position_ < text_.size()) {
        const auto ch = text_[position_];
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
            return;
        }
        ++position_;
    }
}

bool parseAgentJson(const std::string& text, AgentJsonValue& value, std::wstring& errorMessage)
{
    Parser parser(text);
    return parser.parse(value, errorMessage);
}

} // namespace roadproto::application::agent
