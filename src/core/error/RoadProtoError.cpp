#include "core/error/RoadProtoError.h"

namespace roadproto::core {

bool RoadProtoError::ok() const
{
    return code == ErrorCode::Ok;
}

RoadProtoError success()
{
    return RoadProtoError{ErrorCode::Ok, L""};
}

} // namespace roadproto::core
