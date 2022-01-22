// Copyright (c) 2022 Friedt Professional Engineering Services, Inc
// SPDX-License-Identifier: MIT

#include <thrift/TToString.h>
#include <thrift/concurrency/ThreadFactory.h>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>

#include <iostream>
#include <sstream>
#include <stdexcept>

#include "Edu.h"

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::concurrency;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;

class EduHandler : public EduIf {
public:
  EduHandler() : count(0){};
  ~EduHandler(){};

  void ping() override { cout << "ping()" << endl; }

  void echo(std::string &_return, const std::string &msg) override {
    cout << "echo('" << msg << "')" << endl;
    _return = msg;
  }

  int32_t counter() override {
    cout << "counter() => " << this->count << endl;
    return this->count++;
  }

protected:
  int count;
};

int main(void) {
  std::shared_ptr<EduHandler> handler(new EduHandler());
  std::shared_ptr<TProcessor> processor(new EduProcessor(handler));

  TSimpleServer server(processor,
                       std::make_shared<TServerSocket>(4242), // port
                       std::make_shared<TBufferedTransportFactory>(),
                       std::make_shared<TBinaryProtocolFactory>());

  cout << "Starting the server..." << endl;
  server.serve();
  cout << "Done." << endl;
  return 0;
}