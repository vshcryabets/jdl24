package com.v2soft;

import com.v2soft.jdl24.Dl24Controller;
import com.v2soft.jdl24.Dl24ControllerImpl;
import com.v2soft.jdl24.SerialPortSourceImpl;
import com.v2soft.jdl24.Source;

public class Main {
    public static void main(String[] args) throws InterruptedException {
//        var ports = SerialPort.getCommPorts();
//        for (var port: ports) {
//            System.out.println("Port=" + port.getDescriptivePortName() + "  " + port.getSystemPortName());
//        }
        Source source = new SerialPortSourceImpl("/dev/ttyUSB0");
        Dl24Controller ctrl = new Dl24ControllerImpl(source);
        if (!ctrl.connect()) {
            System.out.println("Can't open port");
            System.exit(1);
        }
        System.out.println("A 10");
        System.out.flush();
        Thread.sleep(5000);
        System.out.println("T0");
//        ctrl.resetCounters();
//        ctrl.setVoltage(1.5f);
        ctrl.setCurrent(1.0f);
        Thread.sleep(1000);
        ctrl.setCurrent(1.0f);
        System.out.println("T1");
        Thread.sleep(10000);
        ctrl.disconnect();
        System.out.println("T2");
    }
}