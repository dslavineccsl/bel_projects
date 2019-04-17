bel_projects
============




Current PCI/PCIe core architecture


In current implementation of the kernel module and VHDL the wishbone kernel module acts as EB slave and then through pcie_wb kernel module and FIFO directly accesses XWB in monster.

Here WB kernel module works as EB slave where:
 - when Etherbone/SW writes EB header, the kernel module itself responds back to Etherbone layer/SW with EB header response. Here there is no PCI bus transaction and no XWB transactions yet
 - when Etherbone/SW writes EB records, the kernel module unpacks EB records and opens XWB cycle for each EB record and executes XWB writes/reads inside EB record and then closes XWB cycle

Drawbacks:
 - XWB cycle line is controlled by wishbone kernel module via register in PCI configuration space registers. If SW or OS gets stuck and XWB cycle was opened by WB kernel module then XWB is blocked for access via USB or Ethernet.
 - XWB cycle is opened and closed for each EB record by WB kernel module. If there are many EB records in the packet then XWB cycle opening/closing introduces a lot of overhead since it is not deterministic or the time between opening of the cycle and actual XWB write/read can vary in time.

 
Modified architecture
 
FPGA/VHDL modification
 
New eb_pci_slave module is implemented and is placed between PCIe/PCI core WB master port and XWB crossbar. This module uses same EB slave module (eb_slave_top [1]) as it is used in USB [2] and Ethernet [3] slave cores. It works as EB slave on the USB/Ethernet/PCI side (connected to WB master on the PCIe/PCI core) and as master on XWB side (connected to XWB top crossbar as master). Because eb_slave_top works on XWB clock it needs clock domain crossing to PCI/PCIe clock domain which is done with FIFOs and some glue logic.

Cycle signal on EB RX port of the eb_slave_top module is driven by register in the configuration space of the eb_pci_slave. Also a timeout counter for clearing EB cycle is implemented in case Etherbone/SW would not close EB cycle. Timeout counter is reset when there is activity between PCI and EB slave or when it exceeds predefined maximum set in the configuration register of the eb_pci_slave.

Existing registers and FIFO in PCI module configuration space related to MSI functionality are kept as they are.
   

Wishbone kernel module modification
 
Since eb_slave_top module expects EB packet during EB cycle on its RX port this means that EB cycle line on RX port needs to be driven by someone. This is now implemented in same way as in USB kernel module. When Etherbone/SW calls wishbone open function then WB kernel module opens EB cycle by writing 0xC0000000 to eb_pci_slave CFG_REG_CYCLE register. When Etherbone/SW releases connection then WB kernel module closes EB cycle by writing 0x400000000 to CFG_REG_CYCLE register.
  
Main difference in the kernel module is now in the "etherbone_master_process" function where EB packet content is not analyzed anymore, but it is just passed to EB slave via PCI kernel module where in eb_pci_slave is written into RX_FIFO_DATA fifo and then response is read back from eb_pci_slave from TX_FIFO_DATA fifo. First the EB packet header (first 8 bytes) is written to eb_pci_slave and then response is read back from eb_pci_slave and passed back to SW. Then the EB records are written to eb_pci_slave and response is read back from eb_pci_slave and passed back to Etherbone/SW.


PCI kernel module modification

New module parameter "cyctout" is added which sets default cycle timeout for "classic" XWB access via BAR1 and also EB cycle timeout for eb_pci_slave.



Testing and debugging

Initial plan was to connect eb_pci_slave to separate BAR (BAR2) and keep existing functionality on BAR0, BAR1 and then create another XWB master port on pcie_wb. This way old and new functionality can be compared in the same FPGA design.
When testing there were issues with PCIe IP core where it looked like bar_hit signal from PCIe core did not work as expected when there were more that two BARs. After several tries of various BAR configurations without success I just mapped eb_pci_slave to address range in the BAR0 by increasing BAR0 address space from 7 to 8 bits and using address bit7 to select current configuration space registers (bit7=0) or eb_pci_slave. 

In the kernel module etherbone_master_process function is duplicated as etherbone_master_process_ebs and is then modified to talk to eb_pci_slave in the FPGA.
Kernel module parameter selebslv in the wishbone.c kernel module is used to switch between old and new functionality. Switching can be done without reloading kernel module by 

echo 1 > /sys/module/wishbone/parameters/selebslv

to select access to XWB via new eb_pci_slave or 

echo 0 > /sys/module/wishbone/parameters/selebslv

to select access to XWB via previous direct access via BAR1.


For testing of the VHDL functionality independently of the kernel module and SW, pcimem was used in combination with setpci to enable Memory Space access. 




check if FTRN PCI is enabled for PCI Memory Space access
[root@ftrn-test-box pcie-wb]# setpci -s 02:00 COMMAND
0000

If it is not (bit1 is 0) then enable it by
[root@ftrn-test-box pcie-wb]# setpci -s 02:00 COMMAND=0002
[root@ftrn-test-box pcie-wb]# setpci -s 02:00 COMMAND
0002

Now pcimem can be used to do reads and writes to XWB

example for reading and writing : open XWB cycle to make XWB reads or writes via classic XWB access
pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0x0 w 0xC0000000
  
then make read from LM32-USER-RAM
  
[root@ftrn-test-box pcie-wb]# lspci | grep CERN
02:00.0 Class 6800: CERN/ECP/EDU Device 019a (rev 01)

Optionally: remove card from PCI bus if new FPGA image will be loaded to FPGA  
[root@ftrn-test-box pcie-wb]# echo 1 > /sys/bus/pci/devices/0000\:02\:00.0/remove

Optionally: Load new FPGA image then rescan the bus
[root@ftrn-test-box pcie-wb]# echo 1 > /sys/bus/pci/rescan

Check if PCI core in FPGA is enabled for Memory Space access
[root@ftrn-test-box pcie-wb]# setpci -s 02:00 COMMAND
0000

If it is not (bit1 is 0) then enable it by
[root@ftrn-test-box pcie-wb]# setpci -s 02:00 COMMAND=0002
[root@ftrn-test-box pcie-wb]# setpci -s 02:00 COMMAND
0002

Write XWB address bits 31:16 to WINDOW_OFFSET_LOW register (pcie_wb kernel module does this)
[root@ftrn-test-box pcie-wb]# pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0x14 w 0x04060000
Written 0x 4060000; readback 0x 4060000

Open XWB cycle
[root@ftrn-test-box pcie-wb]# pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0x0 w 0xC0000000
Written 0xC0000000; readback 0x80000000

Read first address of LM32-USER-RAM
[root@ftrn-test-box pcie-wb]# pcimem /sys/bus/pci/devices/0000:02:00.0/resource1 0x0 w
Value at offset 0x0 : 0x90C00000

Close XWB cycle
[root@ftrn-test-box pcie-wb]# pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0x0 w 0x40000000
Written 0x40000000; readback 0x       0

Open XWB cycle
[root@ftrn-test-box pcie-wb]# pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0x0 w 0xC0000000
Written 0xC0000000; readback 0x80000000

Write some value to LM32-USER-RAM (by default pcimem does also readback)
[root@ftrn-test-box pcie-wb]# pcimem /sys/bus/pci/devices/0000:02:00.0/resource1 0x0 w 0xc007babe
Written 0xC007BABE; readback 0xC007BABE

Close XWB cycle
[root@ftrn-test-box pcie-wb]# pcimem /sys/bus/pci/devices/0000:02:00.0/resource0 0x0 w 0x40000000
Written 0x40000000; readback 0x       0
 
  
  
  
[1] https://www.ohwr.org/project/etherbone-core/blob/master/hdl/eb_slave_core/eb_slave_top.vhd

[2] https://www.ohwr.org/project/etherbone-core/blob/master/hdl/eb_usb_core/ez_usb.vhd [:eb_raw_slave:eb_slave_top]

[3] https://www.ohwr.org/project/etherbone-core/blob/master/hdl/eb_master_core/eb_master_slave_wrapper.vhd [:eb_ethernet_slave:eb_slave_top]
