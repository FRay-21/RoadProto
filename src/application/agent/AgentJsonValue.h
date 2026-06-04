#pragma once

#include <map>
#include <string>
#include <vector>

namespace roadproto::application::agent {

class AgentJsonValue {
public:
    enum class Type {
        Null,
        Boolean,
        Number,
        String,
        Object,
        Array
    };

    Type type = Type::Null;
    bool booleanValue = false;
    double numberValue = 0.0;
    std::wstring stringValue;
    std::map<std::wstring, AgentJsonValue> objectValue;
    std::vector<AgentJsonValue> arrayValue;

    bool isObject() const;
    bool isArray() const;
    const AgentJsonValue* find(const std::wstring& key) const;
};

bool parseAgentJson(const std::string& text, AgentJsonValue& value, std::wstring& errorMessage);

} // namespace roadproto::application::agent
