
// Copyright (c) 2022 Friedt Professional Engineering Services, Inc
// SPDX-License-Identifier: MIT

#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/irq.h>

#include "pci-edu.h"

MODULE_AUTHOR("Christopher Friedt <chrisfriedt@gmail.com>");
MODULE_DESCRIPTION("Driver for Jiri Slaby's Educational Qemu PCI device");
// See https://gitlab.com/qemu-project/qemu/-/blob/master/docs/specs/edu.txt
//     https://gitlab.com/qemu-project/qemu/-/blob/master/hw/misc/edu.c
MODULE_LICENSE("MIT");

static u64 dma_mask = PCI_EDU_DMA_MASK_DEFAULT;
// module_param(dma_mask, long, 0);

struct pci_edu_data
{
  int len;
  void __iomem *regs;
  struct pci_dev *dev;
  dma_addr_t dma_handle;
  uint8_t *buf;
};

struct __attribute__((aligned(BITS_PER_LONG / 8)))
pci_edu_dma_op
{
  u64 src_addr;
  u64 dst_addr;
  u64 size;
  u64 cmd;
};

static irqreturn_t pci_edu_handler(int irq, void *irqdata)
{
  struct pci_dev *const pdev = (struct pci_dev *)irqdata;
  struct device *const dev = &pdev->dev;
  struct pci_edu_data *const data = pci_get_drvdata(pdev);
  int handled = 1;
  u32 status;

  // spin_lock(&cp->lock);
  do
  {
    status = readl(data->regs + PCI_EDU_REG_INT_STATUS);
    if (!status)
      break;

    dev_info(dev, "Int Status: 0x%x\n", status);

    // if status & ... (check for FACT, DMA)

    // clear previously raised interrupts
    writel(status, data->regs + PCI_EDU_REG_INT_ACK);
  } while (true);
  // spin_unlock(&cp->lock);

  return IRQ_RETVAL(handled);
}

static int pci_edu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
  int rc;
  u32 val;
  resource_size_t pciaddr;
  struct pci_edu_data *data;
  volatile struct pci_edu_dma_op *dma;
  struct device *const dev = &pdev->dev;

  data = kzalloc(sizeof(*data), GFP_KERNEL);
  if (!data)
    return -ENOMEM;

  rc = pci_enable_device(pdev);
  if (rc)
    goto err_out_free;

  rc = pci_request_regions(pdev, PCI_EDU_DRV_NAME);
  if (rc)
    goto err_out_disable;

  pciaddr = pci_resource_start(pdev, 0);
  if (!pciaddr)
  {
    rc = -EIO;
    dev_err(dev, "no MMIO resource\n");
    goto err_out_res;
  }

  data->len = pci_resource_len(pdev, 0);
  if (data->len < PCI_EDU_REGION_0_SIZE)
  {
    rc = -EIO;
    dev_err(dev, "MMIO resource (%x) too small\n", data->len);
    goto err_out_res;
  }

  data->regs = ioremap(pciaddr, data->len);
  if (!data->regs)
  {
    rc = -EIO;
    dev_err(dev, "Cannot map PCI MMIO (%x@%Lx)\n",
            data->len, (unsigned long long)pciaddr);
    goto err_out_res;
  }

  val = readl(data->regs + PCI_EDU_REG_ID);
  if ((val & 0xffff) != 0x00ed)
  {
    dev_err(dev, "Invalid ID register value %x\n", val);
    rc = -ENODEV;
    goto err_out_iomap;
  }

  dev_info(dev, "Found %s at %02x:%02x.%d\n", PCI_EDU_DRV_NAME, pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
  dev_info(dev, "MMIO %x@%p\n", data->len, data->regs);

  pci_set_drvdata(pdev, data);
  pci_set_master(pdev);

  rc = request_irq(pdev->irq, pci_edu_handler, IRQF_SHARED, PCI_EDU_DRV_NAME, pdev);
  if (rc)
  {
    dev_err(dev, "Failed to request irq: %d\n", rc);
    goto err_out_iomap;
  }
  dev_info(dev, "Acquired IRQ %d\n", pdev->irq);

  dev_info(dev, "DMA Mask: 0x%llx\n", dma_mask);
  rc = pci_set_dma_mask(pdev, dma_mask);
  if (rc)
  {
    dev_err(dev, "Failed to set DMA mask to 0x%llx\n", dma_mask);
    goto err_out_irq;
  }

  dev_info(dev, "Computing ~0xa0a0a0a0..\n");
  writel(0xa0a0a0a0, data->regs + PCI_EDU_REG_INV);
  val = readl(data->regs + PCI_EDU_REG_INV);
  dev_info(dev, "~0xa0a0a0a0 = 0x%x\n", val);

  dev_info(dev, "Computing 6!..\n");
  writel(PCI_EDU_REG_STATUS_FACT_INT_ENABLE, data->regs + PCI_EDU_REG_STATUS);
  writel(6, data->regs + PCI_EDU_REG_FACT);
  while (readl(data->regs + PCI_EDU_REG_STATUS) & PCI_EDU_REG_STATUS_FACT_BUSY)
    ;
  val = readl(data->regs + PCI_EDU_REG_FACT);
  dev_info(dev, "6! = %u\n", val);

  dev_info(dev, "Triggering software interrupt..\n");
  writel(PCI_EDU_REG_INT_STATUS_SWI, data->regs + PCI_EDU_REG_INT_RAISE);

  dev_info(dev, "Writing to device memory..\n");
  dma = (struct pci_edu_dma_op *)(data->regs + PCI_EDU_REG_DMA_SRC);
  data->buf = dma_alloc_coherent(dev, PCI_EDU_DMA_MEM_SIZE, &data->dma_handle, GFP_KERNEL);
  if (!data->buf)
  {
    dev_err(dev, "Failed to allocate %u bytes for DMA xfer\n", PCI_EDU_DMA_MEM_SIZE);
    return 0;
  }
  memset(data->buf, 42, PCI_EDU_DMA_MEM_SIZE);
  dma->src_addr = (u64)data->dma_handle;
  dma->dst_addr = PCI_EDU_DMA_MEM_ADDR;
  dma->size = PCI_EDU_DMA_MEM_SIZE;
  dma->cmd = PCI_EDU_REG_DMA_CMD_START | PCI_EDU_REG_DMA_CMD_FROM_RAM_TO_EDU | PCI_EDU_REG_DMA_CMD_INT_ENABLE;

  dev_info(dev, "Waiting for write..\n");
  while (dma->cmd & 1)
    ;
  dev_info(dev, "Done waiting for write\n");

  dev_info(dev, "Reading from device memory..\n");
  memset(data->buf, 0, PCI_EDU_DMA_MEM_SIZE);
  dma->src_addr = PCI_EDU_DMA_MEM_ADDR;
  dma->dst_addr = (u64)data->dma_handle;
  dma->size = PCI_EDU_DMA_MEM_SIZE;
  dma->cmd = PCI_EDU_REG_DMA_CMD_START | PCI_EDU_REG_DMA_CMD_FROM_EDU_TO_RAM | PCI_EDU_REG_DMA_CMD_INT_ENABLE;

  dev_info(dev, "Waiting for read..\n");
  while (dma->cmd & 1)
    ;
  dev_info(dev, "Done waiting for read\n");

  for (val = 0; val < PCI_EDU_DMA_MEM_SIZE; ++val)
  {
    if (data->buf[val] != 42)
    {
      dev_err(dev, "DMA test failed at index %d\n", val);
      return 0;
    }
  }
  dev_info(dev, "DMA test succeeded!\n");

  return 0;

err_out_irq:
  disable_irq(pdev->irq);
  free_irq(pdev->irq, pdev);
err_out_iomap:
  iounmap(data->regs);
err_out_res:
  pci_release_regions(pdev);
err_out_disable:
  pci_disable_device(pdev);
err_out_free:
  kfree(data);
  return rc;
}

static void pci_edu_remove(struct pci_dev *pdev)
{
  struct device *const dev = &pdev->dev;
  struct pci_edu_data *data = pci_get_drvdata(pdev);

  dev_info(dev, "Freeing DMA memory\n");
  dma_free_coherent(dev, PCI_EDU_DMA_MEM_SIZE, data->buf, data->dma_handle);
  dev_info(dev, "Disabling irq %d\n", pdev->irq);
  disable_irq(pdev->irq);
  dev_info(dev, "Freeing irq %d\n", pdev->irq);
  free_irq(pdev->irq, pdev);
  dev_info(dev, "Unmapping IO %x@%p\n", data->len, data->regs);
  iounmap(data->regs);
  dev_info(dev, "Releasing pci regions\n");
  pci_release_regions(pdev);
  dev_info(dev, "Disabling pci device\n");
  pci_disable_device(pdev);
  dev_info(dev, "Freeing device data\n");
  kfree(data);
}

static struct pci_device_id pci_edu_ids[] = {
    {PCI_DEVICE(PCI_EDU_VENDOR_ID, PCI_EDU_DEVICE_ID)},
    {0}};
MODULE_DEVICE_TABLE(pci, pci_edu_ids);

static struct pci_driver pci_edu_driver = {
    .name = PCI_EDU_DRV_NAME,
    .id_table = pci_edu_ids,
    .probe = pci_edu_probe,
    .remove = pci_edu_remove,
};

module_pci_driver(pci_edu_driver);
