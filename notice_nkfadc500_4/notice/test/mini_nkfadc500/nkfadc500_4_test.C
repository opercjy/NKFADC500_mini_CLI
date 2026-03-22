// Test program of kfadc500 
// Usgage :  ROOT> .L nkfadc500s_test.C
//           ROOT> run_nkfadc500(1000) //for 1000 events data case

#include <unistd.h>
#include <stdio.h>

#define PC_DRAM_SIZE      10                           // available PC DRAM size in Mbyte
#define DATA_ARRAY_SIZE   (PC_DRAM_SIZE*1024*1024)       // array size in byte
#define CHUNK_SIZE        (PC_DRAM_SIZE*1024)            // array size in kilobyte


int run_nkfadc500(int Nevent = 100000) {

  // setting variables
  // common setting variables for NKFADC500
  int sid = 1;  	                // NKFADC500 SID
  unsigned long rl = 8;	        	// recording length: 1=128ns, 2=256ns, 4=512ns, 
                                       	// 8=1us, 16=2us, 32=4us, 64=8us, 128=16us, 256=32us
  unsigned long tlt = 0xFFFE;	       	// trigger lookup table value
  unsigned long ptrig_interval = 0;    	// pedestal trigger interval in ms, 0 for disable
  unsigned long pscale = 1;		// prescale = 1 ~ 65535
  // individual channel setting variables
  // in this example, we'll put the same setting for every channel
  // every timing setting unit is ns
  unsigned long sr = 1;                 // sampling rate /1/2/4/8
  unsigned long cw = 1000;	 	// coincidence width (8 ~ 32760)
  unsigned long offset = 3500;          // ADC offset value (0~4095)
  unsigned long dly = 200;         	// ADC waveform delay from trigger point (0 ~ 31992)
  unsigned long thr = 50;		// discrimination threshold 
                                        // 1 ~ 4095 for pulse height threshold
                                        // 1 ~ 67108863 for peak sum threshold
  unsigned long pol = 0;		// input pulse polarity, 0 = negative, 1 = positive
  unsigned long psw = 2;		// peak sum width in ns (2 ~ 16382 ns)
  unsigned long amode = 1;		// ADC mode, 0 = raw, 1 = filtered
  unsigned long pct = 1;		// pulse count threshold (1 ~ 15)
  unsigned long pci = 1000;		// pulse count interval (32 ~ 8160 ns)
  unsigned long pwt = 100;		// pulse width threshold (2 ~ 1022 ns)
  unsigned long dt = 0;			// trigger deadtime (0 ~ 8355840 ns)
  unsigned long ten = 1; 		 // when 1 enables pulse count trigger 
  unsigned long tew = 0; 		 // when 1 enables pulse width trigger
  unsigned long tep = 0; 	         // when 1 enables peak sum trigger
  unsigned long tep2 = 0;                // when 1 enables peak sum OR trigger
  unsigned long tmode = (tep2<<3) | (tep<<2) | (tew<<1) | ten;
  unsigned long en_strig = 1; 		 // when 1 enables self trigger
  unsigned long en_ptrig = 1; 		 // when 1 enables pedestal trigger
  unsigned long en_rtrig = 1; 	         // when 1 enables software trigger
  unsigned long en_etrig = 1;            // when 1 enables external trigger
  unsigned long trig_enable = (en_etrig<<3) | (en_rtrig<<2) | (en_ptrig<<1) | en_strig;

  // local variables
  char *data;        // raw data array
  int adc;	                     // ADC data
  int data_length;                   // data length
  int run_number;                    // run number
  int trigger_type;                  // trigger type
  int trigger_destination;            // trigger destination
  int trigger_number;                // trigger number from TCB
  unsigned long ttime;    	     // trigger time from TCB in ns
  int mid;                  		 // module id in data packet header
  int channel;                       // channel number of data header
  int local_tnum;                    // local trigger # from NKFADC500
  int trigger_pattern;               // trigger pattern from NKFADC500
  unsigned long ltime;    	     // starting time of waveform
  unsigned int evtn;      	     // event number counter
  unsigned long bcount;   	     // buffer counter

  // temporary variable
  int hist_point;		// number of samples to show
  float hist_range;		// histogram range
  //int daq;			// DAQ flag
  FILE *fp;			// data file
  char filename[256];           // data filename
  int bufcnt;			// size of buffer data to be read
  int flag;
  int chunk, slice;
  int i, j, k;
  unsigned long ltmp;
  unsigned int itmp;
 
  unsigned long trig_timel;
  unsigned long trig_timeh;
  unsigned long start_timel;
  unsigned long start_timeh;

  data = (char *)malloc(sizeof(char) * 4 * DATA_ARRAY_SIZE);

  // define some histograms
  TCanvas *c1 = new TCanvas("c1", "KFADC", 800, 800);
  c1->Divide(2,2);

  TH1F *hoscd1 = 0;
  TH1F *hoscd2 = 0;
  TH1F *hoscd3 = 0;
  TH1F *hoscd4 = 0;

  // Loading usb & nkfadc500 lib.
  R__LOAD_LIBRARY(libusb3comroot.so);		// load usb3 library
  R__LOAD_LIBRARY(libNoticeNKFADC500ROOT.so);// load nkfadc500 library

  // define VME class
  usb3comroot *usb = new usb3comroot;
  usb->USB3Init(0);

  // define KFADC500 class
  NKNKFADC500 *nkfadc = new NKNKFADC500;

  // open KFADC500
  nkfadc->NKFADC500open(sid, 0);

  // set NKFADC500
  nkfadc->NKFADC500write_DSR(sid, sr);
  nkfadc->NKFADC500resetTIMER(sid);  
  nkfadc->NKFADC500reset(sid);  
  nkfadc->NKFADC500_ADCALIGN_500(sid);
  nkfadc->NKFADC500_ADCALIGN_DRAM(sid);
  nkfadc->NKFADC500write_PTRIG(sid, ptrig_interval);
  nkfadc->NKFADC500write_CW(sid, 1, cw); 
  nkfadc->NKFADC500write_CW(sid, 2, cw); 
  nkfadc->NKFADC500write_CW(sid, 3, cw); 
  nkfadc->NKFADC500write_CW(sid, 4, cw); 
  nkfadc->NKFADC500write_RL(sid, rl); 
  nkfadc->NKFADC500write_DRAMON(sid, 1);
  nkfadc->NKFADC500write_DACOFF(sid, 1, offset);
  nkfadc->NKFADC500write_DACOFF(sid, 2, offset);
  nkfadc->NKFADC500write_DACOFF(sid, 3, offset);
  nkfadc->NKFADC500write_DACOFF(sid, 4, offset);
  nkfadc->NKFADC500measure_PED(sid, 1);
  nkfadc->NKFADC500measure_PED(sid, 2);
  nkfadc->NKFADC500measure_PED(sid, 3);
  nkfadc->NKFADC500measure_PED(sid, 4);
  nkfadc->NKFADC500write_DLY(sid, 1, cw + dly);
  nkfadc->NKFADC500write_DLY(sid, 2, cw + dly);
  nkfadc->NKFADC500write_DLY(sid, 3, cw + dly);
  nkfadc->NKFADC500write_DLY(sid, 4, cw + dly);
  nkfadc->NKFADC500write_THR(sid, 1, thr);
  nkfadc->NKFADC500write_THR(sid, 2, thr);
  nkfadc->NKFADC500write_THR(sid, 3, thr);
  nkfadc->NKFADC500write_THR(sid, 4, thr);
  nkfadc->NKFADC500write_POL(sid, 1, pol);
  nkfadc->NKFADC500write_POL(sid, 2, pol);
  nkfadc->NKFADC500write_POL(sid, 3, pol);
  nkfadc->NKFADC500write_POL(sid, 4, pol);
  nkfadc->NKFADC500write_PSW(sid, 1, psw);
  nkfadc->NKFADC500write_PSW(sid, 2, psw);
  nkfadc->NKFADC500write_PSW(sid, 3, psw);
  nkfadc->NKFADC500write_PSW(sid, 4, psw);
  nkfadc->NKFADC500write_AMODE(sid, 1, amode);
  nkfadc->NKFADC500write_AMODE(sid, 2, amode);
  nkfadc->NKFADC500write_AMODE(sid, 3, amode);
  nkfadc->NKFADC500write_AMODE(sid, 4, amode);
  nkfadc->NKFADC500write_PCT(sid, 1, pct);
  nkfadc->NKFADC500write_PCT(sid, 2, pct);
  nkfadc->NKFADC500write_PCT(sid, 3, pct);
  nkfadc->NKFADC500write_PCT(sid, 4, pct);
  nkfadc->NKFADC500write_PCI(sid, 1, pci);
  nkfadc->NKFADC500write_PCI(sid, 2, pci);
  nkfadc->NKFADC500write_PCI(sid, 3, pci);
  nkfadc->NKFADC500write_PCI(sid, 4, pci);
  nkfadc->NKFADC500write_PWT(sid, 1, pwt);
  nkfadc->NKFADC500write_PWT(sid, 2, pwt);
  nkfadc->NKFADC500write_PWT(sid, 3, pwt);
  nkfadc->NKFADC500write_PWT(sid, 4, pwt);
  nkfadc->NKFADC500write_DT(sid, 1, dt);
  nkfadc->NKFADC500write_DT(sid, 2, dt);
  nkfadc->NKFADC500write_DT(sid, 3, dt);
  nkfadc->NKFADC500write_DT(sid, 4, dt);
  nkfadc->NKFADC500write_TM(sid, 1, tmode);
  nkfadc->NKFADC500write_TM(sid, 2, tmode);
  nkfadc->NKFADC500write_TM(sid, 3, tmode);
  nkfadc->NKFADC500write_TM(sid, 4, tmode);
  nkfadc->NKFADC500write_TLT(sid, tlt);
  nkfadc->NKFADC500write_PSCALE(sid, pscale);
  nkfadc->NKFADC500write_TRIGENABLE(sid, trig_enable);

  printf("sampling rate = %ld\n", nkfadc->NKFADC500read_DSR(sid));
  printf("rl = %ld\n", nkfadc->NKFADC500read_RL(sid));
  printf("dramon = %ld\n", nkfadc->NKFADC500read_DRAMON(sid));
  printf("pedestal trigger interval = %ld ms\n", nkfadc->NKFADC500read_PTRIG(sid));
  printf("ch1 cw = %ld\n", nkfadc->NKFADC500read_CW(sid, 1));
  printf("ch2 cw = %ld\n", nkfadc->NKFADC500read_CW(sid, 2));
  printf("ch3 cw = %ld\n", nkfadc->NKFADC500read_CW(sid, 3));
  printf("ch4 cw = %ld\n", nkfadc->NKFADC500read_CW(sid, 4));
  printf("ch1 offset = %ld\n", nkfadc->NKFADC500read_DACOFF(sid, 1));
  printf("ch2 offset = %ld\n", nkfadc->NKFADC500read_DACOFF(sid, 2));
  printf("ch3 offset = %ld\n", nkfadc->NKFADC500read_DACOFF(sid, 3));
  printf("ch4 offset = %ld\n", nkfadc->NKFADC500read_DACOFF(sid, 4));
  printf("ch1 dly = %ld\n", nkfadc->NKFADC500read_DLY(sid, 1));
  printf("ch2 dly = %ld\n", nkfadc->NKFADC500read_DLY(sid, 2));
  printf("ch3 dly = %ld\n", nkfadc->NKFADC500read_DLY(sid, 3));
  printf("ch4 dly = %ld\n", nkfadc->NKFADC500read_DLY(sid, 4));
  printf("ch1 thr = %ld\n", nkfadc->NKFADC500read_THR(sid, 1));
  printf("ch2 thr = %ld\n", nkfadc->NKFADC500read_THR(sid, 2));
  printf("ch3 thr = %ld\n", nkfadc->NKFADC500read_THR(sid, 3));
  printf("ch4 thr = %ld\n", nkfadc->NKFADC500read_THR(sid, 4));
  printf("ch1 pol = %ld\n", nkfadc->NKFADC500read_POL(sid, 1));
  printf("ch2 pol = %ld\n", nkfadc->NKFADC500read_POL(sid, 2));
  printf("ch3 pol = %ld\n", nkfadc->NKFADC500read_POL(sid, 3));
  printf("ch4 pol = %ld\n", nkfadc->NKFADC500read_POL(sid, 4));
  printf("ch1 psw = %ld\n", nkfadc->NKFADC500read_PSW(sid, 1));
  printf("ch2 psw = %ld\n", nkfadc->NKFADC500read_PSW(sid, 2));
  printf("ch3 psw = %ld\n", nkfadc->NKFADC500read_PSW(sid, 3));
  printf("ch4 psw = %ld\n", nkfadc->NKFADC500read_PSW(sid, 4));
  printf("ch1 amode = %ld\n", nkfadc->NKFADC500read_AMODE(sid, 1));
  printf("ch2 amode = %ld\n", nkfadc->NKFADC500read_AMODE(sid, 2));
  printf("ch3 amode = %ld\n", nkfadc->NKFADC500read_AMODE(sid, 3));
  printf("ch4 amode = %ld\n", nkfadc->NKFADC500read_AMODE(sid, 4));
  printf("ch1 pct = %ld\n", nkfadc->NKFADC500read_PCT(sid, 1));
  printf("ch2 pct = %ld\n", nkfadc->NKFADC500read_PCT(sid, 2));
  printf("ch3 pct = %ld\n", nkfadc->NKFADC500read_PCT(sid, 3));
  printf("ch4 pct = %ld\n", nkfadc->NKFADC500read_PCT(sid, 4));
  printf("ch1 pci = %ld\n", nkfadc->NKFADC500read_PCI(sid, 1));
  printf("ch2 pci = %ld\n", nkfadc->NKFADC500read_PCI(sid, 2));
  printf("ch3 pci = %ld\n", nkfadc->NKFADC500read_PCI(sid, 3));
  printf("ch4 pci = %ld\n", nkfadc->NKFADC500read_PCI(sid, 4));
  printf("ch1 pwt = %ld\n", nkfadc->NKFADC500read_PWT(sid, 1));
  printf("ch2 pwt = %ld\n", nkfadc->NKFADC500read_PWT(sid, 2));
  printf("ch3 pwt = %ld\n", nkfadc->NKFADC500read_PWT(sid, 3));
  printf("ch4 pwt = %ld\n", nkfadc->NKFADC500read_PWT(sid, 4));
  printf("ch1 dt = %ld\n", nkfadc->NKFADC500read_DT(sid, 1));
  printf("ch2 dt = %ld\n", nkfadc->NKFADC500read_DT(sid, 2));
  printf("ch3 dt = %ld\n", nkfadc->NKFADC500read_DT(sid, 3));
  printf("ch4 dt = %ld\n", nkfadc->NKFADC500read_DT(sid, 4));
  printf("ch1 tm = %ld\n", nkfadc->NKFADC500read_TM(sid, 1));
  printf("ch2 tm = %ld\n", nkfadc->NKFADC500read_TM(sid, 2));
  printf("ch3 tm = %ld\n", nkfadc->NKFADC500read_TM(sid, 3));
  printf("ch4 tm = %ld\n", nkfadc->NKFADC500read_TM(sid, 4));
  printf("tlt = %lX\n", nkfadc->NKFADC500read_TLT(sid));
  printf("prescale = %ld\n", nkfadc->NKFADC500read_PSCALE(sid));
  printf("trigger enable = %ld\n", nkfadc->NKFADC500read_TRIGENABLE(sid));
  printf("ch1 pedestal = %ld\n", nkfadc->NKFADC500read_PED(sid, 1));
  printf("ch2 pedestal = %ld\n", nkfadc->NKFADC500read_PED(sid, 2));
  printf("ch3 pedestal = %ld\n", nkfadc->NKFADC500read_PED(sid, 3));
  printf("ch4 pedestal = %ld\n", nkfadc->NKFADC500read_PED(sid, 4));

  nkfadc->NKFADC500reset(sid); 
  nkfadc->NKFADC500start(sid);

  // now take data
  // set data filename
  sprintf(filename, "nkfadc500_4.dat");

  // open data file
  fp = fopen(filename, "wb");

  flag = 1;
  evtn = 0;

  while (flag) {

    // check buffer if there are something to read (in kilobyte)
    bcount = nkfadc->NKFADC500read_BCOUNT(sid);

//    printf("bcount = %ld\n", bcount);

    // if there is data, read them
    if (bcount) {

      // if data size is too big, slice them into pieces
      chunk = bcount / CHUNK_SIZE;
      slice = bcount % CHUNK_SIZE;
      slice = slice / 128;
      slice = slice * 128;

      for (i = 0; i < chunk; i++) {
	// read data from ADC's DRAM
	nkfadc->NKFADC500read_DATA(sid, CHUNK_SIZE, data);

	// write to file
	fwrite(data, 1, CHUNK_SIZE * 1024, fp);

	k = 0;
	while (k < CHUNK_SIZE*1024) {
	  // get values from raw data

	  // get data length
	  data_length =  data[k] & 0xFF;
	  itmp = data[k+1*4] & 0xFF;
	  data_length = data_length + (unsigned int)(itmp << 8);
	  itmp = data[k+2*4] & 0xFF;
	  data_length = data_length + (unsigned int)(itmp << 16);
	  itmp = data[k+3*4] & 0xFF;
	  data_length = data_length + (unsigned int)(itmp << 24);

	  // get run number
	  run_number = data[k+4*4] & 0xFF;
	  itmp = data[k+5*4] & 0xFF;
	  run_number = run_number + (unsigned int)(itmp << 8);

	  // get trigger type
	  trigger_type = data[k+6*4] & 0x0F;

	  // get trigger destination
	  itmp = data[k+6*4] & 0xF0;
	  trigger_destination = itmp >> 4;

	  // get trigger number
	  trigger_number = data[k+7*4] & 0xFF;
	  itmp = data[k+8*4] & 0xFF;
	  trigger_number = trigger_number + (unsigned int)(itmp << 8);
	  itmp = data[k+9*4] & 0xFF;
	  trigger_number = trigger_number + (unsigned int)(itmp << 16);
	  itmp = data[k+10*4] & 0xFF;
	  trigger_number = trigger_number + (unsigned int)(itmp << 24);

	  // get trigger fine time
	  ttime = data[k+11*4] & 0xFF;
	  ttime = ttime*8;

	  // get trigger coarse time
	  ltmp = data[k+12*4] & 0xFF;
	  ttime = ttime + ltmp * 1000;
	  ltmp = data[k+13*4] & 0xFF;
	  ltmp = ltmp << 8;
	  ttime = ttime + ltmp * 1000;
	  ltmp = data[k+14*4] & 0xFF;
	  ltmp = ltmp << 16;
	  ttime = ttime + ltmp * 1000;

	  // get module id
	  mid = data[k+15*4] & 0xFF;

	  // get channel id
	  channel = data[k+16*4] & 0xFF;

	  // get local trigger number
	  local_tnum = data[k+17*4] & 0xFF;
	  itmp = data[k+18*4] & 0xFF;
	  local_tnum = local_tnum + (unsigned int)(itmp << 8);
	  itmp = data[k+19*4] & 0xFF;
	  local_tnum = local_tnum + (unsigned int)(itmp << 16);
	  itmp = data[k+20*4] & 0xFF;
	  local_tnum = local_tnum + (unsigned int)(itmp << 24);

	  // get local trigger pattern 
	  trigger_pattern = data[k+21*4] & 0xFF;
	  itmp = data[k+22*4] & 0xFF;
	  trigger_pattern = trigger_pattern + (unsigned int)(itmp << 8);
	  itmp = data[k+23*4] & 0xFF;
	  trigger_pattern = trigger_pattern + (unsigned int)(itmp << 16);
	  itmp = data[k+24*4] & 0xFF;
	  trigger_pattern = trigger_pattern + (unsigned int)(itmp << 24);

	  // get local starting fine time
	  ltime = data[k+25*4] & 0xFF;
	  ltime = ltime*8;

	  // get local starting coarse time
	  ltmp = data[k+26*4] & 0xFF;
	  ltime = ltime + ltmp*1000;
	  ltmp = data[k+27*4] & 0xFF;
	  ltmp = ltmp << 8;
	  ltime = ltime + ltmp*1000;
	  ltmp = data[k+28*4] & 0xFF;
	  ltmp = ltmp << 16;
	  ltime = ltime + ltmp*1000;
	  ltmp = data[k+29] & 0xFF;
	  ltmp = ltmp << 24;
	  ltime = ltime + ltmp*1000;
	  ltmp = data[k+30] & 0xFF;
	  ltmp = ltmp << 32;
	  ltime = ltime + ltmp*1000;
	  ltmp = data[k+31] & 0xFF;
	  ltmp = ltmp << 40;
	  ltime = ltime + ltmp*1000;


	  // get trigger fine time
	  trig_timel = data[k+11*4] & 0xFF;

	  // get trigger coarse time
	  ltmp = data[k+12*4] & 0xFF;
	  trig_timeh = ltmp;
	  ltmp = data[k+13*4] & 0xFF;
	  ltmp = ltmp << 8;
	  trig_timeh = trig_timeh + ltmp;
	  ltmp = data[k+14*4] & 0xFF;
	  ltmp = ltmp << 16;
	  trig_timeh = trig_timeh + ltmp;

	  // get local starting fine time
	  start_timel = data[k+25*4] & 0xFF;

	  // get local starting coarse time
	  ltmp = data[k+26*4] & 0xFF;
	  start_timeh = ltmp;
	  ltmp = data[k+27*4] & 0xFF;
	  ltmp = ltmp << 8;
	  start_timeh = start_timeh + ltmp;
	  ltmp = data[k+28*4] & 0xFF;
	  ltmp = ltmp << 16;
	  start_timeh = start_timeh + ltmp;

	  hist_point = (data_length - 32)/2;
	  hist_range = hist_point * 2;
	  if ( hoscd1 == 0) {
	    hoscd1 = new TH1F("hoscd1", "Channel1", hist_point, 0, hist_range);
	    hoscd2 = new TH1F("hoscd2", "Channel2", hist_point, 0, hist_range);
	    hoscd3 = new TH1F("hoscd3", "Channel3", hist_point, 0, hist_range);
	    hoscd4 = new TH1F("hoscd4", "Channel4", hist_point, 0, hist_range);
	    c1->cd(1); hoscd1->Draw();
	    c1->cd(2); hoscd2->Draw();
	    c1->cd(3); hoscd3->Draw();
	    c1->cd(4); hoscd4->Draw();
	  } else {

	    // reset histogram
	    hoscd1->Reset();
	    hoscd2->Reset();
	    hoscd3->Reset();
	    hoscd4->Reset();

	    for (j=0; j<hist_point; j++) {
	      // get ADC value
	      adc = data[k+128+j*8] & 0xFF; 
	      itmp = data[k+128+j*8+4] & 0xFF;
	      adc = adc + (unsigned int)(itmp << 8);
	      hoscd1->Fill(j*2, adc);
	      adc = data[k+128+j*8+1] & 0xFF; 
	      itmp = data[k+128+j*8+5] & 0xFF;
	      adc = adc + (unsigned int)(itmp << 8);
  	      hoscd2->Fill(j*2, adc);
	      adc = data[k+128+j*8+2] & 0xFF; 
	      itmp = data[k+128+j*8+6] & 0xFF;
	      adc = adc + (unsigned int)(itmp << 8);
 	      hoscd3->Fill(j*2, adc);
	      adc = data[k+128+j*8+3] & 0xFF; 
	      itmp = data[k+128+j*8+7] & 0xFF;
	      adc = adc + (unsigned int)(itmp << 8);
	      hoscd4->Fill(j*2, adc);
	    }	
	    adc = hoscd1->GetMinimum();
	    c1->cd(1); hoscd1->Draw();
	    c1->cd(2); hoscd2->Draw();
	    c1->cd(3); hoscd3->Draw();
	    c1->cd(4); hoscd4->Draw();
	    c1->Modified();
	    c1->Update();
	  }

	  printf("data_length = %d, pedestal = %d, trigger_type = %d, trigger_destination = %d\n", data_length, run_number, trigger_type, trigger_destination);  					
	  printf("trigger_number = %d, local_tnum = %d, trigger_pattern = %X\n", trigger_number, local_tnum, trigger_pattern);  					
	  printf("trigger time = %ld, local starting time = %ld\n", ttime, ltime);
	  printf("-------------------------------------------------------------------------------------------------------\n");
	  k = k + data_length * 4;
	  evtn++;
	  if ( evtn >= Nevent) { 
	    i = chunk;
	    slice = 0;
	    flag = 0;
	    break;
	  }
	}

      }

      if (slice) {
	// read data from ADC's DRAM
	nkfadc->NKFADC500read_DATA(sid, slice, data);

	// write to file
	fwrite(data, 1, slice * 1024, fp);

	k = 0;
	while (k < slice*1024) {
	  // get values from raw data

	  // get data length
	  data_length =  data[k] & 0xFF;
	  itmp = data[k+1*4] & 0xFF;
	  data_length = data_length + (unsigned int)(itmp << 8);
	  itmp = data[k+2*4] & 0xFF;
	  data_length = data_length + (unsigned int)(itmp << 16);
	  itmp = data[k+3*4] & 0xFF;
	  data_length = data_length + (unsigned int)(itmp << 24);

	  // get run number
	  run_number = data[k+4*4] & 0xFF;
	  itmp = data[k+5*4] & 0xFF;
	  run_number = run_number + (unsigned int)(itmp << 8);

	  // get trigger type
	  trigger_type = data[k+6*4] & 0x0F;

	  // get trigger destination
	  itmp = data[k+6*4] & 0xF0;
	  trigger_destination = itmp >> 4;

	  // get trigger number
	  trigger_number = data[k+7*4] & 0xFF;
	  itmp = data[k+8*4] & 0xFF;
	  trigger_number = trigger_number + (unsigned int)(itmp << 8);
	  itmp = data[k+9*4] & 0xFF;
	  trigger_number = trigger_number + (unsigned int)(itmp << 16);
	  itmp = data[k+10*4] & 0xFF;
	  trigger_number = trigger_number + (unsigned int)(itmp << 24);

	  // get trigger fine time
	  ttime = data[k+11*4] & 0xFF;
	  ttime = ttime*8;

	  // get trigger coarse time
	  ltmp = data[k+12*4] & 0xFF;
	  ttime = ttime + ltmp*1000;
	  ltmp = data[k+13*4] & 0xFF;
	  ltmp = ltmp << 8;
	  ttime = ttime + ltmp*1000;
	  ltmp = data[k+14*4] & 0xFF;
	  ltmp = ltmp << 16;
	  ttime = ttime + ltmp*1000;

	  // get module id
	  mid = data[k+15*4] & 0xFF;

	  // get channel id
	  channel = data[k+16*4] & 0xFF;

	  // get local trigger number
	  local_tnum = data[k+17*4] & 0xFF;
	  itmp = data[k+18*4] & 0xFF;
	  local_tnum = local_tnum + (unsigned int)(itmp << 8);
	  itmp = data[k+19*4] & 0xFF;
	  local_tnum = local_tnum + (unsigned int)(itmp << 16);
	  itmp = data[k+20*4] & 0xFF;
	  local_tnum = local_tnum + (unsigned int)(itmp << 24);

	  // get local trigger pattern 
	  trigger_pattern = data[k+21*4] & 0xFF;
	  itmp = data[k+22*4] & 0xFF;
	  trigger_pattern = trigger_pattern + (unsigned int)(itmp << 8);
	  itmp = data[k+23*4] & 0xFF;
	  trigger_pattern = trigger_pattern + (unsigned int)(itmp << 16);
	  itmp = data[k+24*4] & 0xFF;
	  trigger_pattern = trigger_pattern + (unsigned int)(itmp << 24);

	  // get local starting fine time
	  ltime = data[k+25*4] & 0xFF;
	  ltime = ltime*8;

	  // get local starting coarse time
	  ltmp = data[k+26*4] & 0xFF;
	  ltime = ltime + ltmp*1000;
	  ltmp = data[k+27*4] & 0xFF;
	  ltmp = ltmp << 8;
	  ltime = ltime + ltmp*1000;
	  ltmp = data[k+28*4] & 0xFF;
	  ltmp = ltmp << 16;
	  ltime = ltime + ltmp*1000;
	  ltmp = data[k+29] & 0xFF;
	  ltmp = ltmp << 24;
	  ltime = ltime + ltmp*1000;
	  ltmp = data[k+30] & 0xFF;
	  ltmp = ltmp << 32;
	  ltime = ltime + ltmp*1000;
	  ltmp = data[k+31] & 0xFF;
	  ltmp = ltmp << 40;
	  ltime = ltime + ltmp*1000;

	  // get trigger fine time
	  trig_timel = data[k+11*4] & 0xFF;

	  // get trigger coarse time
	  ltmp = data[k+12*4] & 0xFF;
	  trig_timeh = ltmp;
	  ltmp = data[k+13*4] & 0xFF;
	  ltmp = ltmp << 8;
	  trig_timeh = trig_timeh + ltmp;
	  ltmp = data[k+14*4] & 0xFF;
	  ltmp = ltmp << 16;
	  trig_timeh = trig_timeh + ltmp;

	  // get local starting fine time
	  start_timel = data[k+25*4] & 0xFF;

	  // get local starting coarse time
	  ltmp = data[k+26*4] & 0xFF;
	  start_timeh = ltmp;
	  ltmp = data[k+27*4] & 0xFF;
	  ltmp = ltmp << 8;
	  start_timeh = start_timeh + ltmp;
	  ltmp = data[k+28*4] & 0xFF;
	  ltmp = ltmp << 16;
	  start_timeh = start_timeh + ltmp;

	  hist_point = (data_length - 32)/2;
	  hist_range = hist_point * 2;
	  if ( hoscd1 == 0) {
	    hoscd1 = new TH1F("hoscd1", "Channel1", hist_point, 0, hist_range);
	    hoscd2 = new TH1F("hoscd2", "Channel2", hist_point, 0, hist_range);
	    hoscd3 = new TH1F("hoscd3", "Channel3", hist_point, 0, hist_range);
	    hoscd4 = new TH1F("hoscd4", "Channel4", hist_point, 0, hist_range);
	    c1->cd(1); hoscd1->Draw();
	    c1->cd(2); hoscd2->Draw();
	    c1->cd(3); hoscd3->Draw();
	    c1->cd(4); hoscd4->Draw();
	  } else {

	    // reset histogram
	    hoscd1->Reset();
	    hoscd2->Reset();
	    hoscd3->Reset();
	    hoscd4->Reset();

	    for (j=0; j<hist_point; j++) {
	      // get ADC value
	      adc = data[k+128+j*8] & 0xFF; 
	      itmp = data[k+128+j*8+4] & 0xFF;
	      adc = adc + (unsigned int)(itmp << 8);
	      hoscd1->Fill(j*2, adc);
	      adc = data[k+128+j*8+1] & 0xFF; 
	      itmp = data[k+128+j*8+5] & 0xFF;
	      adc = adc + (unsigned int)(itmp << 8);
  	      hoscd2->Fill(j*2, adc);
	      adc = data[k+128+j*8+2] & 0xFF; 
	      itmp = data[k+128+j*8+6] & 0xFF;
	      adc = adc + (unsigned int)(itmp << 8);
 	      hoscd3->Fill(j*2, adc);
	      adc = data[k+128+j*8+3] & 0xFF; 
	      itmp = data[k+128+j*8+7] & 0xFF;
	      adc = adc + (unsigned int)(itmp << 8);
	      hoscd4->Fill(j*2, adc);
	    }	
	    adc = hoscd1->GetMinimum();
	    c1->cd(1); hoscd1->Draw();
	    c1->cd(2); hoscd2->Draw();
	    c1->cd(3); hoscd3->Draw();
	    c1->cd(4); hoscd4->Draw();
	    c1->Modified();
	    c1->Update();
	  }

	  printf("data_length = %d, pedestal = %d, trigger_type = %d, trigger_destination = %d\n", data_length, run_number, trigger_type, trigger_destination);  					
	  printf("trigger_number = %d, local_tnum = %d, trigger_pattern = %X\n", trigger_number, local_tnum, trigger_pattern);  					
	  printf("trigger time = %ld, local starting time = %ld\n", ttime, ltime);
	  printf("-------------------------------------------------------------------------------------------------------\n");

	  k = k + data_length * 4;
	  evtn++;
	  if ( evtn >= Nevent) { 
	    flag = 0;
	    break;
	  }

	}
      }
    }
  }


  // close data file
  fclose(fp);

  // stop NKFADC500
  nkfadc->NKFADC500stop(sid);

  // close KFADC500
  nkfadc->NKFADC500close(sid);
  usb->USB3Exit(0);

  free(data);

  return 0;	
}

