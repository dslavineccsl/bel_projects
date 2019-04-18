#!/bin/bash


#open cycle
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0x80 w 0xC0000000

#write eb read packet
echo "Writing EB read packet header"
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x4e6f11ff
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x00000086

echo "Press enter to continue"
read -s -n 1 

sleep 1

#read eb read response header
echo "Reading EB response to EB read packet Header"
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xB0 w
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xB0 w
#pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xB0 w

echo "Press enter to continue"
read -s -n 1 

pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0xa00f0003
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x00008000
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x04060000
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x04060008
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x04060010

#pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0xa00f0001
#pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x00008000
#pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x04060010

pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0xe80f0001
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x00008001
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x00000004

#close cycle
echo "Closing cycle"
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0x80 w 0x40000000


sleep 1
# read response
echo "Reading response"

for (( i=1; i<=8; i++ ))
do
    pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xB0 w
done
