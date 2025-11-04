#!/usr/bin/env python3

import serial
import time
import sys

def reset_pico(port='/dev/tty.usbmodem101', pulse_length_ms=100, pulses=1, post_delay=0.5):
    """
    Reset a Pico into bootloader mode by toggling DTR/RTS and opening at 1200 baud.

    :param port: Serial port string (e.g., '/dev/tty.usbmodem101')
    :param pulse_length_ms: Duration to hold DTR/RTS low in ms
    :param pulses: Number of reset pulses
    :param post_delay: Delay after pulses before closing port (seconds)
    """
    try:
        # Open port at 1200 baud to trigger USB CDC "touch"
        ser = serial.Serial(port, 1200, timeout=1)
        print(f"[INFO] Opened {port} at 1200 baud for bootloader trigger. Trying for {pulses} pulse")

        for i in range(pulses):
            ser.dtr = False  # Hold DTR low
            ser.rts = False  # Optionally hold RTS low
            print(f"[DEBUG] Pulse {i+1}: DTR/RTS LOW for {pulse_length_ms}ms")
            time.sleep(pulse_length_ms / 1000.0)
            #ser.dtr = True   # Release DTR
            #ser.rts = True   # Release RTS
            #print(f"[DEBUG] Pulse {i+1}: DTR/RTS HIGH")
            time.sleep(0.05)  # Short delay between pulses
        print("[DEBUG] closing")
        #time.sleep(post_delay)  # Give Pico time to enter bootloader
        ser.close()
        print(f"[INFO] Closed {port}. Pico should be in bootloader mode now.")

    except Exception as e:
        print(f"[ERROR] Could not reset Pico: {e}")
        sys.exit(1)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        port = '/dev/tty.usbmodem101'
    else:
        port = sys.argv[1]
    reset_pico(port)


