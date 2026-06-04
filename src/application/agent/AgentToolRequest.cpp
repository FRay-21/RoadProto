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
        if (const auto* x = insertionPoint->find(L"x"); x != nullptr) {
            if (x->type != AgentJsonValue::Type::Number) {
                errorMessage = L"arguments.insertionPoint.x must be a number.";
                return {};
            }
            request.arguments.insertionPoint.x = x->numberValue;
            request.arguments.insertionPoint.hasX = true;
        }
        if (const auto* y = insertionPoint->find(L"y"); y != nullptr) {
            if (y->type != AgentJsonValue::Type::Number) {
                errorMessage = L"arguments.insertionPoint.y must be a number.";
                return {};
            }
            request.arguments.insertionPoint.y = y->numberValue;
            request.arguments.insertionPoint.hasY = true;
        }
        if (const auto* z = insertionPoint->find(L"z"); z != nullptr) {
            if (z->type != AgentJsonValue::Type::Number) {
                errorMessage = L"arguments.insertionPoint.z must be a number.";
                return {};
            }
            request.arguments.insertionPoint.z = z->numberValue;
            request.arguments.insertionPoint.hasZ = true;
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
            if (!readOptionalString(&item, L"side", path + L".side", component.side, errorMessage) ||
                !readOptionalString(&item, L"type", path + L".type", component.type, errorMessage)) {
                return {};
            }
            if (component.side.empty()) {
                component.side = L"Right";
            }
            if (component.type.empty()) {
                component.type = L"TravelLane";
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
                !readOptionalString(&item, L"slopeMode", path + L".slopeMode", component.slopeMode, errorMessage) ||
                !readOptionalNumber(&item, L"fixedSlope", path + L".fixedSlope", component.fixedSlope, errorMessage)) {
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
