#include "NoticeNKFADC500ROOT.h"
#include "NoticeNKFADC500.h"

ClassImp(NKNKFADC500)

NKNKFADC500::NKNKFADC500()
{
}

NKNKFADC500::~NKNKFADC500()
{
}

int NKNKFADC500::NKFADC500open(int sid, libusb_context *ctx)
{
	return ::NKFADC500open(sid, ctx);
}

void NKNKFADC500::NKFADC500close(int sid)
{
	::NKFADC500close(sid);
}

void NKNKFADC500::NKFADC500reset(int sid)
{
	::NKFADC500reset(sid);
}

void NKNKFADC500::NKFADC500resetTIMER(int sid)
{
	::NKFADC500resetTIMER(sid);
}

void NKNKFADC500::NKFADC500start(int sid)
{
	::NKFADC500start(sid);
}

void NKNKFADC500::NKFADC500stop(int sid)
{
	::NKFADC500stop(sid);
}

unsigned long NKNKFADC500::NKFADC500read_RUN(int sid)
{
	return ::NKFADC500read_RUN(sid);
}

void NKNKFADC500::NKFADC500write_CW(int sid, unsigned long ch, unsigned long data)
{
	::NKFADC500write_CW(sid, ch, data);
}

unsigned long NKNKFADC500::NKFADC500read_CW(int sid, unsigned long ch)
{
	return ::NKFADC500read_CW(sid, ch);
}

void NKNKFADC500::NKFADC500write_RL(int sid, unsigned long data)
{
	::NKFADC500write_RL(sid, data);
}

unsigned long NKNKFADC500::NKFADC500read_RL(int sid)
{
	return ::NKFADC500read_RL(sid);
}

void NKNKFADC500::NKFADC500write_DRAMON(int sid, unsigned long data)
{
	::NKFADC500write_DRAMON(sid, data);
}

unsigned long NKNKFADC500::NKFADC500read_DRAMON(int sid)
{
	return ::NKFADC500read_DRAMON(sid);
}

void NKNKFADC500::NKFADC500write_DACOFF(int sid, unsigned long ch, unsigned long data)
{
	::NKFADC500write_DACOFF(sid, ch, data);
}

unsigned long NKNKFADC500::NKFADC500read_DACOFF(int sid, unsigned long ch)
{
	return ::NKFADC500read_DACOFF(sid, ch);
}

void NKNKFADC500::NKFADC500measure_PED(int sid, unsigned long ch)
{
	::NKFADC500measure_PED(sid, ch);
}

unsigned long NKNKFADC500::NKFADC500read_PED(int sid, unsigned long ch)
{
	return ::NKFADC500read_PED(sid, ch);
}

void NKNKFADC500::NKFADC500write_DLY(int sid, unsigned long ch, unsigned long data)
{
	::NKFADC500write_DLY(sid, ch, data);
}

unsigned long NKNKFADC500::NKFADC500read_DLY(int sid, unsigned long ch)
{
	return ::NKFADC500read_DLY(sid, ch);
}

void NKNKFADC500::NKFADC500write_THR(int sid, unsigned long ch, unsigned long data)
{
	::NKFADC500write_THR(sid, ch, data);
}

unsigned long NKNKFADC500::NKFADC500read_THR(int sid, unsigned long ch)
{
	return ::NKFADC500read_THR(sid, ch);
}

void NKNKFADC500::NKFADC500write_POL(int sid, unsigned long ch, unsigned long data)
{
	::NKFADC500write_POL(sid, ch, data);
}

unsigned long NKNKFADC500::NKFADC500read_POL(int sid, unsigned long ch)
{
	return ::NKFADC500read_POL(sid, ch);
}

void NKNKFADC500::NKFADC500write_PSW(int sid, unsigned long ch, unsigned long data)
{
	::NKFADC500write_PSW(sid, ch, data);
}

unsigned long NKNKFADC500::NKFADC500read_PSW(int sid, unsigned long ch)
{
	return ::NKFADC500read_PSW(sid, ch);
}

void NKNKFADC500::NKFADC500write_AMODE(int sid, unsigned long ch, unsigned long data)
{
	::NKFADC500write_AMODE(sid, ch, data);
}

unsigned long NKNKFADC500::NKFADC500read_AMODE(int sid, unsigned long ch)
{
	return ::NKFADC500read_AMODE(sid, ch);
}

void NKNKFADC500::NKFADC500write_PCT(int sid, unsigned long ch, unsigned long data)
{
	::NKFADC500write_PCT(sid, ch, data);
}

unsigned long NKNKFADC500::NKFADC500read_PCT(int sid, unsigned long ch)
{
	return ::NKFADC500read_PCT(sid, ch);
}

void NKNKFADC500::NKFADC500write_PCI(int sid, unsigned long ch, unsigned long data)
{
	::NKFADC500write_PCI(sid, ch, data);
}

unsigned long NKNKFADC500::NKFADC500read_PCI(int sid, unsigned long ch)
{
	return ::NKFADC500read_PCI(sid, ch);
}

void NKNKFADC500::NKFADC500write_PWT(int sid, unsigned long ch, unsigned long data)
{
	::NKFADC500write_PWT(sid, ch, data);
}

unsigned long NKNKFADC500::NKFADC500read_PWT(int sid, unsigned long ch)
{
	return ::NKFADC500read_PWT(sid, ch);
}

void NKNKFADC500::NKFADC500write_DT(int sid, unsigned long ch, unsigned long data)
{
	::NKFADC500write_DT(sid, ch, data);
}

unsigned long NKNKFADC500::NKFADC500read_DT(int sid, unsigned long ch)
{
	return ::NKFADC500read_DT(sid, ch);
}

unsigned long NKNKFADC500::NKFADC500read_BCOUNT(int sid)
{
	return ::NKFADC500read_BCOUNT(sid);
}

void NKNKFADC500::NKFADC500write_PTRIG(int sid, unsigned long data)
{
	::NKFADC500write_PTRIG(sid, data);
}

unsigned long NKNKFADC500::NKFADC500read_PTRIG(int sid)
{
	return ::NKFADC500read_PTRIG(sid);
}

void NKNKFADC500::NKFADC500send_TRIG(int sid)
{
	::NKFADC500send_TRIG(sid);
}

void NKNKFADC500::NKFADC500write_TRIGENABLE(int sid, unsigned long data)
{
	::NKFADC500write_TRIGENABLE(sid, data);
}

unsigned long NKNKFADC500::NKFADC500read_TRIGENABLE(int sid)
{
	return ::NKFADC500read_TRIGENABLE(sid);
}

void NKNKFADC500::NKFADC500write_TM(int sid, unsigned long ch, unsigned long data)
{
	::NKFADC500write_TM(sid, ch, data);
}

unsigned long NKNKFADC500::NKFADC500read_TM(int sid, unsigned long ch)
{
	return ::NKFADC500read_TM(sid, ch);
}

void NKNKFADC500::NKFADC500write_TLT(int sid, unsigned long data)
{
	::NKFADC500write_TLT(sid, data);
}

unsigned long NKNKFADC500::NKFADC500read_TLT(int sid)
{
	return ::NKFADC500read_TLT(sid);
}

void NKNKFADC500::NKFADC500write_ZEROSUP(int sid, unsigned long ch, unsigned long data)
{
	::NKFADC500write_ZEROSUP(sid, ch, data);
}

unsigned long NKNKFADC500::NKFADC500read_ZEROSUP(int sid, unsigned long ch)
{
	return ::NKFADC500read_ZEROSUP(sid, ch);
}

void NKNKFADC500::NKFADC500send_ADCRST(int sid)
{
	::NKFADC500send_ADCRST(sid);
}

void NKNKFADC500::NKFADC500send_ADCCAL(int sid)
{
	::NKFADC500send_ADCCAL(sid);
}

void NKNKFADC500::NKFADC500write_ADCDLY(int sid, unsigned long ch, unsigned long data)
{
	::NKFADC500write_ADCDLY(sid, ch, data);
}

void NKNKFADC500::NKFADC500write_ADCALIGN(int sid, unsigned long data)
{
	::NKFADC500write_ADCALIGN(sid, data);
}

unsigned long NKNKFADC500::NKFADC500read_ADCSTAT(int sid)
{
	return ::NKFADC500read_ADCSTAT(sid);
}

void NKNKFADC500::NKFADC500write_PSCALE(int sid, unsigned long data)
{
	::NKFADC500write_PSCALE(sid, data);
}

unsigned long NKNKFADC500::NKFADC500read_PSCALE(int sid)
{
	return ::NKFADC500read_PSCALE(sid);
}

void NKNKFADC500::NKFADC500write_DSR(int sid, unsigned long data)
{
	::NKFADC500write_DSR(sid, data);
}

unsigned long NKNKFADC500::NKFADC500read_DSR(int sid)
{
	return ::NKFADC500read_DSR(sid);
}

void NKNKFADC500::NKFADC500read_DATA(int sid, int bcount, char *data)
{
	::NKFADC500read_DATA(sid, bcount, data);
}

void NKNKFADC500::NKFADC500_ADCALIGN_500(int sid)
{
	::NKFADC500_ADCALIGN_500(sid);
}

void NKNKFADC500::NKFADC500_ADCALIGN_DRAM(int sid)
{
	::NKFADC500_ADCALIGN_DRAM(sid);
}

void NKNKFADC500::NKFADC500flush_DATA(int sid)
{
	::NKFADC500flush_DATA(sid);
}

unsigned long NKNKFADC500::NKFADC500read_EVENT_NUMBER(int sid, unsigned long ch)
{
	return ::NKFADC500read_EVENT_NUMBER(sid, ch);
}

void NKNKFADC500::NKFADC500write_ACQUISITION_TIME(int sid, unsigned long long data)
{
	::NKFADC500write_ACQUISITION_TIME(sid, data);
}

unsigned long long NKNKFADC500::NKFADC500read_LIVETIME(int sid)
{
	return ::NKFADC500read_LIVETIME(sid);
}


