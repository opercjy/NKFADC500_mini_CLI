#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "usb3com.h"
#include "NoticeNKFADC500.h"

// open NKFADC500
int NKFADC500open(int sid, libusb_context *ctx)
{
  int status;

  status = USB3Open(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, ctx);
  USB3ClaimInterface(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0);

//  NKFADC500read_BCOUNT(sid);
//  NKFADC500read_BCOUNT(sid);
//  NKFADC500read_BCOUNT(sid);
//  NKFADC500read_BCOUNT(sid);

  printf("Now NKFADC500 is ready.\n");

  return status;
}


// close NKFADC500
void NKFADC500close(int sid)
{
  USB3ReleaseInterface(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0);
  USB3Close(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid);
}

// reset data acquisition
void NKFADC500reset(int sid)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000000, 1 << 2);
}

// reset timer
void NKFADC500resetTIMER(int sid)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000000, 1);
}

// start data acquisition
void NKFADC500start(int sid)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000000, 1 << 3);
}

// stop data acquisition
void NKFADC500stop(int sid)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000000, 0 << 3);
}

// read RUN status
unsigned long NKFADC500read_RUN(int sid)
{
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000000);
}

// write coincidence window
void NKFADC500write_CW(int sid, unsigned long ch, unsigned long data)
{
  unsigned long addr;

  addr = 0x20000001 + (((ch - 1) & 0xFF) << 16);

  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, data);
}

// read coincidence windows
unsigned long NKFADC500read_CW(int sid, unsigned long ch)
{
  unsigned long addr;

  addr = 0x20000001 + (((ch - 1) & 0xFF) << 16);
  
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr);
}

// write recording length
void NKFADC500write_RL(int sid, unsigned long data)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000002, data);
}

// read recording length
unsigned long NKFADC500read_RL(int sid)
{
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000002);
}

// turn on/off DRAM
// 0 = off, 1 = on
void NKFADC500write_DRAMON(int sid, unsigned long data)
{
  unsigned long status;

  // turn on DRAM
  if (data) {
    // check DRAM is on
    status = USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000003);

    // when DRAM is on now, turn it off
    if (status)
      USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000003, 0);

    // turn on DRAM
    USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000003, 1);

    // wait for DRAM ready
    status = 0;
    while (!status)
      status = USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000003);
  }
  // trun off DRAM
  else 
    USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000003, 0);
}

// read DRAM status
unsigned long NKFADC500read_DRAMON(int sid)
{
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000003);
}

// write offset adjustment
void NKFADC500write_DACOFF(int sid, unsigned long ch, unsigned long data)
{
  unsigned long addr;

  addr = 0x20000004 + (((ch - 1) & 0xFF) << 16);

  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, data);
  sleep(1);
}

// read offset adjustment
unsigned long NKFADC500read_DACOFF(int sid, unsigned long ch)
{
  unsigned long addr;

  addr = 0x20000004 + (((ch - 1) & 0xFF) << 16);
  
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr);
}

// measure pedestal
void NKFADC500measure_PED(int sid, unsigned long ch)
{
  unsigned long addr;

  addr = 0x20000005 + (((ch - 1) & 0xFF) << 16);

  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, 0);
}

// read pedestal
unsigned long NKFADC500read_PED(int sid, unsigned long ch)
{
  unsigned long addr;

  addr = 0x20000006 + (((ch - 1) & 0xFF) << 16);
  
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr);
}

// write input delay
void NKFADC500write_DLY(int sid, unsigned long ch, unsigned long data)
{
  unsigned long addr;
  unsigned long value;

  addr = 0x20000007 + (((ch - 1) & 0xFF) << 16);
  value = ((data / 1000) << 10) | (data % 1000);

  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, value);
}

// read input delay
unsigned long NKFADC500read_DLY(int sid, unsigned long ch)
{
  unsigned long addr;
  unsigned long value;
  unsigned long data;

  addr = 0x20000007 + (((ch - 1) & 0xFF) << 16);
  value = USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr);
  data = (value >> 10) * 1000 + (value & 0x3FF);
  
  return data;
}

// write discriminator threshold
void NKFADC500write_THR(int sid, unsigned long ch, unsigned long data)
{
  unsigned long addr;

  addr = 0x20000008 + (((ch - 1) & 0xFF) << 16);

  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, data);
}

// read discriminator threshold
unsigned long NKFADC500read_THR(int sid, unsigned long ch)
{
  unsigned long addr;

  addr = 0x20000008 + (((ch - 1) & 0xFF) << 16);
  
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr);
}

// write input pulse polarity
void NKFADC500write_POL(int sid, unsigned long ch, unsigned long data)
{
  unsigned long addr;

  addr = 0x20000009 + (((ch - 1) & 0xFF) << 16);

  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, data);
}

// read input pulse polarity
unsigned long NKFADC500read_POL(int sid, unsigned long ch)
{
  unsigned long addr;

  addr = 0x20000009 + (((ch - 1) & 0xFF) << 16);
  
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr);
}

// write pulse sum trigger width
void NKFADC500write_PSW(int sid, unsigned long ch, unsigned long data)
{
  unsigned long addr;

  addr = 0x2000000A + (((ch - 1) & 0xFF) << 16);

  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, data);
}

// read pulse sum trigger width
unsigned long NKFADC500read_PSW(int sid, unsigned long ch)
{
  unsigned long addr;

  addr = 0x2000000A + (((ch - 1) & 0xFF) << 16);
  
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr);
}

// write ADC mode
void NKFADC500write_AMODE(int sid, unsigned long ch, unsigned long data)
{
  unsigned long addr;

  addr = 0x2000000B + (((ch - 1) & 0xFF) << 16);

  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, data);
}

// read ADC mode
unsigned long NKFADC500read_AMODE(int sid, unsigned long ch)
{
  unsigned long addr;

  addr = 0x2000000B + (((ch - 1) & 0xFF) << 16);
  
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr);
}

// write pulse count threshold
void NKFADC500write_PCT(int sid, unsigned long ch, unsigned long data)
{
  unsigned long addr;

  addr = 0x2000000C + (((ch - 1) & 0xFF) << 16);

  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, data);
}

// read pulse count threshold
unsigned long NKFADC500read_PCT(int sid, unsigned long ch)
{
  unsigned long addr;

 addr = 0x2000000C + (((ch - 1) & 0xFF) << 16);
  
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr);
}

// write pulse count interval
void NKFADC500write_PCI(int sid, unsigned long ch, unsigned long data)
{
  unsigned long addr;

  addr = 0x2000000D + (((ch - 1) & 0xFF) << 16);

  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, data);
}

// write pulse count interval
unsigned long NKFADC500read_PCI(int sid, unsigned long ch)
{
  unsigned long addr;

 addr = 0x2000000D + (((ch - 1) & 0xFF) << 16);
  
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr);
}

// write pulse width threshold
void NKFADC500write_PWT(int sid, unsigned long ch, unsigned long data)
{
  unsigned long addr;

  addr = 0x2000000E + (((ch - 1) & 0xFF) << 16);

  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, data);
}

// read pulse width threshold
unsigned long NKFADC500read_PWT(int sid, unsigned long ch)
{
  unsigned long addr;

  addr = 0x2000000E + (((ch - 1) & 0xFF) << 16);
  
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr);
}

// write deadtime
void NKFADC500write_DT(int sid, unsigned long ch, unsigned long data)
{
  unsigned long addr;

  addr = 0x2000000F + (((ch - 1) & 0xFF) << 16);

  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, data);
}

// read deadtime
unsigned long NKFADC500read_DT(int sid, unsigned long ch)
{
  unsigned long addr;

  addr = 0x2000000F + (((ch - 1) & 0xFF) << 16);
  
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr);
}

// Read data buffer count, 1 buffer count = 1 kbyte data
unsigned long NKFADC500read_BCOUNT(int sid)
{
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000010);
}

// write pedestal trigger interval in ms;
void NKFADC500write_PTRIG(int sid, unsigned long data)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000011, data);
}

// read pedestal trigger interval in ms;
unsigned long NKFADC500read_PTRIG(int sid)
{
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000011);
}

// send trigger
void NKFADC500send_TRIG(int sid)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000012, 0);
}

// write trigger enable
void NKFADC500write_TRIGENABLE(int sid, unsigned long data)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000013, data);
}

// read trigger enable
extern unsigned long NKFADC500read_TRIGENABLE(int sid)
{
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000013);
}

// write trigger mode
void NKFADC500write_TM(int sid, unsigned long ch, unsigned long data)
{
  unsigned long addr;

  addr = 0x20000014 + (((ch - 1) & 0xFF) << 16);

  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, data);
}

// read trigger mode
unsigned long NKFADC500read_TM(int sid, unsigned long ch)
{
  unsigned long addr;

  addr = 0x20000014 + (((ch - 1) & 0xFF) << 16);
  
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr);
}

// write trigger lookup table
void NKFADC500write_TLT(int sid, unsigned long data)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000015, data);
}

// write trigger lookup table
unsigned long NKFADC500read_TLT(int sid)
{
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000015);
}

// write zero suppression
void NKFADC500write_ZEROSUP(int sid, unsigned long ch, unsigned long data)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000016, data);
}

// read zero suppression
unsigned long NKFADC500read_ZEROSUP(int sid, unsigned long ch)
{
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000016);
}

// send ADC reset signal
void NKFADC500send_ADCRST(int sid)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000017, 0);
}

// send ADC calibration signal
void NKFADC500send_ADCCAL(int sid)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x20000018, 0);
}

// write ADC calibration delay
void NKFADC500write_ADCDLY(int sid, unsigned long ch, unsigned long data)
{
  unsigned long addr;

  addr = 0x20000019 + (((ch - 1) & 0xFF) << 16);

  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, data);
}

// write ADC align delay
void NKFADC500write_ADCALIGN(int sid, unsigned long data)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x2000001A, data);
}

// read ADC status
unsigned long NKFADC500read_ADCSTAT(int sid)
{
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x2000001A);
}

// write DRAM calibration delay
void NKFADC500write_DRAMDLY(int sid, unsigned long ch, unsigned long data)
{
  unsigned long addr;

  addr = 0x2000001B + ((ch & 0xFF) << 16);
  
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, data);
}

// write DRAM bitslip
void NKFADC500write_BITSLIP(int sid, unsigned long ch)
{
  unsigned long addr;

  addr = 0x2000001C + ((ch & 0xFF) << 16);

  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr, 0);
}

// write DRAM test 
void NKFADC500write_DRAMTEST(int sid, unsigned long data)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x2000001D, data);
}

// read DRAM alignment
extern unsigned long NKFADC500read_DRAMTEST(int sid, unsigned long ch)
{
  unsigned long addr;

  addr = 0x2000001D + ((ch & 0xFF) << 16);
  
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr);
}

// write self trigger prescale
void NKFADC500write_PSCALE(int sid, unsigned long data)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x2000001E, data);
}

// read self trigger prescale
extern unsigned long NKFADC500read_PSCALE(int sid)
{
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x2000001E);
}

// write sampling rate /1/2/4/8;
void NKFADC500write_DSR(int sid, unsigned long data)
{
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x2000001F, data);
}

// read sampling rate
unsigned long NKFADC500read_DSR(int sid)
{
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x2000001F);
}

// read data, reads bcount * 1 kbytes data from NKFADC500 DRAM
// returns character raw data, needs sorting after data acquisition
void NKFADC500read_DATA(int sid, int bcount, char *data)
{
  int count;

  // maximum data size is 64 Mbyte
  count = bcount * 256;

  USB3Read(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, count, 0x40000000, data);  
}

// align ADC for NKFADC500
void NKFADC500_ADCALIGN_500(int sid)
{
  unsigned long ch, dly, value;
  int count, sum, center;
  unsigned long gdly;
  int flag;

  NKFADC500send_ADCRST(sid); 
  usleep(500000);
  NKFADC500send_ADCCAL(sid); 
  NKFADC500write_ADCALIGN(sid, 1);

  for (ch = 1; ch <= 4; ch++) {
    count = 0;
    sum = 0;
    flag = 0;

    for (dly = 0; dly < 32; dly++) {
      NKFADC500write_ADCDLY(sid, ch, dly); 
      value = (NKFADC500read_ADCSTAT(sid) >> (ch -1)) & 0x1; 
      
      // count bad delay
      if (!value) {
        flag = 1;
        count = count + 1;
        sum = sum + dly;
      }
      else {
        if (flag) 
          dly = 32;
      }
    }

    // get bad delay center
    center = sum / count;

    // set good delay
    if (center < 11)
      gdly = center + 11;
    else
      gdly = center - 11;

    // set delay
    NKFADC500write_ADCDLY(sid, ch, gdly); 
    printf("ch%ld calibration delay = %ld\n", ch, gdly);
  }

  NKFADC500write_ADCALIGN(sid, 0); 
  NKFADC500send_ADCCAL(sid); 
}

// align DRAM
void NKFADC500_ADCALIGN_DRAM(int sid)
{
  unsigned long ch;
  unsigned long dly;
  unsigned long value;
  int flag;
  int count;
  int sum;
  int aflag;
  unsigned long gdly;
  int bitslip;
  int gbitslip;

  // turn on DRAM    
  NKFADC500write_DRAMON(sid, 1);

  // enter DRAM test mode
  NKFADC500write_DRAMTEST(sid, 1);

  // send reset to iodelay  
  NKFADC500send_ADCCAL(sid);

  // fill DRAM test pattern
  NKFADC500write_DRAMTEST(sid, 2);

  for (ch = 0; ch < 8; ch ++) {
    count = 0;
    sum = 0;
    flag = 0;

    // search delay
    for (dly = 0; dly < 32; dly++) {
      // set delay
      NKFADC500write_DRAMDLY(sid, ch, dly);

      // read DRAM test pattern
      NKFADC500write_DRAMTEST(sid, 3);
      value = NKFADC500read_DRAMTEST(sid, ch);

      aflag = 0;
      if (value == 0xFFAA5500)
        aflag = 1;
      else if (value == 0xAA5500FF)
        aflag = 1;
      else if (value == 0x5500FFAA)
        aflag = 1;
      else if (value == 0x00FFAA55)
        aflag = 1;
    
      if (aflag) {
        count = count + 1;
        sum = sum + dly;
        if (count > 4)
          flag = 1; 
     }
      else {
        if (flag)
          dly = 32;
        else {
          count = 0;
          sum = 0;
        }
      }
    }

    // get bad delay center
    if (count)
      gdly = sum / count;
    else
      gdly = 9;

    // set delay
    NKFADC500write_DRAMDLY(sid, ch, gdly); 
  
    // get bitslip
    for (bitslip = 0; bitslip < 4; bitslip++) {
      // read DRAM test pattern
      NKFADC500write_DRAMTEST(sid, 3);
      value = NKFADC500read_DRAMTEST(sid, ch);

      if (value == 0xFFAA5500) {
        aflag = 1;
        gbitslip = bitslip;
        bitslip = 4;
      }
      else {
        aflag = 0;
        NKFADC500write_BITSLIP(sid, ch);
      }
    }

    if (aflag) 
      printf("DRAM(%ld) is aligned, delay = %ld, bitslip = %d\n", ch, gdly, gbitslip);
    else 
      printf("Fail to align DRAM(%ld)!\n", ch);
  }

  // exit DRAM test mode
  NKFADC500write_DRAMTEST(sid, 0);
}

// flush remaining data
void NKFADC500flush_DATA(int sid)
{
  // max buffer size is 10 MB
  char data[10485760];
  unsigned long bcount;
  unsigned long chunk;
  unsigned long slice;
  int i;
  
  // check data size to be flushed
  bcount = NKFADC500read_BCOUNT(sid);
  chunk = bcount / 10240;
  slice = bcount % 10240;
  
  for (i = 0; i < chunk; i++)
    NKFADC500read_DATA(sid, 10240, data);

  if (slice)
    NKFADC500read_DATA(sid, slice, data);
}

// read event number
unsigned long NKFADC500read_EVENT_NUMBER(int sid, unsigned long ch)
{
  unsigned long addr;

  addr = 0x2000001C + (((ch - 1) & 0xFF) << 16);
  
  return USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, addr);
}

// write acquisition time
void NKFADC500write_ACQUISITION_TIME(int sid, unsigned long long data)
{
  unsigned long msb;
  unsigned long lsb;
  unsigned long long tmp;

  lsb = data & 0xFFFFFFFF;
  tmp = (data >> 32) & 0xFFFFFFFF;
  msb = tmp & 0xFFFFFFFF;
  
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x2000001E, lsb);
  USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x2000001F, msb);
}

// read DAQ livetime
unsigned long long NKFADC500read_LIVETIME(int sid)
{
  unsigned long long data;
  unsigned long lsb;
  unsigned long msb;

  lsb = USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x2000001E);
  msb = USB3ReadReg(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, sid, 0x2000001F);

  data = msb;
  data = data << 32;
  data = data + lsb;

  return data;
}

