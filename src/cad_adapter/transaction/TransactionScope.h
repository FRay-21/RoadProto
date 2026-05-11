#pragma once

namespace roadproto::cad_adapter {

class TransactionScope {
public:
    virtual ~TransactionScope() = default;
    virtual bool commit() = 0;
    virtual void abort() = 0;
};

} // namespace roadproto::cad_adapter
