package com.v2soft.jdl24;

public enum CommandsEnum {
    START_STOP((byte) 0x01),
    SET_CURRENT((byte) 0x02),
    SET_VOLATGE((byte) 0x03),
    RESET_ALL((byte) 0x05)
    ;

    public final byte code;

    CommandsEnum(byte code) {
        this.code = code;
    }
}
