#pragma once

#include "Controller.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace dl24 {

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

    bool connect() override;
    bool disconnect() override;
    bool setCurrent(float current) override;
    bool setVoltage(float voltage) override;
    bool setTimer(int32_t timer) override;
    bool start() override;
    bool stop() override;
    bool resetCounters() override;

    void onDataReceived(const uint8_t* data, BufferSize_t size) override;

protected:
    /** Compute the DL24 checksum over a 10-byte packet. */
    static uint8_t calculateChecksum(const uint8_t* packet, BufferSize_t size);

    /**
     * Determine the length of the complete answer that starts at the front of
     * @p buffer (guaranteed to begin with FF 55).
     *
     * @param available number of bytes currently at the front of the buffer.
     * @return the total answer length in bytes, or 0 if not enough bytes have
     *         arrived yet to determine/complete the answer.
     */
    virtual std::size_t answerLength(const uint8_t* buffer, std::size_t available) const;

    /**
     * Called once for each complete answer frame extracted from the collector.
     * @p answer begins with FF 55 and is @p length bytes long.
     */
    virtual void onAnswer(const uint8_t* answer, std::size_t length);

private:
    // Scan the collector for complete answers, delivering and removing each.
    void parseCollector();

    // Low-level fire-and-forget write of a command packet.
    bool sendCommand(const uint8_t* command, BufferSize_t size);

    /**
     * Send a command and block until the device answers or the timeout elapses.
     *
     * Only one round-trip runs at a time (commandMutex_ serializes callers).
     * The answer is delivered by onAnswer() on the Source worker thread.
     *
     * @return the answer bytes, or std::nullopt on send failure / timeout.
     */
    std::optional<std::vector<uint8_t>> sendCommandAndWait(const uint8_t* command,
                                                           BufferSize_t size);

    static constexpr uint8_t MAGIC_B1 = 0xFF;
    static constexpr uint8_t MAGIC_B2 = 0x55;

    // Provisional DL24 answer length. TODO: replace with the real frame size
    // (or a length derived from the frame header) once the answer wire format
    // is known; this is the single place that decides answer boundaries.
    static constexpr std::size_t kAnswerSize = 10;

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
    std::vector<uint8_t> lastAnswer_;
};

} // namespace dl24
