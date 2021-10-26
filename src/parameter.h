/* Copyright 2021 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include "keyvaluestore.grpc.pb.h"

using namespace grpc;
using namespace keyvaluestore;

namespace acap_runtime {
  // Logic and data behind the server's behavior.
  class Parameter final : public KeyValueStore::Service {
    Status GetValues(ServerContext* context,
      ServerReaderWriter<Response, Request>* stream);
  };
}  // namespace acap_runtime
