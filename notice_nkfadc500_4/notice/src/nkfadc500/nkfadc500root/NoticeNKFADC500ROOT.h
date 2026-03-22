#ifndef NKNKFADC500_ROOT_H
#define NKNKFADC500_ROOT_H

#include "TObject.h"

struct libusb_context;

class NKNKFADC500 : public TObject {
public:
	
	NKNKFADC500();
	virtual ~NKNKFADC500();
	int NKFADC500open(int sid, libusb_context *ctx);
	void NKFADC500close(int sid);
	void NKFADC500reset(int sid);
	void NKFADC500resetTIMER(int sid);
	void NKFADC500start(int sid);
	void NKFADC500stop(int sid);
	unsigned long NKFADC500read_RUN(int sid);
	void NKFADC500write_CW(int sid, unsigned long ch, unsigned long data);
	unsigned long NKFADC500read_CW(int sid, unsigned long ch);
	void NKFADC500write_RL(int sid, unsigned long data);
	unsigned long NKFADC500read_RL(int sid);
	void NKFADC500write_DRAMON(int sid, unsigned long data);
	unsigned long NKFADC500read_DRAMON(int sid);
	void NKFADC500write_DACOFF(int sid, unsigned long ch, unsigned long data);
	unsigned long NKFADC500read_DACOFF(int sid, unsigned long ch);
	void NKFADC500measure_PED(int sid, unsigned long ch);
	unsigned long NKFADC500read_PED(int sid, unsigned long ch);
	void NKFADC500write_DLY(int sid, unsigned long ch, unsigned long data);
	unsigned long NKFADC500read_DLY(int sid, unsigned long ch);
	void NKFADC500write_THR(int sid, unsigned long ch, unsigned long data);
	unsigned long NKFADC500read_THR(int sid, unsigned long ch);
	void NKFADC500write_POL(int sid, unsigned long ch, unsigned long data);
	unsigned long NKFADC500read_POL(int sid, unsigned long ch);
	void NKFADC500write_PSW(int sid, unsigned long ch, unsigned long data);
	unsigned long NKFADC500read_PSW(int sid, unsigned long ch);
	void NKFADC500write_AMODE(int sid, unsigned long ch, unsigned long data);
	unsigned long NKFADC500read_AMODE(int sid, unsigned long ch);
	void NKFADC500write_PCT(int sid, unsigned long ch, unsigned long data);
	unsigned long NKFADC500read_PCT(int sid, unsigned long ch);
	void NKFADC500write_PCI(int sid, unsigned long ch, unsigned long data);
	unsigned long NKFADC500read_PCI(int sid, unsigned long ch);
	void NKFADC500write_PWT(int sid, unsigned long ch, unsigned long data);
	unsigned long NKFADC500read_PWT(int sid, unsigned long ch);
	void NKFADC500write_DT(int sid, unsigned long ch, unsigned long data);
	unsigned long NKFADC500read_DT(int sid, unsigned long ch);
	unsigned long NKFADC500read_BCOUNT(int sid);
	void NKFADC500write_PTRIG(int sid, unsigned long data);
	unsigned long NKFADC500read_PTRIG(int sid);
	void NKFADC500send_TRIG(int sid);
        void NKFADC500write_TRIGENABLE(int sid, unsigned long data);
        unsigned long NKFADC500read_TRIGENABLE(int sid);
	void NKFADC500write_TM(int sid, unsigned long ch, unsigned long data);
	unsigned long NKFADC500read_TM(int sid, unsigned long ch);
	void NKFADC500write_TLT(int sid, unsigned long data);
	unsigned long NKFADC500read_TLT(int sid);
	void NKFADC500write_ZEROSUP(int sid, unsigned long ch, unsigned long data);
	unsigned long NKFADC500read_ZEROSUP(int sid, unsigned long ch);
	void NKFADC500send_ADCRST(int sid);
	void NKFADC500send_ADCCAL(int sid);
	void NKFADC500write_ADCDLY(int sid, unsigned long ch, unsigned long data);
	void NKFADC500write_ADCALIGN(int sid, unsigned long data);
	unsigned long NKFADC500read_ADCSTAT(int sid);
        void NKFADC500write_PSCALE(int sid, unsigned long data);
        unsigned long NKFADC500read_PSCALE(int sid);
        void NKFADC500write_DSR(int sid, unsigned long data);
        unsigned long NKFADC500read_DSR(int sid);
	void NKFADC500read_DATA(int sid, int bcount, char *data);
	void NKFADC500_ADCALIGN_500(int sid);
        void NKFADC500_ADCALIGN_DRAM(int sid);
        void NKFADC500flush_DATA(int sid);
	unsigned long NKFADC500read_EVENT_NUMBER(int sid, unsigned long ch);
	void NKFADC500write_ACQUISITION_TIME(int sid, unsigned long long data);
	unsigned long long NKFADC500read_LIVETIME(int sid);

	ClassDef(NKNKFADC500, 0) // NKNKFADC500 wrapper class for root
};

#endif
