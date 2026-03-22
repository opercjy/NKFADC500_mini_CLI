NOTICE_HOME= /usr/local/notice
NOTICE_LIB = $(NOTICE_HOME)/lib
NOTICE_INC = $(NOTICE_HOME)/include

LIBUSB_INC = /usr/include/libusb-1.0
LIBUSB_LIB = /usr/lib64

INSTALL_LIB = $(NOTICE_HOME)/lib
INSTALL_INC = $(NOTICE_HOME)/include

MODULE = programmer_nkfadc500

SrcSuf = cpp
HdrSuf = h
ObjSuf = o
OutPutOpt = -o #

CXXFLAGS = -I. -I$(LIBUSB_INC) -Wall -Wundef -Wunused -Wshadow -O2 -fPIC -m64
LDFLAGS = -L$(LIBUSB_LIB) -lusb-1.0

PROGRAMMERS = Programmer.cpp Actions.cpp Device.cpp module.cpp Option.cpp
PROGRAMMERH = Actions.h Device.h module.h Option.h
PROGRAMMERO = Programmer.o Actions.o Device.o module.o Option.o

PROGRAM = $(MODULE).exe

all: $(PROGRAM)

clean:
	@rm -rf $(PROGRAMMERO) $(PROGRAM)

$(PROGRAM): $(PROGRAMMERO)
	$(CXX) $^ $(OutPutOpt) $@ $(EXPLLINKLIBS) $(LDFLAGS)

#dependicies
Actions.cpp : Actions.h Device.h module.h
Device.cpp : Device.h module.h
