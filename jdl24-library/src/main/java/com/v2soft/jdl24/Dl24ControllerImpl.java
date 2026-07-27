package com.v2soft.jdl24;

import java.util.HexFormat;

public class Dl24ControllerImpl implements Dl24Controller, Source.Listener {
    private final Source source;
    private final int CURRENT_SCALE = 100;
    private final int VOLTAGE_SCALE = 100;
    protected final byte MAGIC_B1 = (byte) 0xFF;
    protected final byte MAGIC_B2 = 0x55;
    private final byte[] rxBuffer = new byte[256];
    private int rxOffset = 0;

    public Dl24ControllerImpl(Source source) {
        this.source = source;
    }

    public byte calculateChecksum(byte[] packet) {
        if (packet.length != 10) {
            throw new IllegalArgumentException("Packet must be 10 bytes long");
        }
        int sum = 0;
        // Let's skip first FF 55, and last one byte (checksum)
        for (int i = 2; i < packet.length - 1; i++) {
            sum += (packet[i] & 0xFF);
        }
        return (byte) ((sum & 0xFF) ^ 0x44);
    }

    @Override
    public boolean connect() {
        source.setListener(this);
        rxOffset = 0;
        return source.open(new Source.UartConfig());
    }

    @Override
    public boolean disconnect() throws InterruptedException {
        boolean result = source.close();
        source.setListener(null);
        return result;
    }

    @Override
    public boolean setCurrent(float current) {
        byte[] command = new byte[]{
                (byte) 0xB1,
                (byte) 0xB2, // header
                CommandsEnum.SET_CURRENT.code,
                0x01, 0x17,
                (byte) 0xB6
        };
//        int intValue = (int) (current * CURRENT_SCALE);
//        command[5] = (byte) ((intValue >> 24) & 0xFF);
//        command[6] = (byte) ((intValue >> 16) & 0xFF);
//        command[7] = (byte) ((intValue >> 8) & 0xFF);
//        command[8] = (byte) ((intValue >> 0) & 0xFF);
//        command[9] = calculateChecksum(command);
        HexFormat format = HexFormat.ofDelimiter(" ").withUpperCase();
        System.out.println(format.formatHex(command));

        return sendCommand(command);
    }

    @Override
    public boolean setVoltage(float voltage) {
        byte[] command = new byte[]{
                (byte) 0xFF, (byte) 0x55, // header
                0x11, 0x02,             // Master-Slave
                CommandsEnum.SET_VOLATGE.code,
                0x00, 0x00, 0x00, 0x00, // Value 0
                0x00                    // Checksum
        };
        int intValue = (int) (voltage * VOLTAGE_SCALE);
        command[5] = (byte) ((intValue >> 24) & 0xFF);
        command[6] = (byte) ((intValue >> 16) & 0xFF);
        command[7] = (byte) ((intValue >> 8) & 0xFF);
        command[8] = (byte) ((intValue >> 0) & 0xFF);
        command[9] = calculateChecksum(command);

        HexFormat format = HexFormat.ofDelimiter(" ").withUpperCase();
        System.out.println(format.formatHex(command));

        return sendCommand(command);
    }

    @Override
    public boolean setTimer(int timer) {
        return false;
    }

    @Override
    public boolean start() {
        return false;
    }

    @Override
    public boolean stop() {
        return false;
    }

    @Override
    public boolean resetCounters() {
        byte[] command = new byte[]{
                (byte) 0xFF, (byte) 0x55, // header
                0x11, 0x02,             // Master-Slave
                CommandsEnum.RESET_ALL.code,
                0x00, 0x00, 0x00, 0x00, // Value 0
                0x00                    // Checksum
        };
        command[9] = calculateChecksum(command);
        return sendCommand(command);
    }

    private boolean sendCommand(byte[] command) {
        return source.write(command);
    }

    @Override
    public void onDataReceived(byte[] data, int size) {
        // Append the freshly read bytes to the accumulation buffer, then let
        // handleData() realign to the magic-byte frame boundary.
        int copy = Math.min(size, rxBuffer.length - rxOffset);
        System.arraycopy(data, 0, rxBuffer, rxOffset, copy);
        rxOffset += copy;
        if (rxOffset > 0) {
            rxOffset = handleData(rxBuffer, rxOffset);
        }
    }

    protected int handleData(byte[] buffer, int capacity) {
        System.out.println("Got response: " + capacity);
        HexFormat format = HexFormat.ofDelimiter(" ").withUpperCase();
        System.out.println(format.formatHex(buffer, 0, capacity));
        int magic = 0;
        boolean magicFound = false;
        // try to find magic bytes
        for (int i = 0; i < capacity - 1; i++) {
            if ((buffer[i] == MAGIC_B1) && (buffer[i + 1] == MAGIC_B2)) {
                magic = i;
                magicFound = true;
                break;
            }
        }
        if (!magicFound) {
            if (buffer[capacity - 1] == MAGIC_B1) {
                buffer[0] = MAGIC_B1;
                return 1;
            }
            // we will drop all buffer
            return 0;
        }
        if (magic > 0) {
            // move buffer
            System.arraycopy(buffer, magic, buffer, 0, capacity - magic);
            capacity = capacity - magic;
        }
        return capacity;
    }
}
