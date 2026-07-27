package com.v2soft.jdl24;

public interface Dl24Controller {
    boolean connect();
    boolean disconnect() throws InterruptedException;
    boolean setCurrent(float current);
    boolean setVoltage(float voltage);
    boolean setTimer(int timer);
    boolean start();
    boolean stop();
    boolean resetCounters();
}
