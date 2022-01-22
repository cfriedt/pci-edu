# Copyright (c) 2022 Friedt Professional Engineering Services, Inc
# SPDX-License-Identifier: GPLv2

.PHONY: all clean check module module-clean module-check app app-clean app-check

OS = $(shell uname -s)

ifeq ($(OS),Darwin)
all: app
clean: app-clean
check: app-check
else
all: app module
clean: app-clean module-clean
check: app-check module-check
endif



# Thrift Application

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

ifeq ($(OS),Darwin)
else
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
CPPFLAGS += -I$(BOOST_DIR)/include
LDLIBS += -L$(BOOST_DIR)/lib
else
CPPFLAGS += $(shell pkg-config --cflags boost)
LDFLAGS += $(shell pkg-config --libs boost)
endif

CLEANFILES += edu-client-pci edu-client-socket edu-server
app: edu-client-pci edu-client-socket edu-server

%.thrift.stamp: %.thrift
	thrift -r --gen cpp $<
	touch $@

CLEANFILES += edu.thrift.stamp
$(GENSRC): edu.thrift.stamp

%.o: %.cpp $(HDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

edu-client-pci: edu-client.cpp $(OBJ)
	$(CXX) $(CPPFLAGS) -DUSE_FD_TRANSPORT=1 $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

edu-client-socket: edu-client.cpp $(OBJ)
	$(CXX) $(CPPFLAGS) -DUSE_FD_TRANSPORT=0 $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

edu-server: edu-server.cpp $(OBJ)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

app-clean:
	rm -Rf $(GENDIR) $(CLEANFILES) *.o || true

# Linux kernel OOT module
obj-m += pci-edu.o
module:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

module-clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean || true

app-check: edu-client-socket edu-server
	./edu-server& \
	SERVER_PID=$$!; \
	sleep 1; \
	./edu-client-socket; \
	SUCCESS=$$?; \
	kill -TERM $$SERVER_PID && \
	wait $$SERVER_PID >/dev/null 2>&1; \
	exit $$SUCCESS

# TODO: module-check
module-check:
	true

