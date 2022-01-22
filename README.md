What even is this?

This is a proof of concept that illustrates how easy it *could* be to interact with custom firmware on a PCIe device. A simple illustration is included below, where we have the Zephyr RTOS running on a virtual PCIe device.

TODO: insert graphic from https://asciiflow.com/legacy/

The main tool in use here is [Apache Thrift](https://github.com/apache/thrift) which is an [RPC framework](https://en.wikipedia.org/wiki/Remote_procedure_call) and code generator. Thrift allows the application programmer to develop and link code that works equally well over a `/dev` node as over a TCP socket. In Linux, both are simple file descriptors.

Instructions

These instruction assume that you are using Ubuntu Impish Indri, but they can be adapted for a number of operating systems.

Install Qemu build dependencies (Ubuntu Impish Indri)
```
sudo apt-get update
sudo apt install -y ...
```

Build a patched version of qemu, the demonstration app, and the module
```
git clone --recurse-submodules https://github.com/cfriedt/pci-edu
cd pci-edu
make -j
```

Run the socket-based test
```
make app-check
```

The output of the test should be match the following:
```
Starting the server...
ping()
echo('Hello, world!')
counter() => 0
counter() => 1
counter() => 2
counter() => 3
counter() => 4
```

Run the pcie-based test
```
make module-check
```

The output of the test should be match the following:
```
Starting the server...
ping()
echo('Hello, world!')
counter() => 0
counter() => 1
counter() => 2
counter() => 3
counter() => 4
```
