#include "Device.h"
#include <cstring>
#include <iostream>

ErrorCode Device::read(int request, uint16_t value, uint16_t index, size_t lenght, void *data) {
  int res;
  if (handle == NULL) {
    return NULL_POINTER;
  }
  res = libusb_control_transfer(handle, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN, request, value, index,
				(unsigned char *) data, lenght, 5000);
  if (res < 0) {
    return LIBUSB_ERROR;
  }
  return SUCCESS;
}

ErrorCode Device::write(int request, uint16_t value, uint16_t index, size_t lenght, void *data) {
  int res;
  if (handle == NULL) {
    return NULL_POINTER;
  }
  res = libusb_control_transfer(handle, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT, request, value, index,
				(unsigned char *) data, lenght, 5000);
  if (res < 0) {
    std::cout << "ERROR CODE  " << res << std::endl;
    return LIBUSB_ERROR;
  }
  return SUCCESS;
}

void Device::uploadFX3Firmware(size_t size, char *data) 
{
  int nsector;
  int naddr;
  int rem;
  int sector;
  int addr;
  int j;
  
  nsector = size / 65536;
  naddr = (size % 65536) / 256;
  rem = (size % 65536) % 256;
  
  for (sector = 0; sector < nsector; sector++) {
    for (addr = 0; addr < 256; addr++) {
      ep0buffer[0] = sector & 0xFF;
      ep0buffer[1] = addr & 0xFF;
      for (j = 0; j < 256; j++)
	ep0buffer[j + 2] = data[sector * 65536 + addr * 256 + j];

      error = write(VENDOR_I2C_EEPROM_WRITE, 0, 0, 258, ep0buffer);
      std::cerr << ".";
    }
  }

  for (addr = 0; addr < naddr; addr++) {
    ep0buffer[0] = nsector & 0xFF;
    ep0buffer[1] = addr & 0xFF;
    for (j = 0; j < 256; j++)
      ep0buffer[j + 2] = data[nsector * 65536 + addr * 256 + j];
    
    error = write(VENDOR_I2C_EEPROM_WRITE, 0, 0, 258, ep0buffer);
    std::cerr << ".";
  }

  if (rem) {
    ep0buffer[0] = nsector & 0xFF;
    ep0buffer[1] = naddr & 0xFF;
    for (j = 0; j < rem; j++)
      ep0buffer[j + 2] = data[nsector * 65536 + naddr * 256 + j];

    for (j = rem; j < 256; j++)
      ep0buffer[j + 2] = data[nsector * 65536 + naddr * 256 + rem - 1];

    error = write(VENDOR_I2C_EEPROM_WRITE, 0, 0, 258, ep0buffer);
    std::cerr << ".";
  }

  std::cerr << std::endl;
  error = SUCCESS;
}

void Device::uploadFPGAFirmware(size_t size, char *data) 
{
  int nsector;
  int naddr;
  int rem;
  int sector;
  int addr;
  int j;
  
  nsector = size / 65536;
  naddr = (size % 65536) / 256;
  rem = (size % 65536) % 256;

  for (sector = 0; sector < nsector; sector++) {
    // erase sector
    ep0buffer[0] = (sector + 2) & 0xFF;
    error = write(VENDOR_FLASH_ERASE, 0, 0, 1, ep0buffer);
    
    // write data
    for (addr = 0; addr < 256; addr++) {
      ep0buffer[0] = (sector + 2) & 0xFF;
      ep0buffer[1] = addr & 0xFF;
      for (j = 0; j < 256; j++)
	ep0buffer[j + 2] = data[sector * 65536 + addr * 256 + j];

      error = write(VENDOR_FLASH_WRITE, 0, 0, 258, ep0buffer);
    }
    
    std::cerr << ".";
  }

  // erase sector
  ep0buffer[0] = (nsector + 2) & 0xFF;
  error = write(VENDOR_FLASH_ERASE, 0, 0, 1, ep0buffer);

  // write data
  for (addr = 0; addr < naddr; addr++) {
    ep0buffer[0] = (nsector + 2) & 0xFF;
    ep0buffer[1] = addr & 0xFF;
    for (j = 0; j < 256; j++)
      ep0buffer[j + 2] = data[nsector * 65536 + addr * 256 + j];
    
    error = write(VENDOR_FLASH_WRITE, 0, 0, 258, ep0buffer);
  }

  if (rem) {
    ep0buffer[0] = (nsector + 2) & 0xFF;
    ep0buffer[1] = naddr & 0xFF;
    for (j = 0; j < rem; j++)
      ep0buffer[j + 2] = data[nsector * 65536 + naddr * 256 + j];

    for (j = rem; j < 256; j++)
      ep0buffer[j + 2] = data[nsector * 65536 + naddr * 256 + rem - 1];

    error = write(VENDOR_FLASH_WRITE, 0, 0, 258, ep0buffer);
  }
  
  std::cerr << ".";

  // finish flash
  ep0buffer[0] = 0;
  error = write(VENDOR_FLASH_FINISH, 0, 0, 0, ep0buffer);

  std::cerr << std::endl;
  error = SUCCESS;
}

unsigned char Device::readSID() 
{
  unsigned char result;

  error = read(VENDOR_READ_SID, 0, 0, 1, ep0buffer);
  if (error != SUCCESS) {
    return 0;
  }
  result = ep0buffer[0];

  return result;
}

void Device::writeSID(unsigned char value) 
{
  if (value > 255) {
    error = WRONG_VALUE;
    return;
  }

  ep0buffer[0] = value & 0xFF;
  error = write(VENDOR_WRITE_SID, 0, 0, 1, ep0buffer);
  if (error != SUCCESS) {
    return;
  }
}

unsigned char Device::readFPGAVersion() 
{
  unsigned char result;

  error = read(VENDOR_READ_FPGA_VERSION, 0, 0, 1, ep0buffer);
  if (error != SUCCESS) {
    return 0;
  }
  result = ep0buffer[0];

  return result;
}

void Device::writeFPGAVersion(unsigned char value)  
{
  if (value > 255) {
    error = WRONG_VALUE;
    return;
  }

  ep0buffer[0] = value & 0xFF;
  error = write(VENDOR_WRITE_FPGA_VERSION, 0, 0, 1, ep0buffer);
  if (error != SUCCESS) {
    return;
  }
}


Device::Device(libusb_device * _device) {
  ep0buffer = new char[260];
  device = _device;
}

Device::~Device() {
  delete[] ep0buffer;
}

void Device::open() {
  handle = NULL;
  int r = libusb_open(device, &handle);
  if (r < 0) {
    handle = NULL;
    error = DEVICE_CANNOT_BE_OPENED;
  } else {
    error = SUCCESS;
  }

}

void Device::close() {
  if (handle != NULL) {
    libusb_close(handle);
  }
}

libusb_device *Device::getDevice() const {
  return device;
}

ErrorCode Device::getError() {
  return error;
}

