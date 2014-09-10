#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "irq.h"
#include "scu_bus.h"
#include "aux.h"
#include "mini_sdb.h"
#include "board.h"
#include "uart.h"
#include "w1.h"
#include "fg.h"
#include "cb.h"
#include "scu_mil.h"

//#define DEBUG
//#define FGDEBUG
//#define CBDEBUG

extern struct w1_bus wrpc_w1_bus;

#define SHARED __attribute__((section(".shared")))
uint64_t SHARED board_id = -1;
uint32_t SHARED board_temp = -1;
uint64_t SHARED ext_id = -1;
uint32_t SHARED ext_temp = -1; 
uint64_t SHARED backplane_id = -1;
uint32_t SHARED backplane_temp = -1;
uint32_t SHARED fg_magic_number = 0xdeadbeef;
uint32_t SHARED fg_version = 0x1;
struct circ_buffer SHARED fg_buffer[MAX_FG_DEVICES]; 
uint32_t SHARED num_fgs_found;  // shows the number of found fgs on scu bus
struct fg_status SHARED fgstat; // shows the status for choosen fg. switched by SWI function

struct scu_bus scub;
struct fg_list fgs;
struct fg_dev wb_fg_dev;

volatile unsigned short* scub_base;
volatile unsigned int* BASE_ONEWIRE;
volatile unsigned int* BASE_UART;
volatile unsigned int* scub_irq_base;
volatile unsigned int* wb_fg_irq_base;
volatile unsigned int* wb_fg_base;
volatile unsigned int* scu_mil_base;
sdb_location lm32_irq_endp[10]; // there are two queues for msis

volatile unsigned int param_sent[MAX_FG_DEVICES];
volatile int initialized[MAX_SCU_SLAVES] = {0};

void usleep(int us)
{
  unsigned i;
  unsigned long long delay = us;
  /* prevent arithmetic overflow */
  delay *= CPU_CLOCK;
  delay /= 1000000;
  delay /= 4; // instructions per loop
  for (i = delay; i > 0; i--) asm("# noop");
}  

void msDelay(int msecs) {
  usleep(1000 * msecs);
}


void show_msi()
{
  char buffer[12];
  
  mat_sprinthex(buffer, global_msi.msg);
  disp_put_str("D ");
  disp_put_str(buffer);
  disp_put_c('\n');

  
  mat_sprinthex(buffer, global_msi.adr);
  disp_put_str("A ");
  disp_put_str(buffer);
  disp_put_c('\n');

  
  mat_sprinthex(buffer, (unsigned long)global_msi.sel);
  disp_put_str("S ");
  disp_put_str(buffer);
  disp_put_c('\n');
}

void send_fg_param(int slave_nr, int fg_base) {
  struct param_set pset;
  int fg_number, add_freq_sel, step_cnt_sel;
  
  fg_number = (scub_base[(slave_nr << 16) + fg_base + FG_CNTRL] & 0x3f0) >> 4; // virtual fg number Bits 9..4
  mprintf("fg_number called: %d\n", fg_number);
  if(!cbisEmpty((struct circ_buffer *)&fg_buffer, fg_number)) {
    cbRead((struct circ_buffer *)&fg_buffer, fg_number, &pset);
    step_cnt_sel = pset.control & 0x7;
    add_freq_sel = (pset.control & 0x38) >> 3;
    scub_base[(slave_nr << 16) + fg_base + FG_CNTRL] &= ~(0xfc00); // clear freq and step select
    scub_base[(slave_nr << 16) + fg_base + FG_CNTRL] |= (add_freq_sel << 13) | (step_cnt_sel << 10);
    scub_base[(slave_nr << 16) + fg_base + FG_A] = pset.coeff_a;
    scub_base[(slave_nr << 16) + fg_base + FG_SHIFTA] = (pset.control & 0x1f000) >> 12;
    scub_base[(slave_nr << 16) + fg_base + FG_B] = pset.coeff_b;
    scub_base[(slave_nr << 16) + fg_base + FG_SHIFTB] = (pset.control & 0xfc0) >> 6;
    //scub_base[(slave_nr << 16) + fg_base + FG_STARTL] = pset.coeff_c & 0xffff;
    //scub_base[(slave_nr << 16) + fg_base + FG_STARTH] = (pset.coeff_c & 0xffff0000) >> 16; // data written with high word
    param_sent[fg_number]++;
  }
}

void slave_irq_handler()
{
  int j = 0;
  char buffer[12];
  unsigned char slave_nr = global_msi.adr>>2 & 0xf;
  volatile unsigned short tmr_irq_cnts; 
  static unsigned short old_tmr_cnt[MAX_SCU_SLAVES];
  volatile unsigned int slv_int_act_reg = scub_base[(slave_nr << 16) + SLAVE_INT_ACT];
  unsigned int slave_acks = 0;
  struct fg_dev* fg1 = 0;
  struct fg_dev* fg2 = 0;


  if (slave_nr < 0 || slave_nr > MAX_SCU_SLAVES) {
    mprintf("unknown IRQ number.\n");
    return;
  }

  j = 0;
  while(scub.slaves[slave_nr-1].devs[j].version) { /* more fgs in list */
    if (scub.slaves[slave_nr-1].devs[j].dev_number == 0) 
      fg1 = &scub.slaves[slave_nr-1].devs[j];
    if (scub.slaves[slave_nr-1].devs[j].dev_number == 1) 
      fg2 = &scub.slaves[slave_nr-1].devs[j];

    j++; 
  } 

  if (slv_int_act_reg & 0x1) { //powerup irq?
    mprintf("ack powerup irq in slave %d\n", slave_nr);
    slave_acks |= 0x1; //ack powerup irq
  }

  if (slv_int_act_reg & 0x2000) { //tmr irq?
    tmr_irq_cnts = scub_base[(slave_nr << 16) + TMR_BASE + TMR_IRQ_CNT];
    // init old_tmr_cnt
    if (!initialized[slave_nr-1]) {
      old_tmr_cnt[slave_nr-1] = tmr_irq_cnts - 1;
      mprintf("init slave: %d with %x\n", slave_nr, tmr_irq_cnts - 1); 
      initialized[slave_nr-1] = 1;
    }  
    // check for lost IRQs
    if ((tmr_irq_cnts == (unsigned short)(old_tmr_cnt[slave_nr-1] + 1))) {
      slave_acks |= (1 << 13); //ack timer irq
      old_tmr_cnt[slave_nr-1] = tmr_irq_cnts;
    } else
      mprintf("irq1 slave: %d, old: %x, act: %x\n", slave_nr, old_tmr_cnt[slave_nr-1], tmr_irq_cnts);
  }
  
  if (slv_int_act_reg & (1<<15)) { //FG1 irq?
    send_fg_param(slave_nr, FG1_BASE);
    slave_acks |= (1<<15); //ack FG1 irq
  } 
 
  if (slv_int_act_reg & (1<<14)) { //FG2 irq?
    send_fg_param(slave_nr, FG2_BASE);  
    slave_acks |= (1<<14); //ack FG2 irq
  } 

  scub_base[(slave_nr << 16) + SLAVE_INT_ACT] = slave_acks; // ack all pending irqs 
}

void wb_fg_irq_handler() {
  struct param_set pset;
  int fg_number, add_freq_sel, step_cnt_sel;
  static int first_irq;

  if (first_irq < 2) {
    first_irq++;
    return;
  }
  
  fg_number = (wb_fg_base[WB_FG_CNTRL] & 0x3f0) >> 4; // virtual fg number Bits 9..4
  if(!cbisEmpty((struct circ_buffer *)&fg_buffer, fg_number)) {
    cbRead((struct circ_buffer *)&fg_buffer, fg_number, &pset);
    step_cnt_sel = pset.control & 0x7;
    add_freq_sel = (pset.control & 0x38) >> 3;
    wb_fg_base[WB_FG_CNTRL] &= ~(0xfc00); // clear freq and step select
    wb_fg_base[WB_FG_CNTRL] |= (add_freq_sel << 13) | (step_cnt_sel << 10);
    wb_fg_base[WB_FG_A] = pset.coeff_a;
    wb_fg_base[WB_FG_SHIFTA] = (pset.control & 0x1f000) >> 12;
    wb_fg_base[WB_FG_B] = pset.coeff_b;
    wb_fg_base[WB_FG_SHIFTB] = (pset.control & 0xfc0) >> 6;
    param_sent[fg_number]++;
  }
  mprintf("fg[%d] wb_fg serviced\n", fg_number);
  mprintf("wb_fg: 0x%x\n", global_msi.adr);
  mprintf("       0x%x\n", global_msi.msg);
}

void enable_msi_irqs() {
  int i;
  //SCU Bus Master
  scub_base[GLOBAL_IRQ_ENA] = 0x20; //enable slave irqs
  scub_irq_base[0] = 0x1; // reset irq master
  for (i = 0; i < 12; i++) {
    scub_irq_base[8] = i;  //channel select
    scub_irq_base[9] = 0x08154711;  //msg
    scub_irq_base[10] = getSdbAdr(&lm32_irq_endp[3]) + ((i+1) << 2); //destination address, do not use lower 2 bits 
  }


  i = 0;
  while(scub.slaves[i].unique_id) {
    initialized[scub.slaves[i].slot - 1] = 0; // counter needs to be resynced
    scub_irq_base[2] |= (1 << (scub.slaves[i].slot - 1)); //enable slaves
    i++;
  }
  
  //wb_fg_irq_ctrl
  //wb_fg_irq_base[0] = 0x1;                            // reset irq master
  //wb_fg_irq_base[8] = 0;                              // only one channel
  //wb_fg_irq_base[9] = 0x47110815;                     // msg
  //wb_fg_irq_base[10] = getSdbAdr(&lm32_irq_endp[5]);  //destination address, do not use lower 2 bits
  //wb_fg_irq_base[2] = 0x1;                            //enable irq channel
  mprintf("IRQs for all slave channels enabled.\n");
}


void disable_msi_irqs() {
  scub_irq_base[3] = 0xfff; //disable irqs for all channels
  wb_fg_irq_base[3] = 0x1; //disable irqs for wb_fg_quad
  mprintf("IRQs for all slave channels disabled.\n");
}

void configure_slaves(unsigned int tmr_value) {
  mprintf("configuring slaves.\n");
  int i = 0;
  int slot;
  scub_base[SRQ_ENA] = 0x0; //reset bitmask
  scub_base[MULTI_SLAVE_SEL] = 0x0; //reset bitmask  
  while(scub.slaves[i].unique_id) {
    slot = scub.slaves[i].slot;
    mprintf("enable slave[%d] in slot %d\n", i, slot);
    scub_base[SRQ_ENA] |= (1 << (slot-1));  //enable irqs for the slave
    scub_base[MULTI_SLAVE_SEL] |= (1 << (slot-1)); //set bitmask for broadcast select
    scub_base[(slot << 16) + SLAVE_INT_ENA] = 0x2000; //enable tmr irq in slave macro
    scub_base[(slot << 16) + TMR_BASE + TMR_CNTRL] = 0x1; //reset TMR
    scub_base[(slot << 16) + TMR_BASE + TMR_VALUEL] = tmr_value & 0xffff; //enable generation of tmr irqs, 1ms, 0xe848
    scub_base[(slot << 16) + TMR_BASE + TMR_VALUEH] = tmr_value >> 16; //enable generation of tmr irqs, 1ms, 0x001e
    i++;
  }
}

void ack_pui() {
  int i = 0;
  int slot;
  while(scub.slaves[i].unique_id) {
    slot = scub.slaves[i].slot;
    scub_base[(slot << 16) + SLAVE_INT_ACT] = 0x1;
    i++;
  }
}

void configure_fgs() {
  int i = 0, j = 0;
  int slot;
  struct param_set pset;
  int add_freq_sel, step_cnt_sel;
  mprintf("configuring fgs.\n");
  while(scub.slaves[i].unique_id) { /* more slaves in list */
    /* actions per slave card */
    slot = scub.slaves[i].slot;
    /* only receive irqs from slaves with fg devices */
    if (scub.slaves[i].devs[j].version) {
      scub_base[SRQ_ENA] |= (1 << (slot-1));  //enable irqs for the slave
      scub_base[MULTI_SLAVE_SEL] |= (1 << (slot-1)); //set bitmask for broadcast select
    }
    if(scub.slaves[i].cid_group == 3 || scub.slaves[i].cid_group == 38) { /* ADDAC -> 2 FGs */
      scub_base[(slot << 16) + SLAVE_INT_ENA] |= 0xc000; /* enable fg1 and fg2 irq */
      scub_base[(slot << 16) + DAC1_BASE + DAC_CNTRL] = 0x10; // set FG mode
      scub_base[(slot << 16) + DAC2_BASE + DAC_CNTRL] = 0x10; // set FG mode
    } else if (scub.slaves[i].cid_group == 26) {
      scub_base[(slot << 16) + SLAVE_INT_ENA] |= 0x8000; /* enable fg1 irq */
      scub_base[(slot << 16) + DAC1_BASE + DAC_CNTRL] = 0x10; // set FG mode
    }
    j = 0;
    while(scub.slaves[i].devs[j].version) { /* more fgs in this device */
      /* actions per fg */
      //mprintf("enable fg[%d] in slot %d\n", scub.slaves[i].devs[j].dev_number, slot);
      scub_base[(slot << 16) + scub.slaves[i].devs[j].offset + FG_CNTRL] = 0x1; // reset fg
      j++;
    }
    i++;
  }
  i = 0;
  while(fgs.devs[i]) { 
    slot = fgs.devs[i]->slave->slot;
    //set virtual fg number Bit 9..4
    //mprintf("virtual fg %d in slot %d\n", i, slot);
    scub_base[(slot << 16) + fgs.devs[i]->offset + FG_CNTRL] |= (i << 4);
    //fetch parameter set from buffer
    if(!cbisEmpty((struct circ_buffer *)&fg_buffer, i)) {
      cbRead((struct circ_buffer *)&fg_buffer, i, &pset);
      step_cnt_sel = pset.control & 0x7;
      add_freq_sel = (pset.control & 0x38) >> 3;
      scub_base[(slot << 16) + fgs.devs[i]->offset + FG_CNTRL] |= add_freq_sel << 13 | step_cnt_sel << 10;
      scub_base[(slot << 16) + fgs.devs[i]->offset + FG_A] = pset.coeff_a;
      scub_base[(slot << 16) + fgs.devs[i]->offset + FG_SHIFTA] = (pset.control & 0x1f000) >> 12;
      scub_base[(slot << 16) + fgs.devs[i]->offset + FG_B] = pset.coeff_b;
      scub_base[(slot << 16) + fgs.devs[i]->offset + FG_SHIFTB] = (pset.control & 0xfc0) >> 6;
      scub_base[(slot << 16) + fgs.devs[i]->offset + FG_STARTL] = pset.coeff_c & 0xffff;
      scub_base[(slot << 16) + fgs.devs[i]->offset + FG_STARTH] = (pset.coeff_c & 0xffff0000) >> 16; // data written with high word
      param_sent[i]++;
    }
    fgs.devs[i]->running = 1;
    i++;
  } 
} 

void reset_slaves() {
  mprintf("resetting slaves.\n");
  int i = 0;
  scub_base[SRQ_ENA] = 0x0; //reset bitmask
  scub_base[MULTI_SLAVE_SEL] = 0x0; //reset bitmask  
  while(scub.slaves[i].unique_id) {
    disp_put_c('x');
    scub_base[(scub.slaves[i].slot << 16) + TMR_BASE + TMR_CNTRL] = 0x1; //reset TMR
    i++;
  }
}

/* scans for slaves and then for fgs */
void print_fgs() {
  int i=0, j=0;
  scan_scu_bus(&scub, backplane_id, scub_base);
  num_fgs_found = scan_for_fgs(&scub, &fgs);
  mprintf("ID: 0x%08x%08x\n", (int)(scub.unique_id >> 32), (int)scub.unique_id); 
  while(scub.slaves[i].unique_id) { /* more slaves in list */ 
      mprintf("slaves[%d] ID:  0x%08x%08x\n",i, (int)(scub.slaves[i].unique_id>>32), (int)scub.slaves[i].unique_id); 
      mprintf("slv ver: 0x%x cid_sys: %d cid_grp: %d\n", scub.slaves[i].version, scub.slaves[i].cid_sys, scub.slaves[i].cid_group); 
      mprintf("slot:        %d\n", scub.slaves[i].slot); 
      j = 0; 
      while(scub.slaves[i].devs[j].version) { /* more fgs in list */ 
        mprintf("   fg[%d], version 0x%x \n", j, scub.slaves[i].devs[j].version); 
        j++; 
      } 
      i++; 
  }
  i = 0;
  mprintf("Found %d FGs:\n", num_fgs_found);
  while(fgs.devs[i]) {
    if (fgs.devs[i]->version == 0x1)
      mprintf("fg[%d] slot: %d \n", i, fgs.devs[i]->slave->slot);
    else if (fgs.devs[i]->version == 0x2)
      mprintf("fg[%d] wb_fg_quad\n", i);
    else
      mprintf("fg[%d] unknown fg dev\n");
    i++;
  } 
}

void sw_irq_handler() {
  int i;
  struct param_set pset;
  switch(global_msi.adr>>2 & 0xf) {
    case 0:
      disable_msi_irqs();  
      init_buffers(&fg_buffer);
      for (i=0; i < MAX_FG_DEVICES; i++) // clear statistics
        param_sent[i] = 0;
    break;
    case 1:
      configure_slaves(global_msi.msg);
      enable_msi_irqs();
      scub_base[(0xd << 16) + TMR_BASE + TMR_CNTRL] = 0x2; //multicast tmr enable
    break;
    case 2:
      disable_msi_irqs();
      print_fgs();
    break;
    case 3:
      enable_msi_irqs();
      configure_fgs();
      //wb_fg_base[WB_FG_BROAD] = 0x4711; //start fg in scu
      mprintf("broadcast to wb_fg\n");
      scub_base[(0xd << 16) + FG1_BASE + FG_BROAD] = 0x4711; //start all FG1s (and linked FG2s)
      mprintf("broadcast to slaves\n");
    break;
    case 4:
      for (i = 0; i < MAX_FG_DEVICES; i++)
        mprintf("fg[%d] buffer: %d param_sent: %d\n", i, cbgetCount((struct circ_buffer *)&fg_buffer, i), param_sent[i]);
    break;
    case 5:
      if (global_msi.msg >= 0 && global_msi.msg < MAX_FG_DEVICES) {
        if(!cbisEmpty((struct circ_buffer *)&fg_buffer, global_msi.msg)) {
          cbRead((struct circ_buffer *)&fg_buffer, global_msi.msg, &pset);
          mprintf("read buffer[%d]: a %d, l_a %d, b %d, l_b %d, c %d, n %d\n",
                   global_msi.msg, pset.coeff_a, (pset.control & 0x1f000) >> 12, pset.coeff_b,
                   (pset.control & 0xfc0) >> 6, pset.coeff_c, (pset.control & 0x7));
        } else {
          mprintf("read buffer[%d]: buffer empty!\n", global_msi.msg);
        }
          
      }
    break;
    case 6:
      run_mil_test(scu_mil_base, global_msi.msg & 0xff);
    break;   
    default:
      mprintf("swi: 0x%x\n", global_msi.adr);
      mprintf("     0x%x\n", global_msi.msg);
    break;
  }
}

void error_handler() {
  mprintf("timer isr called!\n");
}

void init_irq_handlers() {
  isr_table_clr();
  isr_ptr_table[0] = &error_handler;
  isr_ptr_table[1] = &slave_irq_handler;
  isr_ptr_table[2] = &sw_irq_handler;
  isr_ptr_table[3] = &wb_fg_irq_handler;  
  irq_set_mask(0x0e);
  irq_enable();
  mprintf("MSI IRQs configured.\n");
}

void init() {
  uart_init_hw();           //enables the uart for debug messages
  print_fgs();              //scans for slave cards and fgs
  //ack_pui();              //ack slave powerup irqs
  init_buffers(&fg_buffer); //init the ring buffer
  init_irq_handlers();      //enable the irqs
} 

int main(void) {
  char input;
  int i, j;
  int idx = 0;
  int slot;
  int fg_is_running[MAX_FG_DEVICES];
  discoverPeriphery();  
  scub_base     = (unsigned short*)find_device_adr(GSI, SCU_BUS_MASTER);
  BASE_ONEWIRE  = (unsigned int*)find_device_adr(CERN, WR_1Wire);
  scub_irq_base = (unsigned int*)find_device_adr(GSI, SCU_IRQ_CTRL); // irq controller for scu bus
  wb_fg_irq_base = (unsigned int*)find_device_adr(GSI, WB_FG_IRQ_CTRL); // irq controller for wb_fg
  find_device_multi(lm32_irq_endp, &idx, 10, GSI, IRQ_ENDPOINT);
  scu_mil_base = (unsigned int*)find_device(SCU_MIL);
  wb_fg_base = (unsigned int*)find_device_adr(GSI, WB_FG_QUAD);

  

  disp_reset();
  disp_put_c('\f');
  
  usleep(1500000); //wait for powerup of the slave cards
  init(); 

  mprintf("number of irq_endpoints found: %d\n", idx);
  for (i=0; i < idx; i++) {
    mprintf("irq_endp[%d] is: 0x%x\n",i, getSdbAdr(&lm32_irq_endp[i]));
  }
  mprintf("scub_irq_base is: 0x%x\n", scub_irq_base);
  mprintf("wb_fg_irq_base is: 0x%x\n", wb_fg_irq_base); 
  mprintf("wb_fg_base is: 0x%x\n", wb_fg_base); 

  while(1); /*FIXME*/
  //while(1);
  while(1) { 
    i = 0; 
    while(scub.slaves[i].unique_id) { /* more slaves in list */ 
      slot = scub.slaves[i].slot;
      j = 0; 
      while(scub.slaves[i].devs[j].version) { /* more fgs in list */
        if ((scub_base[(slot << 16) + scub.slaves[i].devs[j].offset + FG_CNTRL] & 0x8) > 0) // fg stopped
          if (scub.slaves[i].devs[j].running > 0) { //report only once
            scub.slaves[i].devs[j].endvalue = scub_base[(slot << 16) + scub.slaves[i].devs[j].offset + 0x8]; // last cnt from from fg macro
            mprintf("fg %d in slot %d stopped at ramp value 0x%x.\n",
                     scub.slaves[i].devs[j].dev_number, slot, scub.slaves[i].devs[j].endvalue);
            scub.slaves[i].devs[j].running = 0;
          }  
        j++; 
      } 
      i++; 
    }
    
  }

  return(0);
}
