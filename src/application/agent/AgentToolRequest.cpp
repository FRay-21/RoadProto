#include "application/agent/AgentToolRequest.h"

#include <cmath>
#include <sstream>

namespace roadproto::application::agent {
namespace {

bool readOptionalString(
    const AgentJsonValue* owner,
    const std::wstring& key,
    const std::wstring& fieldPath,
    std::wstring& target,
    std::wstring& errorMessage)
{
    const auto* value = owner->find(key);
    if (value == nullptr) {
        return true;
    }
    if (value->type != AgentJsonValue::Type::String) {
        errorMessage = fieldPath + L" must be a string.";
        return false;
    }
    target = value->stringValue;
    return true;
}

bool readOptionalString(
    const AgentJsonValue* owner,
    const std::wstring& key,
    const std::wstring& fieldPath,
    std::wstring& target,
    bool& present,
    std::wstring& errorMessage)
{
    const auto* value = owner->find(key);
    if (value == nullptr) {
        present = false;
        return true;
    }
    present = true;
    if (value->type != AgentJsonValue::Type::String) {
        errorMessage = fieldPath + L" must be a string.";
        return false;
    }
    target = value->stringValue;
    return true;
}

bool readOptionalNumber(
    const AgentJsonValue* owner,
    const std::wstring& key,
    const std::wstring& fieldPath,
    double& target,
    std::wstring& errorMessage)
{
    const auto* value = owner->find(key);
    if (value == nullptr) {
        return true;
    }
    if (value->type != AgentJsonValue::Type::Number) {
        errorMessage = fieldPath + L" must be a number.";
        return false;
    }
    target = value->numberValue;
    return true;
}

bool readOptionalNullableNumber(
    const AgentJsonValue* owner,
    const std::wstring& key,
    const std::wstring& fieldPath,
    double& target,
    bool& present,
    std::wstring& errorMessage)
{
    const auto* value = owner->find(key);
    if (value == nullptr || value->type == AgentJsonValue::Type::Null) {
        present = false;
        return true;
    }
    present = true;
    if (value->type != AgentJsonValue::Type::Number) {
        errorMessage = fieldPath + L" must be a number.";
        return false;
    }
    target = value->numberValue;
    return true;
}

bool readRequiredNumber(
    const AgentJsonValue* owner,
    const std::wstring& key,
    const std::wstring& fieldPath,
    double& target,
    std::wstring& errorMessage)
{
    const auto* value = owner->find(key);
    if (value == nullptr) {
        errorMessage = fieldPath + L" is required.";
        return false;
    }
    if (value->type != AgentJsonValue::Type::Number) {
        errorMessage = fieldPath + L" must be a number.";
        return false;
    }
    target = value->numberValue;
    return true;
}

bool readOptionalBoolean(
    const AgentJsonValue* owner,
    const std::wstring& key,
    const std::wstring& fieldPath,
    bool& target,
    std::wstring& errorMessage)
{
    const auto* value = owner->find(key);
    if (value == nullptr) {
        return true;
    }
    if (value->type != AgentJsonValue::Type::Boolean) {
        errorMessage = fieldPath + L" must be a boolean.";
        return false;
    }
    target = value->booleanValue;
    return true;
}

bool readRequiredString(
    const AgentJsonValue* owner,
    const std::wstring& key,
    const std::wstring& fieldPath,
    std::wstring& target,
    std::wstring& errorMessage)
{
    const auto* value = owner->find(key);
    if (value == nullptr) {
        errorMessage = fieldPath + L" is required.";
        return false;
    }
    if (value->type != AgentJsonValue::Type::String) {
        errorMessage = fieldPath + L" must be a string.";
        return false;
    }
    target = value->stringValue;
    return true;
}

std::wstring componentPath(std::size_t index)
{
    std::wostringstream output;
    output << L"arguments.components[" << index << L"]";
    return output.str();
}

bool validateTopLevelFields(const AgentJsonValue& root, std::wstring& errorMessage)
{
    for (const auto& field : root.objectValue) {
        if (field.first != L"tool"
            && field.first != L"requestId"
            && field.first != L"arguments"
            && field.first != L"resultPath") {
            errorMessage = L"Unknown agent tool request field: " + field.first;
            return false;
        }
    }
    return true;
}

bool readOptionalIntColorChannel(
    const AgentJsonValue* owner,
    const std::wstring& key,
    const std::wstring& fieldPath,
    int& target,
    std::wstring& errorMessage)
{
    double channel = static_cast<double>(target);
    if (!readOptionalNumber(owner, key, fieldPath, channel, errorMessage)) {
        return false;
    }
    target = static_cast<int>(channel);
    return true;
}

bool readOptionalColor(
    const AgentJsonValue* owner,
    const std::wstring& path,
    AgentToolColor& target,
    std::wstring& errorMessage)
{
    const auto* color = owner->find(L"color");
    if (color == nullptr) {
        return true;
    }
    if (!color->isObject()) {
        errorMessage = path + L".color must be an object.";
        return false;
    }
    return readOptionalIntColorChannel(color, L"r", path + L".color.r", target.r, errorMessage) &&
        readOptionalIntColorChannel(color, L"g", path + L".color.g", target.g, errorMessage) &&
        readOptionalIntColorChannel(color, L"b", path + L".color.b", target.b, errorMessage);
}

bool readOptionalStationValueTable(
    const AgentJsonValue* owner,
    const std::wstring& key,
    const std::wstring& path,
    std::vector<AgentToolStationValue>& target,
    std::wstring& errorMessage)
{
    const auto* table = owner->find(key);
    if (table == nullptr) {
        return true;
    }
    if (!table->isArray()) {
        errorMessage = path + L" must be an array.";
        return false;
    }
    for (std::size_t i = 0; i < table->arrayValue.size(); ++i) {
        const auto& row = table->arrayValue[i];
        std::wostringstream rowPath;
        rowPath << path << L"[" << i << L"]";
        if (!row.isObject()) {
            errorMessage = rowPath.str() + L" must be an object.";
            return false;
        }

        AgentToolStationValue value;
        if (!readRequiredNumber(&row, L"station", rowPath.str() + L".station", value.station, errorMessage) ||
            !readRequiredNumber(&row, L"value", rowPath.str() + L".value", value.value, errorMessage)) {
            return false;
        }
        target.push_back(value);
    }
    return true;
}

bool readOptionalPavementLayer(
    const AgentJsonValue* owner,
    const std::wstring& path,
    AgentToolPavementLayer& target,
    std::wstring& errorMessage)
{
    const auto* pavementLayer = owner->find(L"pavementLayer");
    if (pavementLayer == nullptr) {
        return true;
    }
    if (!pavementLayer->isObject()) {
        errorMessage = path + L".pavementLayer must be an object.";
        return false;
    }
    return readOptionalBoolean(pavementLayer, L"linked", path + L".pavementLayer.linked", target.linked, errorMessage) &&
        readOptionalString(pavementLayer, L"handle", path + L".pavementLayer.handle", target.handle, errorMessage) &&
        readOptionalString(pavementLayer, L"name", path + L".pavementLayer.name", target.name, errorMessage) &&
        readOptionalNumber(pavementLayer, L"thickness", path + L".pavementLayer.thickness", target.thickness, errorMessage);
}

} // namespace

AgentToolRequest parseAgentToolRequestJson(const std::string& text, std::wstring& errorMessage)
{
    AgentJsonValue root;
    if (!parseAgentJson(text, root, errorMessage)) {
        return {};
    }
    if (!root.isObject()) {
        errorMessage = L"Agent tool request root must be an object.";
        return {};
    }
    if (!validateTopLevelFields(root, errorMessage)) {
        return {};
    }

    AgentToolRequest request;
    if (!readRequiredString(&root, L"tool", L"tool", request.tool, errorMessage)) {
        return {};
    }
    if (request.tool != L"cross_section.subgrade_template.create") {
        errorMessage = L"Unsupported agent tool: " + request.tool;
        return {};
    }

    if (!readOptionalString(&root, L"requestId", L"requestId", request.requestId, errorMessage) ||
        !readOptionalString(&root, L"resultPath", L"resultPath", request.resultPath, errorMessage)) {
        return {};
    }

    const auto* arguments = root.find(L"arguments");
    if (arguments == nullptr || !arguments->isObject()) {
        errorMessage = L"Agent tool request arguments must be an object.";
        return {};
    }

    if (!readOptionalString(arguments, L"templateName", L"arguments.templateName", request.arguments.templateName, errorMessage) ||
        !readOptionalNumber(arguments, L"displayScale", L"arguments.displayScale", request.arguments.displayScale, errorMessage) ||
        !readOptionalString(arguments, L"roadGrade", L"arguments.roadGrade", request.arguments.roadGrade, errorMessage) ||
        !readOptionalString(arguments, L"roadCenterlineHandle", L"arguments.roadCenterlineHandle", request.arguments.roadCenterlineHandle, errorMessage) ||
        !readOptionalString(arguments, L"componentSource", L"arguments.componentSource", request.arguments.componentSource, errorMessage)) {
        return {};
    }

    const auto* insertionPoint = arguments->find(L"insertionPoint");
    if (insertionPoint != nullptr) {
        if (!insertionPoint->isObject()) {
            errorMessage = L"arguments.insertionPoint must be an object.";
            return {};
        }
        if (!readOptionalString(insertionPoint, L"mode", L"arguments.insertionPoint.mode", request.arguments.insertionPoint.mode, errorMessage)) {
            return {};
        }
        if (!readOptionalNullableNumber(
                insertionPoint,
                L"x",
                L"arguments.insertionPoint.x",
                request.arguments.insertionPoint.x,
                request.arguments.insertionPoint.hasX,
                errorMessage) ||
            !readOptionalNullableNumber(
                insertionPoint,
                L"y",
                L"arguments.insertionPoint.y",
                request.arguments.insertionPoint.y,
                request.arguments.insertionPoint.hasY,
                errorMessage) ||
            !readOptionalNullableNumber(
                insertionPoint,
                L"z",
                L"arguments.insertionPoint.z",
                request.arguments.insertionPoint.z,
                request.arguments.insertionPoint.hasZ,
                errorMessage)) {
            return {};
        }
    }

    const auto* components = arguments->find(L"components");
    if (components != nullptr) {
        if (!components->isArray()) {
            errorMessage = L"arguments.components must be an array.";
            return {};
        }
        for (std::size_t i = 0; i < components->arrayValue.size(); ++i) {
            const auto& item = components->arrayValue[i];
            const auto path = componentPath(i);
            if (!item.isObject()) {
                errorMessage = path + L" must be an object.";
                return {};
            }
            AgentToolSubgradeComponent component;
            if (!readOptionalString(&item, L"side", path + L".side", component.side, component.hasSide, errorMessage) ||
                !readOptionalString(&item, L"type", path + L".type", component.type, component.hasType, errorMessage)) {
                return {};
            }
            if (const auto* width = item.find(L"width"); width != nullptr) {
                if (width->type != AgentJsonValue::Type::Number) {
                    errorMessage = path + L".width must be a number.";
                    return {};
                }
                component.width = width->numberValue;
                component.hasWidth = true;
            }
            if (!readOptionalNumber(&item, L"height", path + L".height", component.height, errorMessage) ||
                !readOptionalString(&item, L"slopeMode", path + L".slopeMode", component.slopeMode, component.hasSlopeMode, errorMessage) ||
                !readOptionalNumber(&item, L"fixedSlope", path + L".fixedSlope", component.fixedSlope, errorMessage) ||
                !readOptionalColor(&item, path, component.color, errorMessage) ||
                !readOptionalStationValueTable(&item, L"wideningTable", path + L".wideningTable", component.wideningTable, errorMessage) ||
                !readOptionalStationValueTable(&item, L"variableSlopeTable", path + L".variableSlopeTable", component.variableSlopeTable, errorMessage) ||
                !readOptionalPavementLayer(&item, path, component.pavementLayer, errorMessage)) {
                return {};
            }
            request.arguments.components.push_back(component);
        }
    }

    request.succeeded = true;
    errorMessage.clear();
    return request;
}

} // namespace roadproto::application::agent
