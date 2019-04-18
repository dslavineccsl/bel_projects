#!/bin/bash


#open EB cycle to EB slave
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0x80 w 0xC0000000

#write eb read packet
echo "Writing EB read packet"
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xA0 w 0x4e6f11ff
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xA0 w 0x00000086
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xA0 w 0xa00f0001
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xA0 w 0x00008000
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xA0 w 0x04060000
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xA0 w 0xe80f0001
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xA0 w 0x00008001
pcimem /sys/bus/pci/devices/0000:02:00.0/resource2 0xA0 w 0x00000004

#close cycle EB cycle
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
