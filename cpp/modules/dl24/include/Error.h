#pragma once

#include <cstdint>
#include <exception>
#include <string>

namespace dl24 {
enum class ErrorCode : uint8_t {
    Ok = 0,
    InvalidCommand = 1,
    InvalidParameter = 2,
    InvalidState = 3,
    UnknownError = 4,
    NotImplemented = 5,
    AlreadyOpen = 6,
    OpenFailed = 7,
    NotOpen = 8,
    WriteFailed = 9,
    Timeout = 10,
};

class Error: public std::exception {
private:
    ErrorCode code;
    std::string message;
public:
    Error(
        ErrorCode code, 
        const std::string& message
    ) : code(code), message(message) {}
    const char* what() const noexcept override {
        return message.c_str();
    }
    ErrorCode getCode() const noexcept {
        return code;
    }
    bool isSuccess() const noexcept {
        return code == ErrorCode::Ok;
    }
    static Error None;
    static Error NotImplemented;
    static Error Timeout;
};

}
