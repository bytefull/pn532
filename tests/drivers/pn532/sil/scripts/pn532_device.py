#!/usr/bin/env python3

# Usage: python pn532_device.py --port /dev/pts/4

import os
import argparse
import sys
import termios
import time
import tty

class PN532Simulator:
    COMMANDS = [
        {
            "name": "SAMConfiguration",
            "request": bytes.fromhex("0000ff05fbd4140114010200"),
            "response": bytes.fromhex("0000ff00ff000000ff02fed5151600"),
        },
        {
            "name": "GetFirmwareVersion",
            "request": bytes.fromhex("0000ff02fed4022a00"),
            "response": bytes.fromhex("0000ff00ff000000ff06fad50332010607e800"),
        },
    ]

    def __init__(self, pty_path: str):
        self.fd = None
        print(f"Waiting for {pty_path} ...")
        while self.fd is None:
            try:
                self.fd = os.open(pty_path, os.O_RDWR | os.O_NOCTTY)
            except FileNotFoundError:
                time.sleep(0.5)
        tty.setraw(self.fd, termios.TCSANOW)
        self.buf = bytearray()

    def run(self):
        print(f"PN532 simulator ready on {args.port}")
        last = self.COMMANDS[-1]["name"]
        while True:
            self.buf.extend(os.read(self.fd, 64))
            for cmd in self.COMMANDS:
                if cmd["request"] in self.buf:
                    print(f"<< {cmd['name']}")
                    print(f"   {bytes(self.buf).hex(' ')}")
                    self.buf.clear()
                    os.write(self.fd, cmd["response"])
                    print(f">> {cmd['name']} Response")
                    print(f"   {cmd['response'].hex(' ')}")
                    if cmd["name"] == last:
                        print("\nAll commands served, exiting with SUCCESS.")
                        sys.exit(0)
                    break

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--port", required=True, help="PTY path e.g. /dev/pts/4")
    args = parser.parse_args()

    try:
        PN532Simulator(args.port).run()
    except KeyboardInterrupt:
        print("\nInterrupted, exiting with FAILURE.")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        print("\nException occurred, exiting with FAILURE.")
        sys.exit(1)
