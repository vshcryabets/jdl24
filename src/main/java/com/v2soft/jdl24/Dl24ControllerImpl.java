package com.v2soft.jdl24;

import com.fazecast.jSerialComm.SerialPort;

public class Dl24ControllerImpl implements Dl24Controller {
    private SerialPort portObj;

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
    public boolean connect(String port) {
        portObj = SerialPort.getCommPort(port);
        portObj.setBaudRate(9600);
        portObj.setNumDataBits(8);
        portObj.setNumStopBits(1);
        portObj.setParity(SerialPort.NO_PARITY);

        if (!portObj.openPort()) {
            System.out.println("Failed to open port");
            return false;
        }
        return true;
    }

    @Override
    public boolean disconnect() {
        if (portObj != null) {
            portObj.closePort();
            portObj = null;
        }
        return false;
    }

    @Override
    public boolean setCurrent(float current) {
        return false;
    }

    @Override
    public boolean setVoltage(float voltage) {
        return false;
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
        byte[] command = new byte[] {
                (byte)0xFF, (byte)0x55, // header
                0x11, 0x02,             // Master-Slave
                0x05,                   // Type: reset
                0x00, 0x00, 0x00, 0x00, // Value 0
                0x00                    // Checksum
        };
        command[9] = calculateChecksum(command);
        portObj.writeBytes(command, command.length);
        return false;
    }
}
