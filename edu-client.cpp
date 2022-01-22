// Copyright (c) 2022 Friedt Professional Engineering Services, Inc
// SPDX-License-Identifier: MIT

#include <cstdlib>
#include <iostream>

#include <fcntl.h>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TFDTransport.h>
#include <thrift/transport/TSocket.h>

#include "Edu.h"

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

int main(void) {
#if USE_FD_TRANSPORT
  int fd = open("/dev/pci_edu0", O_RDWR);
  if (fd == -1) {
    perror("open");
    return EXIT_FAILURE;
  }
  std::shared_ptr<TTransport> trans(
      new TFDTransport(fd, TFDTransport::ClosePolicy::CLOSE_ON_DESTROY));
#else
  std::shared_ptr<TTransport> trans(new TSocket("localhost", 4242));
#endif
  std::shared_ptr<TTransport> transport(new TBufferedTransport(trans));
  std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
  EduClient client(protocol);

  try {
    transport->open();
    client.ping();
    string s;
    client.echo(s, "Hello, world!");
    for (int i = 0; i < 5; ++i) {
        client.counter();
    }
    transport->close();
  } catch (TException &tx) {
    cout << "ERROR: " << tx.what() << endl;
  }

  return EXIT_SUCCESS;
}