#include "ui/ribbon/RibbonModel.h"

#include <algorithm>
#include <utility>

namespace roadproto::ui {

RibbonModel::RibbonModel(std::wstring tabTitle)
{
    tab_.title = std::move(tabTitle);
}

void RibbonModel::clear()
{
    tab_.panels.clear();
}

void RibbonModel::setTabTitle(std::wstring tabTitle)
{
    tab_.title = std::move(tabTitle);
}

void RibbonModel::ensurePanel(std::wstring moduleCode, std::wstring title)
{
    if (findPanel(moduleCode) != nullptr) {
        return;
    }

    tab_.panels.push_back(RibbonPanelDefinition{std::move(moduleCode), std::move(title), {}});
}

bool RibbonModel::addButton(const std::wstring& moduleCode, RibbonButtonDefinition button)
{
    auto* panel = findPanel(moduleCode);
    if (panel == nullptr) {
        return false;
    }

    panel->buttons.push_back(std::move(button));
    return true;
}

const RibbonTabDefinition& RibbonModel::tab() const
{
    return tab_;
}

RibbonPanelDefinition* RibbonModel::findPanel(const std::wstring& moduleCode)
{
    const auto it = std::find_if(tab_.panels.begin(), tab_.panels.end(), [&](const RibbonPanelDefinition& panel) {
        return panel.moduleCode == moduleCode;
    });

    if (it == tab_.panels.end()) {
        return nullptr;
    }

    return &(*it);
}

} // namespace roadproto::ui
