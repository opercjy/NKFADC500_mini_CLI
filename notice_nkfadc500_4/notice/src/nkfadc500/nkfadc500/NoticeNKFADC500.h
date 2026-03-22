
#ifndef NKFADC500_H
#define NKFADC500_H

#include <libusb.h>
#ifdef __cplusplus
extern "C" {
#endif

#define NKFADC500_VENDOR_ID (0x0547)
#define NKFADC500_PRODUCT_ID (0x1502)

extern int NKFADC500open(int sid, libusb_context *ctx);
extern void NKFADC500close(int sid);
extern void NKFADC500reset(int sid);
extern void NKFADC500resetTIMER(int sid);
extern void NKFADC500start(int sid);
extern void NKFADC500stop(int sid);
extern unsigned long NKFADC500read_RUN(int sid);
extern void NKFADC500write_CW(int sid, unsigned long ch, unsigned long data);
extern unsigned long NKFADC500read_CW(int sid, unsigned long ch);
extern void NKFADC500write_RL(int sid, unsigned long data);
extern unsigned long NKFADC500read_RL(int sid);
extern void NKFADC500write_DRAMON(int sid, unsigned long data);
extern unsigned long NKFADC500read_DRAMON(int sid);
extern void NKFADC500write_DACOFF(int sid, unsigned long ch, unsigned long data);
extern unsigned long NKFADC500read_DACOFF(int sid, unsigned long ch);
extern void NKFADC500measure_PED(int sid, unsigned long ch);
extern unsigned long NKFADC500read_PED(int sid, unsigned long ch);
extern void NKFADC500write_DLY(int sid, unsigned long ch, unsigned long data);
extern unsigned long NKFADC500read_DLY(int sid, unsigned long ch);
extern void NKFADC500write_THR(int sid, unsigned long ch, unsigned long data);
extern unsigned long NKFADC500read_THR(int sid, unsigned long ch);
extern void NKFADC500write_POL(int sid, unsigned long ch, unsigned long data);
extern unsigned long NKFADC500read_POL(int sid, unsigned long ch);
extern void NKFADC500write_PSW(int sid, unsigned long ch, unsigned long data);
extern unsigned long NKFADC500read_PSW(int sid, unsigned long ch);
extern void NKFADC500write_AMODE(int sid, unsigned long ch, unsigned long data);
extern unsigned long NKFADC500read_AMODE(int sid, unsigned long ch);
extern void NKFADC500write_PCT(int sid, unsigned long ch, unsigned long data);
extern unsigned long NKFADC500read_PCT(int sid, unsigned long ch);
extern void NKFADC500write_PCI(int sid, unsigned long ch, unsigned long data);
extern unsigned long NKFADC500read_PCI(int sid, unsigned long ch);
extern void NKFADC500write_PWT(int sid, unsigned long ch, unsigned long data);
extern unsigned long NKFADC500read_PWT(int sid, unsigned long ch);
extern void NKFADC500write_DT(int sid, unsigned long ch, unsigned long data);
extern unsigned long NKFADC500read_DT(int sid, unsigned long ch);
extern unsigned long NKFADC500read_BCOUNT(int sid);
extern void NKFADC500write_PTRIG(int sid, unsigned long data);
extern unsigned long NKFADC500read_PTRIG(int sid);
extern void NKFADC500send_TRIG(int sid);
extern void NKFADC500write_TRIGENABLE(int sid, unsigned long data);
extern unsigned long NKFADC500read_TRIGENABLE(int sid);
extern void NKFADC500write_TM(int sid, unsigned long ch, unsigned long data);
extern unsigned long NKFADC500read_TM(int sid, unsigned long ch);
extern void NKFADC500write_TLT(int sid, unsigned long data);
extern unsigned long NKFADC500read_TLT(int sid);
extern void NKFADC500write_ZEROSUP(int sid, unsigned long ch, unsigned long data);
extern unsigned long NKFADC500read_ZEROSUP(int sid, unsigned long ch);
extern void NKFADC500send_ADCRST(int sid);
extern void NKFADC500send_ADCCAL(int sid);
extern void NKFADC500write_ADCDLY(int sid, unsigned long ch, unsigned long data);
extern void NKFADC500write_ADCALIGN(int sid, unsigned long data);
extern unsigned long NKFADC500read_ADCSTAT(int sid);
extern void NKFADC500write_DRAMDLY(int sid, unsigned long ch, unsigned long data);
extern void NKFADC500write_BITSLIP(int sid, unsigned long ch);
extern void NKFADC500write_DRAMTEST(int sid, unsigned long data);
extern unsigned long NKFADC500read_DRAMTEST(int sid, unsigned long ch);
extern void NKFADC500write_PSCALE(int sid, unsigned long data);
extern unsigned long NKFADC500read_PSCALE(int sid);
extern void NKFADC500write_DSR(int sid, unsigned long data);
extern unsigned long NKFADC500read_DSR(int sid);
extern void NKFADC500read_DATA(int sid, int bcount, char *data);
extern void NKFADC500_ADCALIGN_500(int sid);
extern void NKFADC500_ADCALIGN_DRAM(int sid);
extern void NKFADC500flush_DATA(int sid);
extern unsigned long NKFADC500read_EVENT_NUMBER(int sid, unsigned long ch);
extern void NKFADC500write_ACQUISITION_TIME(int sid, unsigned long long data);
extern unsigned long long NKFADC500read_LIVETIME(int sid);

#ifdef __cplusplus
}
#endif

#endif
