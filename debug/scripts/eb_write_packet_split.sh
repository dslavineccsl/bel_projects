#!/bin/bash


#open cycle
echo "Open Cycle"
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0x80 w 0xC0000000

echo "Press enter to continue"
read -s -n 1 


#echo "Flushing TX FIFO"
#pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xB0 w
#pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xB0 w
#pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xB0 w

#write eb write packet header
echo "Writing EB write packet Header"
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x4e6f11ff
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x00000086

echo "Press enter to continue"
read -s -n 1 


#write eb read response header
echo "Reading EB response to write packet Header"
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xB0 w
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xB0 w
#pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xB0 w

echo "Press enter to continue"
read -s -n 1 


sleep 0.5
echo "Writing EB write packet EB records"
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0xe00f0101
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x04060000
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x11111111
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x00008001
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x00000004
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0xe00f0101
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x04060008
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x22222222
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x00008003
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x00000004
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0xe80f0101
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x04060010
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x33333333
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x00008005
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xA0 w 0x00000004

echo "Press enter to continue"
read -s -n 1 

sleep 1
#close cycle
echo "Closing cycle"
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0x80 w 0x40000000

sleep 2

# read response
echo "Reading response"
sleep 2

for (( i=1; i<=15; i++ ))
do
    pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0xB0 w
done

