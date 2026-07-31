#include "ControllerImpl.h"

#include <iostream>
#include <iomanip>
#include <sstream>

namespace dl24 {

// DL24 command codes (see Java CommandsEnum).
enum class Command : uint8_t {
    StartStop = 0x01,
    SetCurrent = 0x02,
    SetVoltage = 0x03,
    ResetAll = 0x05,
};

constexpr int VOLTAGE_SCALE = 100;

constexpr uint8_t cmd(Command c) {
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
    (void) current;  // value encoding still TODO (see Java, currently hardcoded)
    uint8_t command[] = {
        0xB1, 0xB2,  // header
        cmd(Command::SetCurrent),
        0x01, 0x17,
        0xB6,
    };
    auto answer = sendCommandAndWait(command, sizeof(command));
    if (!answer.has_value()) {
        return answer.error();
    }
    // TODO: analyze answer (verify echoed status / checksum).
    return Error::None;
}

Error ControllerImpl::setVoltage(float voltage) {
    uint8_t command[] = {
        0xFF, 0x55,  // header
        0x11, 0x02,  // Master-Slave
        cmd(Command::SetVoltage),
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
    return Error::NotImplemented;
}

Error ControllerImpl::resetCounters() {
    uint8_t command[] = {
        0xFF, 0x55,  // header
        0x11, 0x02,  // Master-Slave
        cmd(Command::ResetAll),
        0x00, 0x00, 0x00, 0x00,  // value
        0x00,                    // checksum
    };
    command[9] = calculateChecksum(command, sizeof(command));
    auto answer = sendCommandAndWait(command, sizeof(command));
    if (!answer.has_value()) {
        return answer.error();
    }
    // TODO: analyze answer (verify echoed status / checksum).
    return Error::None;
}

Error ControllerImpl::sendCommand(const uint8_t* command, BufferSize_t size) {
    return source.write(command, size);
}

std::expected<std::vector<uint8_t>, Error> ControllerImpl::sendCommandAndWait(
        const uint8_t* command, BufferSize_t size) {
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
    return std::expected<std::vector<uint8_t>, Error>(std::move(lastAnswer_));
}

void ControllerImpl::onDataReceived(const uint8_t* data, BufferSize_t size) {
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
        ss << "Received answer of length " << length << ":" << std::endl;
        for (std::size_t i = 0; i < length; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(answer[i]) << " ";
        }
        debugListener_->onDebugMessage(DebugListener::Level::Raw, ss.str());
    }

    if (!waitingForAnswer_) {
        // TODO: route unsolicited status frames (decode into a Dl24Status).
        return;
    }

    lastAnswer_.assign(answer, answer + length);
    answerReceived_ = true;
    waitingForAnswer_ = false;
    responseCv_.notify_one();
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

} // namespace dl24
