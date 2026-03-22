#ifndef module_H_
#define module_H_

#include <libusb-1.0/libusb.h>
#include <vector>

class Device;

#define  vendor_id  0x0547
#define  product_id 0x1502


// module protocol
#define VENDOR_I2C_EEPROM_WRITE       (0xBA)
#define VENDOR_FLASH_WRITE            (0xC2)
#define VENDOR_FLASH_ERASE            (0xC4)
#define VENDOR_FLASH_FINISH           (0xC5)

#define VENDOR_WRITE_FPGA_VERSION     (0xD5)
#define VENDOR_READ_FPGA_VERSION      (0xD4)
#define VENDOR_WRITE_SID              (0xD3)
#define VENDOR_READ_SID               (0xD2)


// --------------

enum ErrorCode{
	SUCCESS = 0,
	DEVICE_NOT_FOUND,
	DEVICE_CANNOT_BE_OPENED,
	SESSION_INITIALIZATION_FAILED,
	UNKNOWN,
	NULL_POINTER,
	LIBUSB_ERROR,
	WRONG_VALUE
};


std::vector<Device*> * getDeviceList();


#endif 
