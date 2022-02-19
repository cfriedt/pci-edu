# Copyright (c) 2022 Friedt Professional Engineering Services, Inc
# SPDX-License-Identifier: GPLv2

.PHONY: all clean check module module-clean module-check app app-clean app-check

OS = $(shell uname -s)

all: app module
clean: app-clean module-clean
check: app-check module-check

CXX = g++

CLEANFILES =
CPPFLAGS :=
CFLAGS :=
CXXFLAGS :=
LDFLAGS :=

GENDIR = gen-cpp
GENSRC = $(GENDIR)/Edu.cpp $(GENDIR)/Edu_server.skeleton.cpp $(GENDIR)/Edu.h $(GENDIR)/edu-types.h
GENHDR = $(filter %.h, $(GENSRC))
GENOBJ = $(filter-out %.h %.skeleton.o, $(GENSRC:.cpp=.o))

SRC = $(filter-out $(GENSRC), $(shell find * -name '*.cpp'))
HDR = $(GENHDR)
OBJ = $(GENOBJ)

CLEANFILES += $(OBJ)

CFLAGS += -Wall -Werror -Wextra
CXXFLAGS += $(CFLAGS)

CFLAGS += -std=c17
CXXFLAGS += -std=c++17

ifneq ($(OS),Darwin)
LDFLAGS += -Wl,-no-undefined
LDFLAGS += -Wl,-rpath,/usr/local/lib
endif

# thrift flags
CPPFLAGS += -I$(GENDIR)
CPPFLAGS += $(shell pkg-config --cflags thrift)
LDLIBS += $(shell pkg-config --libs thrift)

# boost flags
ifeq ($(OS),Darwin)
BOOST_DIR = $(shell brew --prefix)/Cellar/boost/1.76.0
else
BOOST_DIR = /usr
endif
CPPFLAGS += -I$(BOOST_DIR)/include
LDLIBS += -L$(BOOST_DIR)/lib

# Thrift Application
CLEANFILES += edu-client-pci edu-client-socket edu-server-pci edu-server-socket
app: edu-client-pci edu-client-socket edu-server-pci edu-server-socket

%.thrift.stamp: %.thrift
	thrift -r --gen cpp $<
	touch $@

CLEANFILES += edu.thrift.stamp
$(GENSRC): edu.thrift.stamp

%.o: %.cpp $(HDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

ifeq ($(OS),Linux)
edu-client-pci: edu-client.cpp $(OBJ)
	$(CXX) $(CPPFLAGS) -DUSE_FD_TRANSPORT=1 $(CXXFLAGS) $(LDFLAGS) -static -o $@ $^ $(LDLIBS)

edu-server-pci: edu-server.cpp thrift/transport/TFDServer.o $(OBJ)
	$(CXX) $(CPPFLAGS) -DUSE_FD_TRANSPORT=1 $(CXXFLAGS) $(LDFLAGS) -static -o $@ $^ $(LDLIBS)
else
edu-client-pci: edu-client-pci.static
	cp $< $@

edu-server-pci: edu-server-pci.static
	cp $< $@
endif

edu-client-socket: edu-client.cpp $(OBJ)
	$(CXX) $(CPPFLAGS) -DUSE_FD_TRANSPORT=0 $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

edu-server-socket: edu-server.cpp $(OBJ)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

app-clean:
	rm -Rf $(GENDIR) $(CLEANFILES) *.o || true

app-check: edu-client-socket edu-server-socket
	./edu-server-socket& \
	SERVER_PID=$$!; \
	sleep 1; \
	./edu-client-socket; \
	SUCCESS=$$?; \
	kill -TERM $$SERVER_PID  >/dev/null 2>&1; \
	wait $$SERVER_PID >/dev/null 2>&1; \
	exit $$SUCCESS

# Linux kernel OOT module
ifeq ($(OS),Linux)
obj-m += pci-edu.o
module:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

module-clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean || true
else
module:
	true

module-clean:
	true
endif

QEMU_OPTS = \
	-machine q35 \
	-device edu \
	-netdev user,id=eth0,hostfwd=tcp::2222-:22 \
	-device e1000,netdev=eth0 \
	-kernel bzImage

QEMU_DEVICE_OPTS = \
	-machine q35 \
	-device edu,device=on \
	-netdev user,id=eth0,hostfwd=tcp::2223-:22 \
	-device e1000,netdev=eth0 \
	-kernel bzImage

SSH_OPTS = -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=QUIET

module-check: bzImage pci-edu.ko edu-client-pci edu-server-pci
	qemu-system-x86_64 $(QEMU_DEVICE_OPTS) & \
	SERVER_PID=$$!; \
	scp $(SSH_OPTS) -P 2223 pci-edu.ko edu-server-pci root@localhost:/root; \
	ssh $(SSH_OPTS) -p 2223 root@localhost '/sbin/insmod pci-edu.ko && ./edu-server-pci'; \
	sleep 1; \
	qemu-system-x86_64 $(QEMU_OPTS) & \
	QEMU_PID=$$!; \
	sleep 1; \
	scp $(SSH_OPTS) -P 2222 pci-edu.ko edu-client-pci root@localhost:/root; \
	ssh $(SSH_OPTS) -p 2222 root@localhost '/sbin/insmod pci-edu.ko && ./edu-client-pci'; \
	SUCCESS=$$?; \
	kill -TERM $$SERVER_PID >/dev/null 2>&1; \
	wait $$SERVER_PID >/dev/null 2>&1; \
	kill -TERM $$QEMU_PID >/dev/null 2>&1; \
	wait $$QEMU_PID >/dev/null 2>&1; \
	exit $$SUCCESS
