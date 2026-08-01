package com.v2soft.jdl24;

/**
 * Abstract UART source used by the DL24 controller logic.
 * <p>
 * It hides the platform-specific serial transport behind a single interface so
 * the controller can talk to the device the same way regardless of the
 * underlying implementation (jSerialComm, a test double, a network bridge, ...).
 * <p>
 * Java counterpart of the C++ {@code dl24::Source} interface.
 */
public interface Source {

    enum Parity {
        NONE,
        EVEN,
        ODD
    }

    enum StopBits {
        ONE,
        TWO
    }

    enum DataBits {
        FIVE(5),
        SIX(6),
        SEVEN(7),
        EIGHT(8);

        public final int bits;

        DataBits(int bits) {
            this.bits = bits;
        }
    }

    /** UART line settings applied when opening the port. */
    class UartConfig {
        public int baudRate = 9600;
        public DataBits dataBits = DataBits.EIGHT;
        public Parity parity = Parity.NONE;
        public StopBits stopBits = StopBits.ONE;
    }

    /** Callback interface for incoming UART data. */
    interface Listener {
        /**
         * Called when new bytes arrive from the UART.
         *
         * @param data buffer holding the received bytes; only the first
         *             {@code size} bytes are valid and the buffer is reused
         *             after the call returns, so copy anything that must
         *             outlive it.
         * @param size number of valid bytes in {@code data}.
         */
        void onDataReceived(byte[] data, int size);
    }

    /**
     * Open the UART with the given settings.
     *
     * @return true on success.
     */
    boolean open(UartConfig config);

    /**
     * Close the UART and stop delivering incoming data.
     * Safe to call when already closed.
     *
     * @return true on success.
     */
    boolean close() throws InterruptedException;

    /** @return true when the UART is open and usable. */
    boolean isOpen();

    /**
     * Write a raw packet to the UART.
     *
     * @return true when all bytes were queued/sent.
     */
    boolean write(byte[] data);

    /**
     * Register the listener that receives incoming bytes.
     * Pass null to unsubscribe. Only a single listener is supported.
     */
    void setListener(Listener listener);
}
