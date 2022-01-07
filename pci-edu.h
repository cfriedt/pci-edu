// Copyright (c) 2022 Friedt Professional Engineering Services, Inc
// SPDX-License-Identifier: MIT

#ifndef PCI_EDU_H_
#define PCI_EDU_H_

#define PCI_EDU_DRV_NAME "pci_edu"
#define PCI_EDU_VENDOR_ID 0x1234
#define PCI_EDU_DEVICE_ID 0x11e8

#define PCI_EDU_REGION_NUM 1
#define PCI_EDU_REGION_0_SIZE (1ULL << 20)

#define PCI_EDU_REG_ID 0x00
#define PCI_EDU_REG_INV 0x04
#define PCI_EDU_REG_FACT 0x08
#define PCI_EDU_REG_STATUS 0x20
#define PCI_EDU_REG_INT_STATUS 0x24
#define PCI_EDU_REG_INT_RAISE 0x60
#define PCI_EDU_REG_INT_ACK 0x64

#define PCI_EDU_REG_DMA_SRC 0x80
#define PCI_EDU_REG_DMA_DST 0x88
#define PCI_EDU_REG_DMA_SIZE 0x90
#define PCI_EDU_REG_DMA_CMD 0x98

#define PCI_EDU_REG_STATUS_FACT_BUSY 0x1
#define PCI_EDU_REG_STATUS_FACT_INT_ENABLE 0x80

#define PCI_EDU_REG_INT_STATUS_FACT 0x001
#define PCI_EDU_REG_INT_STATUS_DMA 0x100
#define PCI_EDU_REG_INT_STATUS_SWI 0x200

#define PCI_EDU_DMA_MASK_DEFAULT DMA_BIT_MASK(64)
#define PCI_EDU_DMA_MEM_ADDR 0x40000
#define PCI_EDU_DMA_MEM_SIZE 4096

#define PCI_EDU_REG_DMA_CMD_START 0x01
#define PCI_EDU_REG_DMA_CMD_FROM_RAM_TO_EDU 0x00
#define PCI_EDU_REG_DMA_CMD_FROM_EDU_TO_RAM 0x02
#define PCI_EDU_REG_DMA_CMD_INT_ENABLE 0x04

#endif
