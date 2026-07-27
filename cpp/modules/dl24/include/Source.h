#pragma once

#include <cstdint>

// `BufferSize_t` is normally provided project-wide via a compile definition
// (see the top-level CMakeLists.txt). Fall back to a sane default so this
// header stays self-contained when included on its own.
#ifndef BufferSize_t
#define BufferSize_t uint16_t
#endif

namespace dl24 {

/**
 * Abstract UART source used by the DL24 controller logic.
 *
 * It hides the platform-specific serial transport behind a single interface so
 * the controller can talk to the device the same way on every target. Concrete
 * implementations are provided per platform (e.g. Unix/Linux/macOS termios,
 * ESP32 driver, STM32 HAL, ...).
 *
 * A Source owns a hardware/OS resource, therefore it is non-copyable.
 */
class Source {
public:
    enum class Parity : uint8_t {
        None,
        Even,
        Odd,
    };

    enum class StopBits : uint8_t {
        One,
        Two,
    };

    enum class DataBits : uint8_t {
        Five = 5,
        Six = 6,
        Seven = 7,
        Eight = 8,
    };

    /** UART line settings applied when opening the port. */
    struct UartConfig {
        uint32_t baudRate = 9600;
        DataBits dataBits = DataBits::Eight;
        Parity parity = Parity::None;
        StopBits stopBits = StopBits::One;
    };

    enum class Result : uint8_t {
        Ok = 0,
        AlreadyOpen,
        NotOpen,
        OpenFailed,
        WriteFailed,
        InvalidConfig,
        Unsupported,
    };

    /**
     * Callback interface for incoming UART data.
     *
     * Implemented as an abstract interface rather than std::function to stay
     * allocation-free and usable on bare-metal targets (ESP32/STM32).
     */
    class Listener {
    public:
        virtual ~Listener() = default;

        /**
         * Called when new bytes arrive from the UART.
         *
         * @param data pointer to the received bytes; valid only for the
         *             duration of the call. Copy anything that must outlive it.
         * @param size number of valid bytes in @p data.
         */
        virtual void onDataReceived(const uint8_t* data, BufferSize_t size) = 0;
    };

    virtual ~Source() = default;

    /**
     * Open the UART with the given settings.
     *
     * @return Result::Ok on success, Result::AlreadyOpen if the port is already
     *         open, or another error code on failure.
     */
    virtual Result open(const UartConfig& config) = 0;

    /**
     * Close the UART and stop delivering incoming data.
     * Safe to call when already closed.
     */
    virtual Result close() = 0;

    /** @return true when the UART is open and usable. */
    virtual bool isOpen() const = 0;

    /**
     * Write a raw packet to the UART.
     *
     * @param data pointer to the bytes to send.
     * @param size number of bytes to send.
     * @return Result::Ok when all bytes were queued/sent.
     */
    virtual Result write(const uint8_t* data, BufferSize_t size) = 0;

    /**
     * Register the listener that receives incoming bytes.
     *
     * Pass nullptr to unsubscribe. Only a single listener is supported.
     */
    virtual void setListener(Listener* listener) = 0;

protected:
    Source() = default;

    Source(const Source&) = delete;
    Source& operator=(const Source&) = delete;
};

} // namespace dl24
