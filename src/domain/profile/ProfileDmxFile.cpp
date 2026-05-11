#include "domain/profile/ProfileDmxFile.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace roadproto::domain::profile {
namespace {

std::wstring widenAscii(const std::string& value)
{
    std::wstring result;
    result.reserve(value.size());
    for (const unsigned char ch : value) {
        result.push_back(static_cast<wchar_t>(ch));
    }
    return result;
}

bool isSpace(wchar_t ch)
{
    return ch == static_cast<wchar_t>(0xFEFF) || std::iswspace(static_cast<std::wint_t>(ch)) != 0;
}

std::wstring trim(std::wstring value)
{
    if (value.size() >= 3
        && value[0] == static_cast<wchar_t>(0x00EF)
        && value[1] == static_cast<wchar_t>(0x00BB)
        && value[2] == static_cast<wchar_t>(0x00BF)) {
        value.erase(0, 3);
    }

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](wchar_t ch) { return !isSpace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](wchar_t ch) { return !isSpace(ch); }).base(), value.end());
    return value;
}

bool isCommentLine(const std::wstring& line)
{
    return line.size() >= 2 && line[0] == L'/' && line[1] == L'/';
}

double parseDoubleStrict(const std::wstring& text)
{
    if (text.empty()) {
        throw std::runtime_error("empty number");
    }

    std::size_t parsed = 0;
    const auto value = std::stod(text, &parsed);
    if (parsed != text.size() || !std::isfinite(value)) {
        throw std::runtime_error("invalid number");
    }
    return value;
}

int parseIntStrict(const std::wstring& text)
{
    if (text.empty()) {
        throw std::runtime_error("empty integer");
    }

    std::size_t parsed = 0;
    const auto value = std::stoi(text, &parsed);
    if (parsed != text.size()) {
        throw std::runtime_error("invalid integer");
    }
    return value;
}

ProfileGroundSample parseSample(const std::wstring& stationToken, const std::wstring& elevationToken)
{
    ProfileGroundSample sample;
    sample.rawStationText = stationToken;

    const auto breakChainSeparator = stationToken.find(L'_');
    const auto stationText = stationToken.substr(0, breakChainSeparator);
    sample.station = parseDoubleStrict(stationText);

    if (breakChainSeparator != std::wstring::npos) {
        sample.breakChainIndex = parseIntStrict(stationToken.substr(breakChainSeparator + 1));
    }

    sample.elevation = parseDoubleStrict(elevationToken);
    return sample;
}

} // namespace

ProfileDmxParseResult ProfileDmxFile::parseText(const std::wstring& content)
{
    ProfileDmxParseResult result;

    std::wistringstream input(content);
    std::wstring line;
    while (std::getline(input, line)) {
        const auto trimmed = trim(line);
        if (trimmed.empty() || isCommentLine(trimmed)) {
            continue;
        }

        std::wistringstream fields(trimmed);
        std::wstring stationToken;
        std::wstring elevationToken;
        std::wstring extraToken;
        if (!(fields >> stationToken >> elevationToken) || (fields >> extraToken)) {
            ++result.invalidLineCount;
            continue;
        }

        try {
            result.samples.push_back(parseSample(stationToken, elevationToken));
        } catch (...) {
            ++result.invalidLineCount;
        }
    }

    result.succeeded = result.samples.size() >= 2;
    if (!result.succeeded) {
        result.errorMessage = L"At least two valid DMX ground samples are required.";
    }
    return result;
}

ProfileDmxParseResult ProfileDmxFile::read(const std::wstring& path)
{
    ProfileDmxParseResult result;

    std::ifstream stream{std::filesystem::path(path)};
    if (!stream) {
        result.errorMessage = L"Unable to open DMX file.";
        return result;
    }

    std::ostringstream content;
    content << stream.rdbuf();
    return parseText(widenAscii(content.str()));
}

} // namespace roadproto::domain::profile
