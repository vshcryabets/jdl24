package com.v2soft.jdl24;

import com.fazecast.jSerialComm.SerialPort;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * {@link Source} implementation backed by a physical serial port via
 * jSerialComm. Owns the port and a worker thread that reads incoming bytes and
 * forwards them to the registered {@link Listener}.
 */
public class SerialPortSourceImpl implements Source {
    private final String portName;
    private SerialPort portObj;
    private Listener listener;
    private Thread worker = null;
    private final AtomicBoolean stopFlag = new AtomicBoolean(false);

    public SerialPortSourceImpl(String portName) {
        this.portName = portName;
    }

    @Override
    public boolean open(UartConfig config) {
        if (isOpen()) {
            return false;
        }
        portObj = SerialPort.getCommPort(portName);
        portObj.setBaudRate(config.baudRate);
        portObj.setNumDataBits(config.dataBits.bits);
        portObj.setNumStopBits(toSerialStopBits(config.stopBits));
        portObj.setParity(toSerialParity(config.parity));

        if (!portObj.openPort()) {
            System.out.println("Failed to open port");
            portObj = null;
            return false;
        }
        stopFlag.set(false);
        worker = new Thread(this::workerFunction);
        worker.start();
        return true;
    }

    @Override
    public boolean close() throws InterruptedException {
        if (worker != null) {
            stopFlag.set(true);
            worker.join();
            worker = null;
        }
        if (portObj != null) {
            portObj.closePort();
            portObj = null;
        }
        return true;
    }

    @Override
    public boolean isOpen() {
        return portObj != null && portObj.isOpen();
    }

    @Override
    public boolean write(byte[] data) {
        if (!isOpen()) {
            return false;
        }
        portObj.writeBytes(data, data.length);
        return true;
    }

    @Override
    public void setListener(Listener listener) {
        this.listener = listener;
    }

    private void workerFunction() {
        var buffer = new byte[256];
        while (!stopFlag.get()) {
            // check serial port
            if (portObj.bytesAvailable() > 0) {
                int read = portObj.readBytes(buffer, buffer.length);
                if (read > 0 && listener != null) {
                    listener.onDataReceived(buffer, read);
                }
            } else {
                try {
                    Thread.sleep(20);
                } catch (Exception err) {
                    // nothing to do
                }
            }
        }
    }

    private static int toSerialStopBits(StopBits stopBits) {
        return switch (stopBits) {
            case TWO -> SerialPort.TWO_STOP_BITS;
            case ONE -> SerialPort.ONE_STOP_BIT;
        };
    }

    private static int toSerialParity(Parity parity) {
        return switch (parity) {
            case EVEN -> SerialPort.EVEN_PARITY;
            case ODD -> SerialPort.ODD_PARITY;
            case NONE -> SerialPort.NO_PARITY;
        };
    }
}
