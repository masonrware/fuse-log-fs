#!/usr/bin/python3

import time
import subprocess
import os
import shutil

TEST_DIR = '/home/cs537-1/tests/P7'
READONLY_PREBUILT_DISK = f'{TEST_DIR}/prebuilt_disk'
TEST_PREBUILT_DISK = 'test_prebuilt_disk'
NEW_DISK_PATH = f'disk'

CC = 'gcc'
CFLAGS = '-Wall -Werror -pedantic -std=gnu18'

MKFS_TEST_NUM = 1
FSCK_TEST_NUM = 10

MOUNT_POINT = os.path.abspath('mnt')

# tests on prebuilt image
readonly_tests = [0]

# tests on new image
new_image_tests = list(range(2, 10))


passed_tests, total_tests = 0, 0

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

def compile():
    # Compile students' code
    run_command(f'{CC} {CFLAGS} mount.wfs.c `pkg-config fuse --cflags --libs` -o mount.wfs', 'Failed to compile mount.wfs.c')
    run_command(f'{CC} {CFLAGS} -o mkfs.wfs mkfs.wfs.c', 'Failed to compile mkfs.wfs.c')
    if 'fsck.wfs.c' in os.listdir():
        run_command(f'{CC} {CFLAGS} -o fsck.wfs fsck.wfs.c', 'Failed to compile fsck.wfs.c')
    else:
        print('No fsck.wfs.c found, skipping compilation.')

def run_single_test(test_number):

    run_command = f"{TEST_DIR}/{test_number}"
    try:
        run_process = subprocess.run(run_command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
    except subprocess.TimeoutExpired:
        print(f'Test {test_number} timed out')
        return False

    # Check the return value of the executable
    if run_process.returncode == 0:
        return True
    else:
        print(f'Test {test_number} failed with exit code {run_process.returncode}')
        print(f'Ouput: {run_process.stdout}\nError: {run_process.stderr}')
        return False

def run_tests(test_num_list):
    for i in test_num_list:
        with open(f'{TEST_DIR}/{i}.desc', 'r') as f:
            desc = f.read()
        print(f'Test {i}: {desc}')
        test_result = run_single_test(i)
        global passed_tests, total_tests
        if test_result:
            passed_tests += 1
        total_tests += 1
        str_result = 'passed' if test_result else 'failed'
        print(f'{str_result}\n\n')

def create_image(file_size=1024*1024):
    # Create a new file with the specified size
    if os.path.exists(NEW_DISK_PATH):
        os.remove(NEW_DISK_PATH)

    with open(NEW_DISK_PATH, 'wb') as file:
        file.truncate(file_size)

def is_mounted():
    return subprocess.run(['grep', '-qs', MOUNT_POINT, '/proc/mounts']).returncode == 0

def umount():
    mounted = is_mounted()
    if mounted:
        subprocess.run(['fusermount', '-u', MOUNT_POINT])

def mkdir_mnt():
    if not os.path.exists(MOUNT_POINT):
        os.mkdir(MOUNT_POINT)

def main():

    compile()

    umount()
    mkdir_mnt()

    # Mount the prebuilt image system
    shutil.copyfile(READONLY_PREBUILT_DISK, TEST_PREBUILT_DISK)
    run_command(f'./mount.wfs -s {TEST_PREBUILT_DISK} {MOUNT_POINT}', './mount.wfs returned non-zero exit code')
    if not is_mounted():
        print('Failed to mount the prebuilt file system')
        return

    run_tests(readonly_tests)
    
    # Unmount the file system and rm test_prebuilt_disk
    time.sleep(1)
    umount()
    os.remove(TEST_PREBUILT_DISK)

    # Tests on new image

    create_image()
    
    # Format the file system
    run_command(f'./mkfs.wfs {NEW_DISK_PATH}', './mkfs.wfs returned non-zero exit code')

    # Test the formatted file system
    run_tests([MKFS_TEST_NUM])

    # Mount the file system
    run_command(f'./mount.wfs -s {NEW_DISK_PATH} {MOUNT_POINT}', './mount.wfs returned non-zero exit code')
    if not is_mounted():
        print('Failed to mount the empty file system')
        return
    
    # Run the tests
    run_tests(new_image_tests)

    # Unmount the file system
    time.sleep(1)
    umount()

    if 'fsck.wfs.c' in os.listdir():
        create_image(1024)
        run_command(f'./mkfs.wfs {NEW_DISK_PATH}', './mkfs.wfs returned non-zero exit code')
        run_command(f'./mount.wfs -s {NEW_DISK_PATH} {MOUNT_POINT}', './mount.wfs returned non-zero exit code')
        if not is_mounted():
            print('Failed to mount the empty file system')
            return
        run_tests([FSCK_TEST_NUM])
        time.sleep(1)
        umount()

    
    try:
        os.rmdir(MOUNT_POINT)
    except OSError as e:
        print(f"Error: {e}")

    print(f'{passed_tests} / {total_tests} tests passed')

if __name__ == "__main__":
    main()
