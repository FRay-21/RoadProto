#pragma once

#include "application/agent/AgentJsonValue.h"

#include <string>
#include <vector>

namespace roadproto::application::agent {

struct AgentToolPoint {
    std::wstring mode = L"PickInCad";
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool hasX = false;
    bool hasY = false;
    bool hasZ = false;
};

struct AgentToolStationValue {
    double station = 0.0;
    double value = 0.0;
};

struct AgentToolColor {
    int r = -1;
    int g = -1;
    int b = -1;
};

struct AgentToolPavementLayer {
    bool linked = false;
    std::wstring handle;
    std::wstring name;
    double thickness = 0.0;
};

struct AgentToolSubgradeComponent {
    std::wstring side;
    bool hasSide = false;
    std::wstring type;
    bool hasType = false;
    double width = 0.0;
    bool hasWidth = false;
    double height = 0.0;
    std::wstring slopeMode = L"Fixed";
    bool hasSlopeMode = false;
    double fixedSlope = 0.0;
    AgentToolColor color;
    std::vector<AgentToolStationValue> wideningTable;
    std::vector<AgentToolStationValue> variableSlopeTable;
    AgentToolPavementLayer pavementLayer;
};

struct AgentSubgradeTemplateCreateArguments {
    std::wstring templateName = L"默认路基模板";
    double displayScale = 10.0;
    std::wstring roadGrade = L"Expressway";
    std::wstring roadCenterlineHandle;
    AgentToolPoint insertionPoint;
    std::wstring componentSource = L"DefaultByRoadGrade";
    std::vector<AgentToolSubgradeComponent> components;
};

struct AgentToolRequest {
    bool succeeded = false;
    std::wstring tool;
    std::wstring requestId;
    std::wstring resultPath;
    AgentSubgradeTemplateCreateArguments arguments;
};

AgentToolRequest parseAgentToolRequestJson(const std::string& text, std::wstring& errorMessage);

} // namespace roadproto::application::agent
