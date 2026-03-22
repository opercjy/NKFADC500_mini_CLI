/*
 * Actions.cpp
 *
 *  Created on: Oct 2, 2012
 *      Author: evgeniy
 */

#include <iostream>
#include <libusb-1.0/libusb.h>
#include <vector>
#include <iterator>
#include <cstring>
#include <fstream>
#include <cstdlib>

#include "module.h"
#include "Device.h"
#include "Actions.h"

using namespace std;

void uploadFX3_action(Option* option, void *data) {
  Device * dev = (Device*) data;
  fstream file;
  file.open(option->args[0]);
  if (file.is_open()) {
    int length;
    char * buffer;
    file.seekg(0, ios::end);
    length = file.tellg();
    file.seekg(0, ios::beg);
    buffer = new char[length];
    file.read(buffer, length);
    cout << "Uploading firmware to fx3. Please wait." << endl;
    dev->uploadFX3Firmware(length, buffer);
    if (dev->getError() == SUCCESS) {
      cout << "FX3 Firmware has been uploaded successfully.\n";
    } else {
      cout << "Failed to upload FX3 firmware.\n";
    }
    delete[] buffer;
  } else {
    cout << "Could not open file: " << option->args[0] << endl;
  }
  file.close();
}

void uploadFPGA_action(Option* option, void *data) {
  Device * dev = (Device*) data;
  fstream file;
  file.open(option->args[0]);
  if (file.is_open()) {
    int length;
    char * buffer;
    file.seekg(0, ios::end);
    length = file.tellg();
    file.seekg(0, ios::beg);
    buffer = new char[length];
    file.read(buffer, length);
    cout << "Uploading fpga firmware. Please wait.\n";
    dev->uploadFPGAFirmware(length, buffer);
    if (dev->getError() == SUCCESS) {
      cout << "FPGA Firmware has been uploaded successfully.\n";
    } else
      cout << "Failed to upload FPGA firmware.\n";
    delete[] buffer;
  } else {
    cout << "Could not open file: " << option->args[0] << endl;
  }
  file.close();
}

void devinfo_action(Option* option, void *data) {
  Device * dev = (Device*) data;
  int version = dev->readFPGAVersion();
  if (dev->getError() != SUCCESS) {
    cout << "Failed to read FPGA vesrion  " << dev->getError() << endl;
    return;
  }
  cout << "Version: " << version << endl;
  int address = dev->readSID();
  if (dev->getError() != SUCCESS) {
    cout << "Failed to read device address " << dev->getError() << endl;
    return;
  }
  cout << "Address: " << address << endl;
}

void write_sid_action(Option* option, void *data) {
  Device * dev = (Device*) data;
  unsigned char newaddress = atoi(option->args[0]);
  dev->writeSID(newaddress);
  if (dev->getError() != SUCCESS) {
    cout << "Failed to set new address  " << dev->getError() << endl;
  }
}

void write_version_action(Option* option, void *data) {
  Device * dev = (Device*) data;
  unsigned char newversion = atoi(option->args[0]);
  dev->writeFPGAVersion(newversion);
  if (dev->getError() != SUCCESS) {
    cout << "Failed to set new version  " << dev->getError() << endl;
  }
}

void help_action(Option* option, void *data) {
  cout << "NOTICE Korea (c) 2012 <http://www.noticekorea.com/>\n"
    "Progarmmer, version 1.0\n"
    "Usage: Programmer <options>\n"
    "Options:\n"
    "  -dev <deviceid>            :Choose a device. Deviceid format is\n"
    "                             usb_bus:usb_address. Use option -list\n"
    "                             to get available device list. Use \"all\"\n"
    "                             instead of usb_bus:usb_address to\n"
    "                             select all available devices.\n"
    "  -fx3 <filename>            :Upload fx3 firmware.\n"
    "                             Option -dev has to be set.\n"
    "  -fpga <filename>           :Upload fpga firmware. \n"
    "                             Option -dev has to be set.\n"
    "  -list                      :Lists all available devices.\n"
    "  -set-address <address>     :Sets crate address. Address range is [0..255]\n"
    "                             Option -dev has to be set.\n"
    "  -set-version               :Sets firmware version.\n"
    "                             Option -dev has to be set.\n"
    "  -info                      :Shows device information such as\n"
    "                             firmware version, crate address.\n"
    "                             Option -dev has to be set.\n"
    "  -help                      :Shows this message." << endl;
  //TODO: update help message
}

void list_action(Option* option, void *data) {
  std::vector<Device*>* devices = getDeviceList();
  if (devices == NULL) {
    cout << "Failed to init libusb. " << endl;
    return;
  }
  int size = devices->size();
  if (size == 0) {
    cout << "No available devices. " << endl;
    return;
  }
  for (int i = 0; i < size; i++) {
    Device * dev = devices->at(i);
    cout << int(libusb_get_bus_number(dev->getDevice())) << ":" << int(libusb_get_device_address(dev->getDevice())) << endl;
  }
}

void dev_action(Option* option, void *data) {
  std::vector<Device*> *devices = getDeviceList();
  std::vector<Device*>* tempdevices;
  int size;
  if (!strcmp(option->args[0], "all")) {
    tempdevices = devices;
  } else {
    size = devices->size();
    tempdevices = new std::vector<Device*>;
    int bus;
    int address;
    char * devname = strtok(option->args[0], ":\n\0");
    if (devices == NULL) {
      cout << "Wrong format. Please see help for more details." << endl;
      return ;
    }
    bus = atoi(devname);
    devname = strtok(NULL, ":\n\0");
    if (devices == NULL) {
      cout << "Wrong format. Please see help for more details." << endl;
      return ;
    }
    address = atoi(devname);
    for (int i = 0; i < size; i++) {
      Device * dev = devices->at(i);
      if(libusb_get_bus_number(dev->getDevice()) == bus && libusb_get_device_address(dev->getDevice())==address){
	tempdevices->push_back(dev);
      }
    }
  }
  size = tempdevices->size();
  if (size == 0) {
    cout << "Device not found devices. " << endl;
    return;
  }
  for (int i = 0; i < size; i++) {
    Device * dev = tempdevices->at(i);
    cout << int(libusb_get_bus_number(dev->getDevice())) << ":" << int(libusb_get_device_address(dev->getDevice())) << endl;
    dev->open();
    if(dev->getError()!=SUCCESS){
      cout << "Failed to open device" << endl;
      continue;
    }
    for (int j = 0; j < option->chidrensize; j++) {
      if (option->children[j]->enabled) {
	option->children[j]->runAction(option->children[j], dev);
      }
    }
  }
}

