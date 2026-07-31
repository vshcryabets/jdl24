#pragma once

#include "Source.h"

#include <atomic>
#include <string>
#include <thread>

namespace dl24 {

/**
 * Linux {@link Source} implementation backed by a termios serial device.
 *
 * A dedicated worker thread waits for incoming bytes using poll(2) on the
 * serial file descriptor, so it consumes no CPU while idle and delivers data
 * with no polling latency. An eventfd is polled alongside the serial fd so
 * close() can interrupt the blocked poll(2) immediately for a clean shutdown.
 */
class SourceLinuxImpl : public Source {
public:
    /**
     * @param device serial device path, e.g. "/dev/ttyUSB0".
     */
    explicit SourceLinuxImpl(std::string device);
    ~SourceLinuxImpl() override;

    Error open(const UartConfig& config) override;
    Error close() override;
    bool isOpen() const override;
    Error write(const uint8_t* data, BufferSize_t size) override;
    void setListener(Listener* listener) override;

private:
    void workerLoop();

    // Configure termios line settings on fd_. Returns true on success.
    bool configurePort(const UartConfig& config);

    // Map a numeric baud rate to a termios speed constant (Bxxxx).
    // Returns false if the rate is not supported.
    static bool toBaudConstant(uint32_t baudRate, unsigned& out);

    std::string device_;
    int fd_ = -1;      // serial device fd
    int wakeFd_ = -1;  // eventfd used to interrupt poll() on close()
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<Listener*> listener_{nullptr};
};

} // namespace dl24
