package com.v2soft;

import com.fazecast.jSerialComm.SerialPort;
import com.v2soft.jdl24.Dl24Controller;
import com.v2soft.jdl24.Dl24ControllerImpl;

//TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or
// click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.
public class Main {
    public static void main(String[] args) throws InterruptedException {
        var ports = SerialPort.getCommPorts();
        for (var port: ports) {
            System.out.println("Port=" + port.getDescriptivePortName() + "  " + port.getSystemPortName());
        }
        Dl24Controller ctrl = new Dl24ControllerImpl();
        if (!ctrl.connect("/dev/ttyUSB0")) {
            System.out.println("Can't open port");
            System.exit(1);
        }
        ctrl.resetCounters();
        ctrl.setVoltage(1.5f);
        ctrl.setCurrent(1.0f);
        System.out.println("T1");
        Thread.sleep(5000);
        ctrl.disconnect();
        System.out.println("T2");
    }
}