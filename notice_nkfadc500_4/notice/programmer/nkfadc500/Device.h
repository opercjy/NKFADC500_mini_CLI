#ifndef DEVICE_H_
#define DEVICE_H_

#include <libusb-1.0/libusb.h>

#include "module.h"

class Device {
 private:
  libusb_device_handle * handle;
  libusb_device * device;
  int progress;
  ErrorCode error;
  char * ep0buffer;
  ErrorCode read(int request, uint16_t value, uint16_t index, size_t lenght, void *data);
  ErrorCode write(int request, uint16_t value, uint16_t index, size_t lenght, void *data);
 public:
  Device(libusb_device * device);
  ~Device();

  void open();
  void close();
  void uploadFX3Firmware(size_t lenght, char * data);
  void uploadFPGAFirmware(size_t lenght, char * data);
  unsigned char readSID();
  void writeSID(unsigned char value);
  unsigned char readFPGAVersion();
  void writeFPGAVersion(unsigned char value);
  ErrorCode getError();
  libusb_device *getDevice() const;
};

#endif /* DEVICE_H_ */


