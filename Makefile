# Copyright (c) 2022 Friedt Professional Engineering Services, Inc
# SPDX-License-Identifier: GPLv2

obj-m += pci-edu.o

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
