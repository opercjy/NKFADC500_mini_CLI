/*
 * module.cpp
 *
 *  Created on: Sep 16, 2012
 *      Author: Milman Evgeniy
 */
#include <iostream>
#include <cstring>
#include <libusb-1.0/libusb.h>

#include "module.h"
#include "Device.h"

libusb_context *ctx;
std::vector<Device*>* _devices = NULL;

std::vector<Device*>* getDeviceList() {
  if (_devices != NULL)
    return _devices;
  _devices = new std::vector<Device*>;
  int r;
  int i =0;
  r = libusb_init(&ctx);
  if (r < 0) {
    return NULL;
  }
  libusb_device **devs;
  libusb_device *dev;
  if (libusb_get_device_list(ctx, &devs) < 0) {
    return NULL;
  }
  while ((dev = devs[i++]) != NULL) {
    struct libusb_device_descriptor desc;
    r = libusb_get_device_descriptor(dev, &desc);
    if (r < 0) {
      return NULL;
    }
    if (desc.idVendor == vendor_id && desc.idProduct == product_id) {
      _devices->push_back(new Device(dev));
    }
  }
  return _devices;
}
