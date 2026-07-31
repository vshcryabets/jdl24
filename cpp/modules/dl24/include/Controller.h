#pragma once

#include <cstdint>

#include "Error.h"
#include "Source.h"

namespace dl24 {

class DebugListener {
public:
    enum class Level: uint8_t {
        Raw,
        Paket,
        Answer,
    };
public:
    virtual ~DebugListener() = default;

    /**
     * Called when a debug message is available.
     *
     * @param message the debug message.
     */
    virtual void onDebugMessage(Level level, const std::string& message) {};
};

/**
 * Abstract DL24 electronic-load controller.
 *
 * Encapsulates the DL24 command protocol (connect, configure, start/stop,
 * counters) on top of a UART Source. Concrete implementations build the command
 * packets and talk to the device through a Source instance.
 *
 * Mirrors the Java `com.v2soft.jdl24.Dl24Controller` interface.
 *
 * A Controller drives a stateful device connection, therefore it is
 * non-copyable.
 */
class Controller {
public:
    virtual ~Controller() = default;

    /**
     * Open the connection to the device using the injected Source.
     * @return true on success.
     */
    virtual Error connect() = 0;

    /**
     * Close the connection to the device and stop any background processing.
     * @return true on success.
     */
    virtual Error disconnect() = 0;

    /** Set the target load current in amperes. @return Error indicating success or failure. */
    virtual Error setCurrent(float current) = 0;

    /** Set the target/cutoff voltage in volts. @return Error indicating success or failure. */
    virtual Error setVoltage(float voltage) = 0;

    /** Set the timer in seconds. @return Error indicating success or failure. */
    virtual Error setTimer(int32_t timer) = 0;

    /** Start the load. @return Error indicating success or failure. */
    virtual Error start() = 0;

    /** Stop the load. @return Error indicating success or failure. */
    virtual Error stop() = 0;

    /** Reset all accumulated counters (capacity, energy, time). @return Error indicating success or failure. */
    virtual Error resetCounters() = 0;

    /** Debug methods */
    virtual void subscribeToDebugLogs(DebugListener *listener) = 0;

protected:
    /**
     * @param source UART transport used to talk to the device. The Controller
     *               stores a reference to it; the Source must outlive the
     *               Controller.
     */
    explicit Controller(Source& source) : source(source) {}

    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;

    /** UART transport used by concrete implementations. */
    Source& source;
};

} // namespace dl24
