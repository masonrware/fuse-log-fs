#!/usr/bin/python3

import time
import subprocess

import os
import shutil

TEST_DIR = '/home/cs537-1/tests/P7'
READONLY_PREBUILT_DISK = f'{TEST_DIR}/prebuilt_disk'
TEST_PREBUILT_DISK = 'test_prebuilt_disk'
NEW_DISK_PATH = f'd'

MOUNT_POINT = os.path.abspath('mnt')

def run_command(command, err_msg):
    # Run the command and check if it returns 0
    try:
        ret = subprocess.run(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
    except subprocess.TimeoutExpired:
        print(f'Command {command} timed out')
        return False
    if ret.returncode != 0:
        print(err_msg)
        return False
    return True

def main():
    # Mount the prebuilt image system
    shutil.copyfile(READONLY_PREBUILT_DISK, TEST_PREBUILT_DISK)
    run_command(f'./mount.wfs -s {TEST_PREBUILT_DISK} {MOUNT_POINT}', './mount.wfs returned non-zero exit code')

if __name__ == "__main__":
    main()
