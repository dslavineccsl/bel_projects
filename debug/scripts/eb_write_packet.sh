#!/bin/bash


#open cycle
echo "Opening EB cycle"
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0x80 w 0xC0000000

echo "Flushing TX FIFO"
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xB0 w
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xB0 w
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xB0 w

#write eb write packet
echo "Writing EB write packet"
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xA0 w 0x4e6f11ff
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xA0 w 0x00000086
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xA0 w 0xe80f0101
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xA0 w 0x04060000
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xA0 w 0xB00BBABE
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xA0 w 0x00008001
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xA0 w 0x00000004

#close cycle
echo "Closing EB cycle"
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0x80 w 0x40000000

sleep 2

# read response
echo "Reading response"
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xB0 w
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xB0 w
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xB0 w
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xB0 w
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xB0 w
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xB0 w
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xB0 w
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xB0 w
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xB0 w
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xB0 w


