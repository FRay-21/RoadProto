#pragma once

#include <string>
#include <vector>

namespace roadproto::ui {

struct RibbonButtonDefinition {
    std::wstring commandName;
    std::wstring label;
    std::wstring description;
    std::wstring iconPath;
    std::wstring businessDocPath;
};

struct RibbonPanelDefinition {
    std::wstring moduleCode;
    std::wstring title;
    std::vector<RibbonButtonDefinition> buttons;
};

struct RibbonTabDefinition {
    std::wstring title;
    std::vector<RibbonPanelDefinition> panels;
};

class RibbonModel {
public:
    explicit RibbonModel(std::wstring tabTitle = L"RoadProto");

    void clear();
    void setTabTitle(std::wstring tabTitle);
    void ensurePanel(std::wstring moduleCode, std::wstring title);
    bool addButton(const std::wstring& moduleCode, RibbonButtonDefinition button);

    const RibbonTabDefinition& tab() const;

private:
    RibbonPanelDefinition* findPanel(const std::wstring& moduleCode);

    RibbonTabDefinition tab_;
};

} // namespace roadproto::ui
