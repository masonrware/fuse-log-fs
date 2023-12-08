#!/bin/bash

# Unmount existing filesystem
fusermount -u mnt

# Clean project
make clean

# Remove existing disk and mount directory
rm -rf disk
rm -rf mnt

# Recompile project
make

# Create disk
./create_disk.sh

# Initialize filesystem
./mkfs.wfs disk

# Create mount directory
mkdir mnt
