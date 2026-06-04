#include "application/agent/AgentToolRequest.h"

#include <cmath>

namespace roadproto::application::agent {
namespace {

std::wstring stringOrDefault(const AgentJsonValue* value, const std::wstring& fallback)
{
    return value != nullptr && value->type == AgentJsonValue::Type::String
        ? value->stringValue
        : fallback;
}

double numberOrDefault(const AgentJsonValue* value, double fallback)
{
    return value != nullptr && value->type == AgentJsonValue::Type::Number
        ? value->numberValue
        : fallback;
}

bool boolOrDefault(const AgentJsonValue* value, bool fallback)
{
    return value != nullptr && value->type == AgentJsonValue::Type::Boolean
        ? value->booleanValue
        : fallback;
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
    request.tool = stringOrDefault(root.find(L"tool"), L"");
    if (request.tool != L"cross_section.subgrade_template.create") {
        errorMessage = L"Unsupported agent tool: " + request.tool;
        return {};
    }

    request.requestId = stringOrDefault(root.find(L"requestId"), L"");
    request.resultPath = stringOrDefault(root.find(L"resultPath"), L"");

    const auto* arguments = root.find(L"arguments");
    if (arguments == nullptr || !arguments->isObject()) {
        errorMessage = L"Agent tool request arguments must be an object.";
        return {};
    }

    request.arguments.templateName = stringOrDefault(arguments->find(L"templateName"), L"默认路基模板");
    request.arguments.displayScale = numberOrDefault(arguments->find(L"displayScale"), 10.0);
    request.arguments.roadGrade = stringOrDefault(arguments->find(L"roadGrade"), L"Expressway");
    request.arguments.roadCenterlineHandle = stringOrDefault(arguments->find(L"roadCenterlineHandle"), L"");
    request.arguments.componentSource = stringOrDefault(arguments->find(L"componentSource"), L"DefaultByRoadGrade");

    const auto* insertionPoint = arguments->find(L"insertionPoint");
    if (insertionPoint != nullptr && insertionPoint->isObject()) {
        request.arguments.insertionPoint.mode = stringOrDefault(insertionPoint->find(L"mode"), L"PickInCad");
        if (const auto* x = insertionPoint->find(L"x"); x != nullptr && x->type == AgentJsonValue::Type::Number) {
            request.arguments.insertionPoint.x = x->numberValue;
            request.arguments.insertionPoint.hasX = true;
        }
        if (const auto* y = insertionPoint->find(L"y"); y != nullptr && y->type == AgentJsonValue::Type::Number) {
            request.arguments.insertionPoint.y = y->numberValue;
            request.arguments.insertionPoint.hasY = true;
        }
        if (const auto* z = insertionPoint->find(L"z"); z != nullptr && z->type == AgentJsonValue::Type::Number) {
            request.arguments.insertionPoint.z = z->numberValue;
            request.arguments.insertionPoint.hasZ = true;
        }
    }

    const auto* components = arguments->find(L"components");
    if (components != nullptr && components->isArray()) {
        for (const auto& item : components->arrayValue) {
            if (!item.isObject()) {
                errorMessage = L"Agent subgrade component must be an object.";
                return {};
            }
            AgentToolSubgradeComponent component;
            component.side = stringOrDefault(item.find(L"side"), L"Right");
            component.type = stringOrDefault(item.find(L"type"), L"TravelLane");
            if (const auto* width = item.find(L"width"); width != nullptr && width->type == AgentJsonValue::Type::Number) {
                component.width = width->numberValue;
                component.hasWidth = true;
            }
            component.height = numberOrDefault(item.find(L"height"), 0.0);
            component.slopeMode = stringOrDefault(item.find(L"slopeMode"), L"Fixed");
            component.fixedSlope = numberOrDefault(item.find(L"fixedSlope"), 0.0);
            request.arguments.components.push_back(component);
        }
    }

    request.succeeded = true;
    errorMessage.clear();
    return request;
}

} // namespace roadproto::application::agent
