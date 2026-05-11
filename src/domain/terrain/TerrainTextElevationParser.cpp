#include "domain/terrain/TerrainTextElevationParser.h"

#include <algorithm>
#include <cwctype>
#include <regex>
#include <string>
#include <vector>

namespace roadproto::domain::terrain {

namespace {

std::wstring trim(std::wstring value)
{
    const auto notSpace = [](wchar_t ch) {
        return std::iswspace(ch) == 0;
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::optional<double> parseNumber(const std::wstring& value)
{
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(value, &used);
        if (used == value.size()) {
            return parsed;
        }
    } catch (...) {
    }

    return std::nullopt;
}

std::optional<double> parseNumberAfterTag(const std::wstring& text, const std::wstring& tag)
{
    const auto tagPosition = text.find(tag);
    if (tagPosition == std::wstring::npos) {
        return std::nullopt;
    }

    const auto rest = text.substr(tagPosition + tag.size());
    static const std::wregex numberAfterTagPattern(LR"(^\s*[:=]?\s*([+-]?\d+(?:\.\d+)?))");
    std::wsmatch match;
    if (std::regex_search(rest, match, numberAfterTagPattern) && match.size() > 1) {
        return parseNumber(match[1].str());
    }

    return std::nullopt;
}

} // namespace

std::optional<double> TerrainTextElevationParser::tryParse(const std::wstring& text) const
{
    const auto trimmed = trim(text);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    static const std::wregex asciiTaggedPattern(
        LR"((?:^|[^\w])(?:H|EL|ELEV|Z|h|el|elev|z)\s*[:=]?\s*([+-]?\d+(?:\.\d+)?))",
        std::regex_constants::icase);

    std::wsmatch match;
    if (std::regex_search(trimmed, match, asciiTaggedPattern) && match.size() > 1) {
        return parseNumber(match[1].str());
    }

    const std::vector<std::wstring> chineseTags{
        L"\u8bbe\u8ba1\u9ad8\u7a0b",
        L"\u9ad8\u7a0b",
        L"\u6807\u9ad8"};
    for (const auto& tag : chineseTags) {
        if (const auto value = parseNumberAfterTag(trimmed, tag)) {
            return value;
        }
    }

    static const std::wregex numberPattern(LR"([+-]?\d+(?:\.\d+)?)");
    std::vector<std::wstring> numbers;
    for (auto it = std::wsregex_iterator(trimmed.begin(), trimmed.end(), numberPattern);
         it != std::wsregex_iterator();
         ++it) {
        numbers.push_back((*it).str());
    }

    if (numbers.size() == 1 && trim(numbers.front()) == trimmed) {
        return parseNumber(numbers.front());
    }

    return std::nullopt;
}

} // namespace roadproto::domain::terrain
