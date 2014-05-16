#include <unistd.h> /* getopt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <etherbone.h>
#include "xmlaux.h"
#include "ftmx86.h"


#define MAX_DEVICES  100
#define BUF_SIZE     0x600
#define PACKET_SIZE  1500

const char* program;
eb_device_t device;
eb_socket_t mySocket;

const    uint32_t devID_RAM 	      = 0x66cfeb52;
const    uint64_t vendID_CERN       = 0x000000000000ce42;
const    uint32_t devID_ClusterInfo = 0x10040086;
const    uint64_t vendID_GSI        = 0x0000000000000651;
char              devName_RAM_pre[] = "WB4-BlockRAM_";
		
volatile uint32_t embeddedOffset;



int ebRamOpen(const char* netaddress, uint8_t cpuId);
int ebRamRead(uint32_t address, uint32_t len, const uint8_t* buf);
int ebRamWrite(const uint8_t* buf, uint32_t address, uint32_t len);
int ebRamClose(void);


int ebRamOpen(const char* netaddress, uint8_t cpuId)
{
   
   eb_status_t status;
   int idx;
   int attempts;
   int num_devices;
   struct sdb_device devices[MAX_DEVICES];
   char              devName_RAM_post[4];
   
   attempts   = 3;
   idx        = -1;
         printf("test1\n");

   
  
  /* open EB socket and device */
      if ((status = eb_socket_open(EB_ABI_CODE, 0, EB_ADDR32 | EB_DATA32, &mySocket)) != EB_OK) {
    fprintf(stderr, "%s: failed to open Etherbone socket: %s\n", program, eb_status(status));
    return 1;
  }
  if ((status = eb_device_open(mySocket, netaddress, EB_ADDR32 | EB_DATA32, attempts, &device)) != EB_OK) {
    fprintf(stderr, "%s: failed to open Etherbone device: %s\n", program, eb_status(status));
    return 1;
  }

 printf("test2\n");

  num_devices = MAX_DEVICES;
  eb_sdb_find_by_identity(device, vendID_GSI, devID_ClusterInfo, &devices[0], &num_devices);
  if (num_devices == 0) {
    fprintf(stderr, "%s: no matching devices found\n", program);
    return 1;
  }

  if (num_devices > MAX_DEVICES) {
    fprintf(stderr, "%s: more devices found that tool supports (%d > %d)\n", program, num_devices, MAX_DEVICES);
    return 1;
  }

  if (idx > num_devices) {
    fprintf(stderr, "%s: device #%d could not be found; only %d present\n", program, idx, num_devices);
    return 1;
  }

  if (idx == -1) {
    printf("Found %u devs\n", num_devices);
    
  } else {
    
    
  }
   printf("Found Cluster Info @ %08x\n", (uint32_t)devices[0].sdb_component.addr_first);

  num_devices = MAX_DEVICES;
  eb_sdb_find_by_identity(device, vendID_CERN, devID_RAM, &devices[0], &num_devices);
  if (num_devices == 0) {
    fprintf(stderr, "%s: no matching devices found\n", program);
    return 1;
  }

  if (num_devices > MAX_DEVICES) {
    fprintf(stderr, "%s: more devices found that tool supports (%d > %d)\n", program, num_devices, MAX_DEVICES);
    return 1;
  }

  if (idx > num_devices) {
    fprintf(stderr, "%s: device #%d could not be found; only %d present\n", program, idx, num_devices);
    return 1;
  }

  if (idx == -1) {
    printf("Found %u devs\n", num_devices);
    for (idx = 0; idx < num_devices; ++idx) {
      printf("%.*s 0x%"PRIx64"\n", 19, &devices[idx].sdb_component.product.name[0], devices[idx].sdb_component.addr_first);
         if(strncmp(devName_RAM_post, (const char*)&devices[idx].sdb_component.product.name[13], 3) == 0)
         printf("MATCH! found %s\n", devName_RAM_post);
         embeddedOffset = devices[idx].sdb_component.addr_first + FTM_SHARED_OFFSET;
    }
  } else {
    printf("0x%"PRIx64"\n", devices[idx].sdb_component.addr_first);
  }
 
  return 0;
}

int ebRamClose()
{

   eb_status_t status;

      if ((status = eb_device_close(device)) != EB_OK) {
       fprintf(stderr, "%s: failed to close Etherbone device: %s\n", program, eb_status(status));
       return 1;
     }

     if ((status = eb_socket_close(mySocket)) != EB_OK) {
       fprintf(stderr, "%s: failed to close Etherbone socket: %s\n", program, eb_status(status));
       return 1;
     }
     
     return 0;
  
}

int ebRamRead(uint32_t address, uint32_t len, const uint8_t* buf)
{
   
   eb_status_t status;
   eb_cycle_t cycle;
   uint32_t i,j, parts, partLen;
   uint32_t* readin = (uint32_t*)buf;	

   printf("Reading %u bytes...", len);
   //wrap frame buffer in EB packet
   parts = (len/PACKET_SIZE)+1;
   
   for(j=0; j<parts; j++)
   {
      if(j == parts-1 && (len % PACKET_SIZE != 0)) partLen = len % PACKET_SIZE;
      else partLen = PACKET_SIZE;
      
      if ((status = eb_cycle_open(device, 0, eb_block, &cycle)) != EB_OK) {
            fprintf(stderr, "%s: failed to create cycle: %s\n", program, eb_status(status));
            return 1;
         }  
      for(i=(j*partLen)>>2; i<(((j+1)*partLen)>>2);i++)  
      {
         
         eb_cycle_read(cycle, (eb_address_t)(address+(i<<2)), EB_BIG_ENDIAN | EB_DATA32,  &readin[i]); 
         
      }
      eb_cycle_close(cycle);
   }   
   printf("done\n", len);
   return 0;
}

int ebRamWrite(const uint8_t* buf, uint32_t address, uint32_t len)
{
   eb_status_t status;
   eb_cycle_t cycle;
   uint32_t i,j, parts, partLen;
   uint32_t* writeout = (uint32_t*)buf;	
   
   printf("pBuf: %p socket: %p device: %p\n", buf, &mySocket, &device);
   
   printf("Writing %u bytes...", len);
   //wrap frame buffer in EB packet
   parts = (len/PACKET_SIZE)+1;
   
   for(j=0; j<parts; j++)
   {
      if(j == parts-1 && (len % PACKET_SIZE != 0)) partLen = len % PACKET_SIZE;
      else partLen = PACKET_SIZE;
      
      printf("len: %u parts: %u PartLen %u\n", len, parts, partLen);
      if ((status = eb_cycle_open(device, 0, eb_block, &cycle)) != EB_OK) {
            fprintf(stderr, "%s: failed to create cycle: %s\n", program, eb_status(status));
            return 1;
         } 
      
      for(i=(j*partLen)>>2; i<(((j+1)*partLen)>>2);i++) 
      {
          
         //printf("idx: %u adr: %08x val: %08x\n", i, (address+(i<<2)), writeout[i]);
         eb_cycle_write(cycle, (eb_address_t)(address+(i<<2)), EB_BIG_ENDIAN | EB_DATA32, writeout[i]); 
      
      }
      eb_cycle_close(cycle);
      printf("send cycle\n");
   }
   printf("done\n");
   return 0;
}




   int main(int argc, char** argv) {
      uint32_t i, wSize;
      int opt;
      t_ftmPage* pPage = NULL;
      t_ftmPage* pNewPage = NULL;
      const char* netaddress;
      char filename[64];
      uint8_t bufWrite[BUF_SIZE];
      uint8_t* pBufWrite;
      
      uint8_t bufRead[BUF_SIZE];
      uint8_t* pBufRead = &bufRead[0];

      FILE *f;    
   
      program = argv[0];
      
      while ((opt = getopt(argc, argv, "x:")) != -1) {
         switch (opt) {
            case 'x':
               i=0;
               while(i<64 && (optarg[i] != '\0')) { filename[i] = optarg[i]; i++;}
               filename[i] = '\0';
        
            case '?':
            fprintf(stderr, "Usage: %s  <protocol/host/port> -s <\"my string\">\n", program);
            break;
            default:
            fprintf(stderr, "%s: bad getopt result\n", program);
            return 1;
         }
      }

      if (optind + 1 != argc) {
         fprintf(stderr, "%s: Usage: simple-pRam<protocol/host/port> -x <\"myfile.xml\">\n", program);
         return 1;
      }

      program = argv[0];
      
      netaddress = argv[optind];
      
      //netaddress = "tcp/scul030.acc.gsi.de";
      
      printf("bufstartPtr: %p, %s \n", &bufWrite[0], filename);
      pPage = parseXml(filename);
      showFtmPage(pPage);
      ebRamOpen(netaddress, 1);
      
      
	   
	   pBufWrite = serPage (pPage, &bufWrite[0], FTM_SHARED_OFFSET);
	   printf("pBuf: %p\n", &bufWrite[0]);
	   printf("TestX\n");
	   printf("Offs:\t %08x\n", embeddedOffset);
      printf("\n\n");
	   ebRamWrite(&bufWrite[0], embeddedOffset, BUF_SIZE);
      printf("TestY\n");
	   ebRamRead(embeddedOffset, BUF_SIZE, pBufRead);
      printf("TestZ\n");
	   ebRamClose();

	   pNewPage = deserPage(calloc(1, sizeof(t_ftmPage)), &bufRead[0], FTM_SHARED_OFFSET);
      
      if(pNewPage != NULL) showFtmPage(pNewPage);
      else printf("deserialize failed\n");
	   
	   
   return 0;
}

