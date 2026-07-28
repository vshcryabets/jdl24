#include "ControllerImpl.h"

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

bool ControllerImpl::connect() {
    source.setListener(this);
    collector_.clear();
    Source::UartConfig config;  // defaults: 9600 8N1
    return source.open(config) == Source::Result::Ok;
}

bool ControllerImpl::disconnect() {
    Source::Result result = source.close();
    source.setListener(nullptr);
    return result == Source::Result::Ok;
}

bool ControllerImpl::setCurrent(float current) {
    (void) current;  // value encoding still TODO (see Java, currently hardcoded)
    uint8_t command[] = {
        0xB1, 0xB2,  // header
        cmd(Command::SetCurrent),
        0x01, 0x17,
        0xB6,
    };
    auto answer = sendCommandAndWait(command, sizeof(command));
    if (!answer) {
        return false;
    }
    // TODO: analyze answer (verify echoed status / checksum).
    return true;
}

bool ControllerImpl::setVoltage(float voltage) {
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
    if (!answer) {
        return false;
    }
    // TODO: analyze answer (verify echoed status / checksum).
    return true;
}

bool ControllerImpl::setTimer(int32_t timer) {
    (void) timer;
    return false;
}

bool ControllerImpl::start() {
    return false;
}

bool ControllerImpl::stop() {
    return false;
}

bool ControllerImpl::resetCounters() {
    uint8_t command[] = {
        0xFF, 0x55,  // header
        0x11, 0x02,  // Master-Slave
        cmd(Command::ResetAll),
        0x00, 0x00, 0x00, 0x00,  // value
        0x00,                    // checksum
    };
    command[9] = calculateChecksum(command, sizeof(command));
    auto answer = sendCommandAndWait(command, sizeof(command));
    if (!answer) {
        return false;
    }
    // TODO: analyze answer (verify echoed status / checksum).
    return true;
}

bool ControllerImpl::sendCommand(const uint8_t* command, BufferSize_t size) {
    return source.write(command, size) == Source::Result::Ok;
}

std::optional<std::vector<uint8_t>> ControllerImpl::sendCommandAndWait(
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
    if (!sendCommand(command, size)) {
        waitingForAnswer_ = false;
        return std::nullopt;
    }

    bool ok = responseCv_.wait_for(lock, kResponseTimeout,
                                   [this] { return answerReceived_; });
    waitingForAnswer_ = false;
    if (!ok) {
        return std::nullopt;  // timed out
    }
    return std::move(lastAnswer_);
}

void ControllerImpl::onDataReceived(const uint8_t* data, BufferSize_t size) {
    // Add everything to the collector as-is, without any checks.
    collector_.insert(collector_.end(), data, data + size);
    parseCollector();
}

void ControllerImpl::parseCollector() {
    while (true) {
        // 1. Align the collector to the next answer start (FF 55).
        std::size_t start = 0;
        bool found = false;
        for (std::size_t i = 0; i + 1 < collector_.size(); ++i) {
            if (collector_[i] == MAGIC_B1 && collector_[i + 1] == MAGIC_B2) {
                start = i;
                found = true;
                break;
            }
        }
        if (!found) {
            // No frame start yet. Drop leading garbage, but keep a trailing FF
            // in case the matching 55 arrives in the next chunk.
            if (!collector_.empty() && collector_.back() == MAGIC_B1) {
                collector_.erase(collector_.begin(), collector_.end() - 1);
            } else {
                collector_.clear();
            }
            return;
        }
        if (start > 0) {
            // Discard bytes before the frame start.
            collector_.erase(collector_.begin(),
                             collector_.begin() + static_cast<std::ptrdiff_t>(start));
        }

        // 2. Do we have a complete answer at the front yet?
        std::size_t length = answerLength(collector_.data(), collector_.size());
        if (length == 0 || length > collector_.size()) {
            return;  // wait for more bytes
        }

        // 3. Deliver the complete answer, then remove it from the collector.
        onAnswer(collector_.data(), length);
        collector_.erase(collector_.begin(),
                         collector_.begin() + static_cast<std::ptrdiff_t>(length));
    }
}

std::size_t ControllerImpl::answerLength(const uint8_t* buffer, std::size_t available) const {
    // TODO: derive the real DL24 answer length from the frame header once the
    // answer wire format is known. For now a fixed-size frame is assumed.
    (void) buffer;
    if (available < kAnswerSize) {
        return 0;
    }
    return kAnswerSize;
}

void ControllerImpl::onAnswer(const uint8_t* answer, std::size_t length) {
    // Runs on the Source worker thread. Hand the answer to a command that is
    // blocked in sendCommandAndWait(); ignore unsolicited frames.
    std::lock_guard<std::mutex> lock(stateMutex_);
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

} // namespace dl24
