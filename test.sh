#!/bin/sh
set -e

reset

rsync -avr cfriedt@192.168.1.26:/Users/cfriedt/workspace/pci-edu/ ./

make -j clean
make -j

if [ "$(lsmod | grep pci_edu)" != "" ]; then
    echo "Removing pci-edu module"
    sudo rmmod -f pci-edu
fi

echo "loading pci-edu module"
sudo insmod pci-edu.ko

if [ "$(lsmod | grep pci_edu)" != "" ]; then
    echo "running dmesg" 
    sudo dmesg

    echo "Removing pci-edu module"
    sudo rmmod -f pci-edu

    echo "running dmesg" 
    sudo dmesg
fi

