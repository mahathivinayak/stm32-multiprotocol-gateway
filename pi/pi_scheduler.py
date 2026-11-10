#!/usr/bin/env python3
"""Raspberry Pi side of the scheduling link to the STM32 gateway.

3.3 V UART: Pi GPIO14 TX (pin 8) -> STM32 RX, GPIO15 RX (pin 10) <- STM32 TX, GND-GND.
Enable with: sudo raspi-config -> Interface Options -> Serial Port
             (login shell over serial: No, serial hardware: Yes)

  python3 pi_scheduler.py SCHED temp 2000     # one command
  python3 pi_scheduler.py STATUS
  python3 pi_scheduler.py --plan              # scripted demo schedule
  python3 pi_scheduler.py                     # interactive prompt
"""
import argparse
import time

import serial

# (command, seconds to wait afterwards)
PLAN = [
    ("STATUS", 1),
    ("SCHED temp 500", 10),     # burst: sample temperature fast
    ("SCHED can 0", 10),        # pause the CAN heartbeat
    ("STATUS", 1),
    ("SCHED temp 1000", 1),     # back to normal
    ("SCHED can 1000", 1),
    ("STATUS", 0),
]


def send(ser, cmd):
    ser.reset_input_buffer()
    ser.write((cmd + "\n").encode("ascii"))
    reply = ser.readline().decode("ascii", errors="replace").strip()
    print(f"> {cmd}\n< {reply or '(no reply)'}")
    return reply


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("command", nargs="*", help="e.g. SCHED temp 2000, or STATUS")
    ap.add_argument("--port", default="/dev/serial0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--plan", action="store_true", help="run the scripted demo schedule")
    args = ap.parse_args()

    with serial.Serial(args.port, args.baud, timeout=1) as ser:
        if args.plan:
            for cmd, wait_s in PLAN:
                send(ser, cmd)
                time.sleep(wait_s)
        elif args.command:
            send(ser, " ".join(args.command))
        else:
            try:
                while True:
                    cmd = input("sched> ").strip()
                    if cmd:
                        send(ser, cmd)
            except (EOFError, KeyboardInterrupt):
                print()


if __name__ == "__main__":
    main()
