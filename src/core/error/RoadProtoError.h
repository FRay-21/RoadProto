#pragma once

#include <string>

namespace roadproto::core {

enum class ErrorCode {
    Ok,
    InvalidCommandDefinition,
    DuplicateCommand,
    InvalidModuleDefinition,
    DuplicateModule,
    ObjectArxRegistrationFailed,
    RelationEntityNotFound
};

struct RoadProtoError {
    ErrorCode code = ErrorCode::Ok;
    std::wstring message;

    bool ok() const;
};

RoadProtoError success();

} // namespace roadproto::core
