#include "ControllerImpl.h"

#include <iostream>
#include <iomanip>
#include <sstream>

namespace dl24 {

// DL24 command codes (see Java CommandsEnum).
enum class CommandPX100 : uint8_t {
    StartStop = 0x01,
    SetCurrent = 0x02,
    SetCutoffVoltage = 0x03,
    SetTImeout = 0x04,
    ResetCounters = 0x05,
};

enum class CommandAtorch : uint8_t {
    ResetWh = 0x01,
    ResetAh = 0x02,
    ResetTime = 0x03,
    ResetAll = 0x05,
    SetupButton = 0x31,
    EnterButton = 0x32,
    PlusButton = 0x33,
    MinusButton = 0x34,
};

constexpr int VOLTAGE_SCALE = 100;

constexpr uint8_t cmd(CommandAtorch c) {
    return static_cast<uint8_t>(c);
}
constexpr uint8_t cmd(CommandPX100 c) {
    return static_cast<uint8_t>(c);
}

ControllerImpl::ControllerImpl(Source& source)
    : Controller(source) {}

Error ControllerImpl::connect() {
    source.setListener(this);
    collector_.clear();
    Source::UartConfig config;  // defaults: 9600 8N1
    auto result = source.open(config);
    if (!result.isSuccess()) {
        source.setListener(nullptr);
        return result;
    }
    return Error::None;
}

Error ControllerImpl::disconnect() {
    auto result = source.close();
    source.setListener(nullptr);
    return result;
}

Error ControllerImpl::setCurrent(float current) {
    if (current < 0.0f) {
        return Error(ErrorCode::InvalidParameter, "Current must be positive");
    }
    uint8_t amperes = static_cast<uint8_t>(current);
    uint8_t milliamperes = static_cast<uint8_t>((current - amperes) * 100);
    // PX100
    uint8_t command[] = {
        0xB1, 0xB2,  // header
        cmd(CommandPX100::SetCurrent),
        amperes,
        milliamperes,
        0xB6,
    };
    auto answer = sendPX100CommandAndWait(command, sizeof(command));
    if (!answer.has_value()) {
        sendDebugMessage(DebugListener::Level::Answer, "Failed to send SetCurrent command");
        return answer.error();
    }
    // TODO: analyze answer (verify echoed status / checksum).
    return Error::None;
}

Error ControllerImpl::setVoltage(float voltage) {
    uint8_t command[] = {
        0xFF, 0x55,  // header
        0x11, 0x02,  // Master-Slave
        cmd(CommandPX100::SetCutoffVoltage),
        0x00, 0x00, 0x00, 0x00,  // value
        0x00,                    // checksum
    };
    int32_t intValue = static_cast<int32_t>(voltage * VOLTAGE_SCALE);
    command[5] = static_cast<uint8_t>((intValue >> 24) & 0xFF);
    command[6] = static_cast<uint8_t>((intValue >> 16) & 0xFF);
    command[7] = static_cast<uint8_t>((intValue >> 8) & 0xFF);
    command[8] = static_cast<uint8_t>((intValue >> 0) & 0xFF);
    command[9] = calculateChecksum(command, sizeof(command));
    auto answer = sendCommandAndWait(command, sizeof(command));
    if (!answer.has_value()) {
        return answer.error();
    }
    // TODO: analyze answer (verify echoed status / checksum).
    return Error::None;
}

Error ControllerImpl::setTimer(int32_t timer) {
    (void) timer;
    return Error::NotImplemented;
}

Error ControllerImpl::start() {
    return Error::NotImplemented;
}

Error ControllerImpl::stop() {
        uint8_t command[] = {
        0xFF, 0x55,  // header
        static_cast<uint8_t>(MessageType::MasterSlave), // Master-Slave
        0x02,
        0x31,
        0x80, 0x80, 0x80, 0x80,  // value
        0x00,                    // checksum
    };
    command[9] = calculateChecksum(command, sizeof(command));
    auto answer = sendCommandAndWait(command, sizeof(command));
    if (!answer.has_value()) {
        return answer.error();
    }
    return Error::None;
}

Error ControllerImpl::resetCounters() {
    uint8_t command[] = {
        0xFF, 0x55,  // header
        static_cast<uint8_t>(MessageType::MasterSlave), // Master-Slave
        0x02,
        cmd(CommandAtorch::ResetAll),
        0x00, 0x00, 0x00, 0x00,  // value
        0x00,                    // checksum
    };
    command[9] = calculateChecksum(command, sizeof(command));
    auto answer = sendCommandAndWait(command, sizeof(command));
    if (!answer.has_value()) {
        return answer.error();
    }
    return Error::None;
}

Error ControllerImpl::sendCommand(const uint8_t* command, BufferSize_t size) {
    if (debugListener_) {
        std::stringstream ss;
        ss << "Sending command of length " << size << ": " ;
        for (std::size_t i = 0; i < size; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(command[i]) << " ";
        }
        sendDebugMessage(DebugListener::Level::Raw, ss.str());
    }
    return source.write(command, size);
}

std::expected<std::vector<uint8_t>, Error> 
ControllerImpl::sendCommandAndWait(
        const uint8_t* command, 
        BufferSize_t size) {
    // Serialize whole round-trips so only one command is in flight at a time.
    std::lock_guard<std::mutex> commandGuard(commandMutex_);

    std::unique_lock<std::mutex> lock(stateMutex_);
    answerReceived_ = false;
    waitingForAnswer_ = true;
    lastAnswer_.clear();

    // Send while holding stateMutex_: cv.wait_for() below releases it
    // atomically, so an answer arriving on the worker thread can't slip in
    // between the send and the wait (no lost wakeup).
    Error err = sendCommand(command, size);
    if (!err.isSuccess()) {
        waitingForAnswer_ = false;
        return std::unexpected(err);
    }

    bool ok = responseCv_.wait_for(lock, kResponseTimeout,
                                   [this] { return answerReceived_; });
    waitingForAnswer_ = false;
    if (!ok) {
        return std::unexpected(Error::Timeout);  // timed out
    }
    if (!replyError_.isSuccess()) {
        return std::unexpected(replyError_);
    }
    return std::expected<std::vector<uint8_t>, Error>(std::move(lastAnswer_));
}

std::expected<std::vector<uint8_t>, Error> 
ControllerImpl::sendPX100CommandAndWait(
        const uint8_t* command, 
        BufferSize_t size) {
    // Serialize whole round-trips so only one command is in flight at a time.
    std::lock_guard<std::mutex> commandGuard(commandMutex_);

    std::unique_lock<std::mutex> lock(stateMutex_);
    answerPx100Received_ = false;
    waitingForPx100Answer_ = true;
    lastAnswer_.clear();

    // Send while holding stateMutex_: cv.wait_for() below releases it
    // atomically, so an answer arriving on the worker thread can't slip in
    // between the send and the wait (no lost wakeup).
    Error err = sendCommand(command, size);
    if (!err.isSuccess()) {
        waitingForPx100Answer_ = false;
        return std::unexpected(err);
    }

    bool ok = responseCv_.wait_for(lock, kResponseTimeout,
                                   [this] { return answerPx100Received_; });
    waitingForPx100Answer_ = false;
    if (!ok) {
        return std::unexpected(Error::Timeout);  // timed out
    }
    if (!replyError_.isSuccess()) {
        return std::unexpected(replyError_);
    }
    return std::expected<std::vector<uint8_t>, Error>(std::move(lastAnswer_));
}

void ControllerImpl::onDataReceived(const uint8_t* data, BufferSize_t size) {
    if (size == 1 && data[0] == PX100_ACK && waitingForPx100Answer_) {
        sendDebugMessage(DebugListener::Level::Raw, "Received PX100 ACK");
        answerPx100Received_ = true;
        waitingForPx100Answer_ = false;
        replyError_ = Error::None;
        responseCv_.notify_one();
        return;
    }
    // Add everything to the collector as-is, without any checks.
    collector_.insert(collector_.end(), data, data + size);
    parseCollector();
}

void ControllerImpl::parseCollector() {
    while (true) {
        // 1. Align the collector to the first answer start (FF 55).
        std::size_t first = findFrameStart(0);
        if (first == collector_.size()) {
            // No frame start yet. Drop leading garbage, but keep a trailing FF
            // in case the matching 55 arrives in the next chunk.
            if (!collector_.empty() && collector_.back() == MAGIC_B1) {
                collector_.erase(collector_.begin(), collector_.end() - 1);
            } else {
                collector_.clear();
            }
            return;
        }
        if (first > 0) {
            // Discard bytes before the frame start.
            collector_.erase(collector_.begin(),
                             collector_.begin() + static_cast<std::ptrdiff_t>(first));
        }

        // 2. The answer ends where the next one begins. Without the following
        //    FF 55 we can't know the current answer is complete yet.
        std::size_t next = findFrameStart(2);
        if (next == collector_.size()) {
            return;  // wait for the next periodic packet
        }

        // 3. Deliver [0, next) as one complete answer, then remove it; the loop
        //    continues from the FF 55 that now sits at the front.
        onAnswer(collector_.data(), next);
        collector_.erase(collector_.begin(),
                         collector_.begin() + static_cast<std::ptrdiff_t>(next));
    }
}

std::size_t ControllerImpl::findFrameStart(std::size_t from) const {
    for (std::size_t i = from; i + 1 < collector_.size(); ++i) {
        if (collector_[i] == MAGIC_B1 && collector_[i + 1] == MAGIC_B2) {
            return i;
        }
    }
    return collector_.size();  // not found
}

void ControllerImpl::onAnswer(const uint8_t* answer, std::size_t length) {
    // Runs on the Source worker thread. Hand the answer to a command that is
    // blocked in sendCommandAndWait(); ignore unsolicited frames.
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (debugListener_) {
        std::stringstream ss;
        ss << "Received answer of length " << length << ": ";
        for (std::size_t i = 0; i < length; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(answer[i]) << " ";
        }
        sendDebugMessage(DebugListener::Level::Raw, ss.str());
    }
    // try to decode and check crc
    if (length < 4) {  // Minimum length: 2 header bytes, 1 payload byte, 1 checksum byte
        sendDebugMessage(DebugListener::Level::Raw, "Packet too small < 4 bytes");
        return;
    }
    if (answer[0] != MAGIC_B1 || answer[1] != MAGIC_B2) {
        sendDebugMessage(DebugListener::Level::Raw, "Invalid packet header");
        return;
    }
    uint8_t calculatedChecksum = calculateChecksum(answer, length);
    uint8_t receivedChecksum = answer[length - 1];
    if (calculatedChecksum != receivedChecksum) {
        sendDebugMessage(DebugListener::Level::Raw, "Checksum mismatch");
        return;
    }
    MessageType messageType = static_cast<MessageType>(answer[2]);

    switch (messageType) {
        case MessageType::Report: {
            sendDebugMessage(DebugListener::Level::Answer, "Received report frame");
            parseReport(answer, length);
            break;
        }

        case MessageType::Reply: {
            sendDebugMessage(DebugListener::Level::Answer, "Received reply frame");
            parseReply(answer, length);
            break;
        }
        case MessageType::MasterSlave: {
            if (length != 11) {
                sendDebugMessage(DebugListener::Level::Raw, "Request packet length mismatch");
                return;
            }
            sendDebugMessage(DebugListener::Level::Answer, "Received Master-Slave frame");
            break;
        }
        default: {
            sendDebugMessage(DebugListener::Level::Answer, 
                "Received unknown frame type: " + std::to_string(static_cast<uint8_t>(messageType)));
            break;
        }
    }
}

uint8_t ControllerImpl::calculateChecksum(const uint8_t* packet, BufferSize_t size) {
    int sum = 0;
    // Skip the first two header bytes and the trailing checksum byte.
    for (BufferSize_t i = 2; i + 1 < size; ++i) {
        sum += packet[i];
    }
    return static_cast<uint8_t>((sum & 0xFF) ^ 0x44);
}

void ControllerImpl::subscribeToDebugLogs(DebugListener *listener) {
    debugListener_ = listener;
}

void ControllerImpl::sendDebugMessage(DebugListener::Level level, const std::string& message) {
    if (debugListener_) {
        debugListener_->onDebugMessage(level, message);
    }
}

void ControllerImpl::parseReport(const uint8_t* answer, std::size_t length) {
    if (length != 36) {
        sendDebugMessage(DebugListener::Level::Raw, "Report packet length mismatch");
        return;
    }
    uint8_t deviceType = answer[3];
    if (deviceType != 0x02) {
        sendDebugMessage(DebugListener::Level::Answer, "Unknown device type: " + std::to_string(deviceType));
        return;  // Not a DL24 device, ignore
    }
    float volt = ((static_cast<uint32_t>(answer[4]) << 16) |
                    (static_cast<uint32_t>(answer[5]) << 8) |
                    (static_cast<uint32_t>(answer[6]) << 0)) / 10.0f;

    float current = ((static_cast<uint32_t>(answer[7]) << 16) |
                    (static_cast<uint32_t>(answer[8]) << 8) |
                    (static_cast<uint32_t>(answer[9]) << 0)) / 1000.0f;
    float capacity = ((static_cast<uint32_t>(answer[10]) << 16) |
                    (static_cast<uint32_t>(answer[11]) << 8) |
                    (static_cast<uint32_t>(answer[12]) << 0)) / 100.0f;
    sendDebugMessage(DebugListener::Level::Answer, 
        "Voltage: " + std::to_string(volt) + 
        ", Current: " + std::to_string(current) +
        ", Capacity: " + std::to_string(capacity));
}

void ControllerImpl::parseReply(const uint8_t* answer, std::size_t length) {
    if (length != 8) {
        sendDebugMessage(DebugListener::Level::Raw, "Reply packet length mismatch");
        return;
    }
    if (waitingForAnswer_) {
        uint16_t errorCode = (static_cast<uint16_t>(answer[3]) << 8) | static_cast<uint16_t>(answer[4]);
        switch (errorCode) {
            case 0x0101:
                replyError_ = Error::None;  // No error
                break;
            case 0x0103:
                replyError_ = Error(ErrorCode::InvalidCommand, "Device does not support this command");
                break;
            default:
                replyError_ = Error(ErrorCode::UnknownError, "Unknown error code: " + std::to_string(errorCode));
                break;
        }
        lastAnswer_.assign(answer, answer + length);
        answerReceived_ = true;
        waitingForAnswer_ = false;
        responseCv_.notify_one();
    }
}

} // namespace dl24
