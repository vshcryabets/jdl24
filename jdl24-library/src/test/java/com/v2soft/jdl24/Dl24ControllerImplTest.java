package com.v2soft.jdl24;

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

public class Dl24ControllerImplTest {
    @Test
    void testPacketHandler() {
        Dl24ControllerImpl impl = new Dl24ControllerImpl();
        var buffer = new byte[]{0x12, (byte) 0xFF, 0x55};
        int newOffset = impl.handleData(buffer, 3);
        Assertions.assertEquals(2, newOffset);
        Assertions.assertEquals((byte) 0xFF, buffer[0]);
        Assertions.assertEquals((byte) 0x55, buffer[1]);

        buffer = new byte[]{0x12, 0x13, (byte) 0xFF};
        newOffset = impl.handleData(buffer, 3);
        Assertions.assertEquals(1, newOffset);
        Assertions.assertEquals((byte) 0xFF, buffer[0]);

        // no magic at all
        buffer = new byte[]{0x12, 0x13, 0x55};
        newOffset = impl.handleData(buffer, 3);
        Assertions.assertEquals(0, newOffset);
    }
}
