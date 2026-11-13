#!/usr/bin/env python3
"""Set the board's RTC from this computer's clock over the console UART."""
import argparse
import datetime
import os
import re
import sys
import termios
import time

CONFIRM = re.compile(r"time set to (\d{4}-\d\d-\d\d \d\d:\d\d:\d\d)")


def open_port(path):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0                                    # iflag
    attrs[1] = 0                                    # oflag
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0                                    # lflag: raw
    attrs[4] = attrs[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    return fd


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", default="/dev/ttyUSB0", help="serial device (default /dev/ttyUSB0)")
    args = ap.parse_args()

    fd = open_port(args.port)
    stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    os.write(fd, f"time {stamp}\r".encode())

    seen, end = "", time.time() + 2.0
    while time.time() < end:
        try:
            seen += os.read(fd, 4096).decode(errors="replace")
        except BlockingIOError:
            time.sleep(0.02)
        if CONFIRM.search(seen):
            break

    m = CONFIRM.search(seen)
    if not m:
        sys.exit("no confirmation from the board")
    print(f"board clock set to {m.group(1)}")


if __name__ == "__main__":
    main()
