#!/usr/bin/env python3

import argparse
import subprocess
import time
import os
import sys

from resource_rich.monitor.monitor_impl import MonitorBase

from tools.run import supported_mote_types, supported_firmware_types
from tools.run.util import Teed, Popen, StreamNoTimestamp

DEFAULT_LOG_DIR="~/iot-trust-task-alloc/logs"

parser = argparse.ArgumentParser(description='Adversary runner')
parser.add_argument('--log-dir', type=str, default=DEFAULT_LOG_DIR, help='The directory to store log output')

# Flash.py
parser.add_argument("--mote", default="/dev/ttyUSB0", help="The mote to flash.")
parser.add_argument("--mote_type",
                    choices=supported_mote_types,
                    default=supported_mote_types[0],
                    help="The type of mote.")
parser.add_argument("--firmware_type",
                    choices=supported_firmware_types,
                    default=supported_firmware_types[0],
                    help="The OS that was used to create the firmware.")

args = parser.parse_args()

if args.log_dir.startswith("~"):
    args.log_dir = os.path.expanduser(args.log_dir)

# Create log dir if it does not exist
if not os.path.isdir(args.log_dir):
    os.makedirs(args.log_dir)

hostname = os.uname()[1]

motelist_log_path = os.path.join(args.log_dir, f"adversary.{hostname}.motelist.log")
flash_log_path = os.path.join(args.log_dir, f"adversary.{hostname}.flash.log")
pyterm_log_path = os.path.join(args.log_dir, f"adversary.{hostname}.pyterm.log")

print(f"Logging motelist to {motelist_log_path}", flush=True)
print(f"Logging flash to {flash_log_path}", flush=True)
print(f"Logging pyterm to {pyterm_log_path}", flush=True)

with open(motelist_log_path, 'w') as motelist_log:
    teed = Teed()
    motelist = Popen(
        f"motelist --mote-type {parser.mote_type}",
        cwd="tools/deploy",
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
        encoding="utf-8",
    )
    teed.add(motelist,
             stdout=[motelist_log, StreamNoTimestamp(sys.stdout)],
             stderr=[motelist_log, StreamNoTimestamp(sys.stderr)])
    teed.wait()
    motelist.wait()

time.sleep(0.1)

with open(flash_log_path, 'w') as flash_log:
    teed = Teed()
    flash = Popen(
        f"python3 flash.py '{args.mote}' adversary.bin {args.mote_type} {args.firmware_type}",
        cwd="tools/deploy",
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
        encoding="utf-8",
    )
    teed.add(flash,
             stdout=[flash_log, StreamNoTimestamp(sys.stdout)],
             stderr=[flash_log, StreamNoTimestamp(sys.stderr)])
    teed.wait()
    flash.wait()
    print("Flashing finished!", flush=True)

time.sleep(0.1)

with open(pyterm_log_path, 'w') as pyterm_log, \
     MonitorBase(f"adversary.{hostname}", log_dir=args.log_dir) as pcap_monitor:
    teed = Teed()

    # stdin=subprocess.PIPE is needed in order to ensure that a stdin handle exists.
    # This is because this script may be called under nohup in which case stdin won't exist.
    pyterm = Popen(
        f"python3 pyterm -b 115200 -p {args.mote}",
        cwd="tools/deploy",
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        stdin=subprocess.PIPE,
        universal_newlines=True,
        encoding="utf-8",
    )
    teed.add(pyterm,
             stdout=[pcap_monitor, pyterm_log, StreamNoTimestamp(sys.stdout)],
             stderr=[pcap_monitor, pyterm_log, StreamNoTimestamp(sys.stderr)])
    teed.wait()
    pyterm.wait()
    print("pyterm finished!", flush=True)
