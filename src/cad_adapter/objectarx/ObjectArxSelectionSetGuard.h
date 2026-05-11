#pragma once

#include "acedads.h"

namespace roadproto::cad_adapter::objectarx {

class SelectionSetGuard {
public:
    explicit SelectionSetGuard(ads_name selectionSet)
        : active_(true)
    {
        selectionSet_[0] = selectionSet[0];
        selectionSet_[1] = selectionSet[1];
    }

    ~SelectionSetGuard()
    {
        release();
    }

    SelectionSetGuard(const SelectionSetGuard&) = delete;
    SelectionSetGuard& operator=(const SelectionSetGuard&) = delete;

    void release()
    {
        if (active_) {
            acedSSFree(selectionSet_);
            active_ = false;
        }
    }

private:
    ads_name selectionSet_{0, 0};
    bool active_ = false;
};

} // namespace roadproto::cad_adapter::objectarx
