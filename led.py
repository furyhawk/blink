import time
import math
import machine, neopixel

led = neopixel.NeoPixel(machine.Pin(16), 25)
led[0] = (64,64,64)
led[1] = (64,0,0)
led[2] = (0,64,0)
led[3] = (0,64,0)
led[4] = (0,64,0)
led[10] = (0,0,64)
led[11] = (0,0,0)

led.write()

time.sleep(3)
# clear
for i in range(led.n):
    led[i] = (0, 0, 0, 0)
led.write()
time.sleep(3)
def demo(np):
    n = np.n

    # cycle
    for i in range(4 * n):
        for j in range(n):
            np[j] = (0, 0, 0)
        np[i % n] = (64, 64, 64)
        np.write()
        time.sleep_ms(25)

    # bounce
    for i in range(4 * n):
        for j in range(n):
            np[j] = (0, 0, 32)
        if (i // n) % 2 == 0:
            np[i % n] = (0, 0, 0)
        else:
            np[n - 1 - (i % n)] = (0, 0, 0)
        np.write()
        time.sleep_ms(60)

    # fade in/out
    for i in range(0, 4 * 256, 8):
        for j in range(n):
            if (i // 256) % 2 == 0:
                val = i & 0xff
            else:
                val = 255 - (i & 0xff)
            np[j] = (val, 0, 0)
        np.write()

    # clear
    for i in range(n):
        np[i] = (0, 0, 0)
    np.write()
    
# demo(led)