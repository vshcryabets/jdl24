#include "Error.h"

namespace dl24 {

Error Error::None = Error(ErrorCode::Ok, "No error");
Error Error::NotImplemented = Error(ErrorCode::NotImplemented, "Not implemented");
Error Error::Timeout = Error(ErrorCode::Timeout, "Timeout");

}