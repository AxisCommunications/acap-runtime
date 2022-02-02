/* Copyright 2021 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include "keyvaluestore.grpc.pb.h"

#ifdef TEST
  #define APP_NAME "acapruntimetest"
#else
  #define APP_NAME "acapruntime"
#endif

using namespace grpc;
using namespace keyvaluestore;

namespace acap_runtime {

  // Logic and data behind the server's behavior.
  class Parameter final : public KeyValueStore::Service {

  public:
    bool Init(const bool verbose);

  private:
    Status GetValues(ServerContext* context,
      ServerReaderWriter<Response, Request>* stream);

    bool _verbose;
  };
}  // namespace acap_runtime
