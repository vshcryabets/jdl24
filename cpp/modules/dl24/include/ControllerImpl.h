#pragma once

#include "Controller.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>
#include <expected>

namespace dl24 {

constexpr uint8_t PX100_ACK = 0x6F;  // Acknowledge byte sent by PX100 on successful command receipt

/**
 * Concrete DL24 controller implementing the device command protocol on top of
 * a UART Source. Port of the Java {@code Dl24ControllerImpl}.
 *
 * It subscribes to the Source as a Listener. Incoming bytes are appended to a
 * collector buffer as-is; the buffer is then scanned for complete answer
 * frames. Every answer starts with the magic bytes {@code FF 55}; a fully
 * received answer is handed to onAnswer() and removed from the collector.
 */
class ControllerImpl : public Controller, public Source::Listener {
public:
    explicit ControllerImpl(Source& source);

    Error connect() override;
    Error disconnect() override;
    Error setCurrent(float current) override;
    Error setVoltage(float voltage) override;
    Error setTimer(int32_t timer) override;
    Error start() override;
    Error stop() override;
    Error resetCounters() override;

    void onDataReceived(const uint8_t* data, BufferSize_t size) override;
    void subscribeToDebugLogs(DebugListener *listener) override;

protected:
    /** Compute the DL24 checksum over a 10-byte packet. */
    static uint8_t calculateChecksum(const uint8_t* packet, BufferSize_t size);

    /**
     * Called once for each complete answer frame extracted from the collector.
     * @p answer begins with FF 55 and is @p length bytes long.
     */
    virtual void onAnswer(const uint8_t* answer, std::size_t length);

    void sendDebugMessage(DebugListener::Level level, const std::string& message);
    void parseReport(const uint8_t* answer, std::size_t length);
    void parseReply(const uint8_t* answer, std::size_t length);

private:
    // Extract complete answers from the collector, delivering and removing each.
    // The device streams answers periodically, so an answer runs from its FF 55
    // header up to (but not including) the next FF 55 header; the trailing,
    // not-yet-terminated answer stays buffered until the next header arrives.
    void parseCollector();

    // Index of the next FF 55 header at or after @p from, or collector_.size()
    // if none is present.
    std::size_t findFrameStart(std::size_t from) const;

    // Low-level fire-and-forget write of a command packet.
    Error sendCommand(const uint8_t* command, BufferSize_t size);

    /**
     * Send a command and block until the device answers or the timeout elapses.
     *
     * Only one round-trip runs at a time (commandMutex_ serializes callers).
     * The answer is delivered by onAnswer() on the Source worker thread.
     *
     * @return the answer bytes, or std::nullopt on send failure / timeout.
     */
    std::expected<std::vector<uint8_t>, Error> 
    sendCommandAndWait(
        const uint8_t* command,
        BufferSize_t size
    );

    /**
     * Send a PX100 command and block until the device answers or the timeout elapses.
     *
     * @return the answer bytes, or std::nullopt on send failure / timeout.
     */
    std::expected<std::vector<uint8_t>, Error> 
    sendPX100CommandAndWait(
        const uint8_t* command,
        BufferSize_t size
    );

    static constexpr uint8_t MAGIC_B1 = 0xFF;
    static constexpr uint8_t MAGIC_B2 = 0x55;

    // How long a command waits for its answer before giving up.
    static constexpr std::chrono::milliseconds kResponseTimeout{1000};

    std::vector<uint8_t> collector_;

    // Command/response synchronization. commandMutex_ serializes whole
    // round-trips; stateMutex_ guards the answer handoff between the caller
    // thread and the Source worker thread.
    std::mutex commandMutex_;
    std::mutex stateMutex_;
    std::condition_variable responseCv_;
    bool waitingForAnswer_ = false;
    bool answerReceived_ = false;
    bool waitingForPx100Answer_ = false;
    bool answerPx100Received_ = false;
    Error replyError_ = Error::Timeout;
    std::vector<uint8_t> lastAnswer_;
    DebugListener *debugListener_ = nullptr;
};

} // namespace dl24
