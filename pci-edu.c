
// Copyright (c) 2022 Friedt Professional Engineering Services, Inc
// SPDX-License-Identifier: MIT

#include <linux/bitops.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "pci-edu.h"

#ifndef MIN
#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#endif

MODULE_AUTHOR("Christopher Friedt <chrisfriedt@gmail.com>");
MODULE_DESCRIPTION("Driver for Jiri Slaby's Educational Qemu PCI device");
// See https://gitlab.com/qemu-project/qemu/-/blob/master/docs/specs/edu.txt
//     https://gitlab.com/qemu-project/qemu/-/blob/master/hw/misc/edu.c
MODULE_LICENSE("Dual MIT/GPL");

static unsigned long dma_mask = PCI_EDU_DMA_MASK_DEFAULT;
module_param(dma_mask, ulong, 0);

struct pci_edu_data {
  int len;
  void __iomem *regs;
  struct pci_dev *pdev;
  struct miscdevice misc;
  char nodename[64];
  struct file *owner;
  wait_queue_head_t wq;
};

struct __attribute__((aligned(BITS_PER_LONG / 8))) pci_edu_dma_op {
  u64 src_addr;
  u64 dst_addr;
  u64 size;
  u64 cmd;
};

static loff_t pci_edu_llseek(struct file *file, loff_t off, int whence) {
  struct miscdevice *misc = file->private_data;
  struct pci_edu_data *data = container_of(misc, struct pci_edu_data, misc);
  struct pci_dev *pdev = data->pdev;
  struct device *dev = &pdev->dev;

  dev_err(dev, "llseeked %s\n", misc->nodename);

  return -ENOSYS;
}

static ssize_t pci_edu_read(struct file *file, char __user *udata, size_t size,
                            loff_t *offs) {
  int rv;
  uint8_t *buf;
  dma_addr_t dma_handle;
  struct miscdevice *misc = file->private_data;
  struct pci_edu_data *data = container_of(misc, struct pci_edu_data, misc);
  struct pci_dev *pdev = data->pdev;
  struct device *dev = &pdev->dev;
  volatile struct pci_edu_dma_op *dma =
      (struct pci_edu_dma_op *)(data->regs + PCI_EDU_REG_DMA_SRC);

  size = MIN(PCI_EDU_DMA_MEM_SIZE, size);
  buf = dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
  if (!buf) {
    dev_err(dev, "Failed to allocate %zu bytes of DMA memory\n", size);
    return -ENOMEM;
  }

  dma->src_addr = PCI_EDU_DMA_MEM_ADDR;
  dma->dst_addr = (u64)dma_handle;
  dma->size = size;
  dma->cmd = PCI_EDU_REG_DMA_CMD_START | PCI_EDU_REG_DMA_CMD_FROM_EDU_TO_RAM |
             PCI_EDU_REG_DMA_CMD_INT_ENABLE;

  wait_event_interruptible(data->wq, !(dma->cmd & PCI_EDU_REG_DMA_CMD_START));

  size = dma->size;
  rv = copy_to_user(udata, buf, size);
  if (rv != 0) {
    dev_err(dev, "only copied %zu/%zu bytes\n", size - rv, size);
    size -= rv;
  }

  dev_info(dev, "read %zu bytes from %s\n", size, misc->nodename);

  dma_free_coherent(dev, size, buf, dma_handle);

  return size;
}

static ssize_t pci_edu_write(struct file *file, const char __user *udata,
                             size_t size, loff_t *offs) {
  int rv;
  uint8_t *buf;
  dma_addr_t dma_handle;
  struct miscdevice *misc = file->private_data;
  struct pci_edu_data *data = container_of(misc, struct pci_edu_data, misc);
  struct pci_dev *pdev = data->pdev;
  struct device *dev = &pdev->dev;
  volatile struct pci_edu_dma_op *dma =
      (struct pci_edu_dma_op *)(data->regs + PCI_EDU_REG_DMA_SRC);
  size = MIN(PCI_EDU_DMA_MEM_SIZE, size);

  buf = dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
  if (!buf) {
    dev_err(dev, "Failed to allocate %zu bytes of DMA memory\n", size);
    return -ENOMEM;
  }

  rv = copy_from_user(buf, udata, size);
  if (rv != 0) {
    dev_err(dev, "only copied %zu/%zu bytes\n", size - rv, size);
    size -= rv;
  }

  dma->src_addr = (u64)dma_handle;
  dma->dst_addr = PCI_EDU_DMA_MEM_ADDR;
  dma->size = size;
  dma->cmd = PCI_EDU_REG_DMA_CMD_START | PCI_EDU_REG_DMA_CMD_FROM_RAM_TO_EDU |
             PCI_EDU_REG_DMA_CMD_INT_ENABLE;

  while (dma->cmd & PCI_EDU_REG_DMA_CMD_START)
    ;

  dev_info(dev, "wrote %zu bytes to %s\n", size, misc->nodename);

  dma_free_coherent(dev, size, buf, dma_handle);

  return size;
}

static __poll_t pci_edu_poll(struct file *file,
                             struct poll_table_struct *poll) {
  struct miscdevice *misc = file->private_data;
  struct pci_edu_data *data = container_of(misc, struct pci_edu_data, misc);
  struct pci_dev *pdev = data->pdev;
  struct device *dev = &pdev->dev;

  dev_info(dev, "polled %s\n", misc->nodename);

  return -ENOSYS;
}

static long pci_edu_compat_ioctl(struct file *file, unsigned int request,
                                 unsigned long arg) {
  struct miscdevice *misc = file->private_data;
  struct pci_edu_data *data = container_of(misc, struct pci_edu_data, misc);
  struct pci_dev *pdev = data->pdev;
  struct device *dev = &pdev->dev;

  dev_info(dev, "ioctled %s\n", misc->nodename);

  return -ENOSYS;
}

static int pci_edu_open(struct inode *inode, struct file *file) {
  struct miscdevice *misc = file->private_data;
  struct pci_edu_data *data = container_of(misc, struct pci_edu_data, misc);
  struct pci_dev *pdev = data->pdev;
  struct device *dev = &pdev->dev;

  if (data->owner)
    return -EBUSY;

  dev_info(dev, "opened %s\n", misc->nodename);
  data->owner = file;

  return 0;
}

static int pci_edu_flush(struct file *file, fl_owner_t id) {
  struct miscdevice *misc = file->private_data;
  struct pci_edu_data *data = container_of(misc, struct pci_edu_data, misc);
  struct pci_dev *pdev = data->pdev;
  struct device *dev = &pdev->dev;

  dev_info(dev, "flushed %s\n", misc->nodename);

  return 0;
}

static int pci_edu_release(struct inode *inode, struct file *file) {
  struct miscdevice *misc = file->private_data;
  struct pci_edu_data *data = container_of(misc, struct pci_edu_data, misc);
  struct pci_dev *pdev = data->pdev;
  struct device *dev = &pdev->dev;

  data->owner = NULL;
  dev_info(dev, "closed %s\n", misc->nodename);

  return 0;
}

static int pci_edu_fsync(struct file *file, loff_t off1, loff_t off2,
                         int datasync) {
  struct miscdevice *misc = file->private_data;
  struct pci_edu_data *data = container_of(misc, struct pci_edu_data, misc);
  struct pci_dev *pdev = data->pdev;
  struct device *dev = &pdev->dev;

  dev_info(dev, "fsynced %s\n", misc->nodename);

  return 0;
}

static const struct file_operations pci_edu_fops = {
    .llseek = pci_edu_llseek,
    .read = pci_edu_read,
    .write = pci_edu_write,
    .poll = pci_edu_poll,
    .compat_ioctl = pci_edu_compat_ioctl,
    .open = pci_edu_open,
    .flush = pci_edu_flush,
    .release = pci_edu_release,
    .fsync = pci_edu_fsync,
};

static irqreturn_t pci_edu_handler(int irq, void *irqdata) {
  struct pci_dev *const pdev = (struct pci_dev *)irqdata;
  struct device *const dev = &pdev->dev;
  struct pci_edu_data *const data = pci_get_drvdata(pdev);
  int handled = 1;
  u32 status;

  // spin_lock(&cp->lock);
  do {
    status = readl(data->regs + PCI_EDU_REG_INT_STATUS);
    if (!status)
      break;

    dev_info(dev, "Int Status: 0x%x\n", status);

    // if status & ... (check for FACT, DMA)

    // clear previously raised interrupts
    writel(status, data->regs + PCI_EDU_REG_INT_ACK);
  } while (true);
  // spin_unlock(&cp->lock);

  wake_up(&data->wq);

  return IRQ_RETVAL(handled);
}

static unsigned pci_edu_counter;
static void *pci_edu_data_new(struct pci_dev *pdev) {
  struct pci_edu_data *data;

  data = kzalloc(sizeof(*data), GFP_KERNEL);
  if (!data)
    return NULL;

  data->pdev = pdev;
  data->misc.fops = &pci_edu_fops;
  data->misc.minor = MISC_DYNAMIC_MINOR;
  data->misc.name = PCI_EDU_DRV_NAME;
  data->misc.parent = &pdev->dev;
  snprintf(data->nodename, sizeof(data->nodename), "%s%u", PCI_EDU_DRV_NAME,
           pci_edu_counter++);
  data->misc.nodename = data->nodename;
  init_waitqueue_head(&data->wq);

  return data;
}

static int pci_edu_probe(struct pci_dev *pdev, const struct pci_device_id *id) {
  int rc;
  u32 val;
  resource_size_t pciaddr;
  struct pci_edu_data *data;
  struct device *const dev = &pdev->dev;

  data = pci_edu_data_new(pdev);
  if (!data)
    return -ENOMEM;

  rc = pci_enable_device(pdev);
  if (rc)
    goto err_out_free;

  rc = pci_request_regions(pdev, PCI_EDU_DRV_NAME);
  if (rc)
    goto err_out_disable;

  pciaddr = pci_resource_start(pdev, 0);
  if (!pciaddr) {
    rc = -EIO;
    dev_err(dev, "no MMIO resource\n");
    goto err_out_res;
  }

  data->len = pci_resource_len(pdev, 0);
  if (data->len < PCI_EDU_REGION_0_SIZE) {
    rc = -EIO;
    dev_err(dev, "MMIO resource (%x) too small\n", data->len);
    goto err_out_res;
  }

  data->regs = ioremap(pciaddr, data->len);
  if (!data->regs) {
    rc = -EIO;
    dev_err(dev, "Cannot map PCI MMIO (%x@%Lx)\n", data->len,
            (unsigned long long)pciaddr);
    goto err_out_res;
  }

  val = readl(data->regs + PCI_EDU_REG_ID);
  if ((val & 0xffff) != 0x00ed) {
    dev_err(dev, "Invalid ID register value %x\n", val);
    rc = -ENODEV;
    goto err_out_iomap;
  }

  dev_info(dev, "Found %s at %02x:%02x.%d\n", PCI_EDU_DRV_NAME,
           pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
  dev_info(dev, "MMIO %x@%p\n", data->len, data->regs);

  pci_set_drvdata(pdev, data);
  pci_set_master(pdev);

  rc = request_irq(pdev->irq, pci_edu_handler, IRQF_SHARED, PCI_EDU_DRV_NAME,
                   pdev);
  if (rc) {
    dev_err(dev, "Failed to request irq: %d\n", rc);
    goto err_out_iomap;
  }
  dev_info(dev, "Acquired IRQ %d\n", pdev->irq);

  dev_info(dev, "DMA Mask: 0x%lx\n", dma_mask);
  rc = pci_set_dma_mask(pdev, dma_mask);
  if (rc) {
    dev_err(dev, "Failed to set DMA mask to 0x%lx\n", dma_mask);
    goto err_out_irq;
  }

  dev_info(dev, "Registering misc device\n");
  rc = misc_register(&data->misc);
  if (rc) {
    dev_err(dev, "Failed to register misc device: %d\n", rc);
    goto err_out_irq;
  }

  dev_info(dev, "Success!\n");

  return 0;

err_out_irq:
  dev_info(dev, "Disabling irq %d\n", pdev->irq);
  disable_irq(pdev->irq);
  dev_info(dev, "Freeing irq %d\n", pdev->irq);
  free_irq(pdev->irq, pdev);
err_out_iomap:
  dev_info(dev, "Unmapping IO %x@%p\n", data->len, data->regs);
  iounmap(data->regs);
err_out_res:
  dev_info(dev, "Releasing pci regions\n");
  pci_release_regions(pdev);
err_out_disable:
  dev_info(dev, "Disabling pci device\n");
  pci_disable_device(pdev);
err_out_free:
  dev_info(dev, "Freeing device data\n");
  kfree(data);
  return rc;
}

static void pci_edu_remove(struct pci_dev *pdev) {
  struct device *const dev = &pdev->dev;
  struct pci_edu_data *data = pci_get_drvdata(pdev);

  dev_info(dev, "Deregistering misc device\n");
  misc_deregister(&data->misc);
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
    {PCI_DEVICE(PCI_EDU_VENDOR_ID, PCI_EDU_DEVICE_ID)}, {0}};
MODULE_DEVICE_TABLE(pci, pci_edu_ids);

static struct pci_driver pci_edu_driver = {
    .name = PCI_EDU_DRV_NAME,
    .id_table = pci_edu_ids,
    .probe = pci_edu_probe,
    .remove = pci_edu_remove,
};

module_pci_driver(pci_edu_driver);
