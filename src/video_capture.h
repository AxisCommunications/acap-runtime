/* Copyright 2022 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include <vdo-buffer.h>
#include <vdo-stream.h>

#include "videocapture.grpc.pb.h"

using namespace std;
using namespace grpc;
using namespace videocapture;

namespace acap_runtime {

// Logic and data behind the server's behavior.
class Capture final : public VideoCapture::Service {
 public:
  bool Init(const bool verbose);

 private:
  Status GetValues(ServerContext* context,
                   ServerReaderWriter<videocapture::Response,
                                      videocapture::Request>* stream);

  Status VdoStreamNew(ServerContext* context,
                      const VdoStreamNewRequest* request,
                      VdoStreamNewResponse* response);

  bool _verbose;

  map<uint, VdoStream*> streams;
  map<string, VdoBuffer*> buffers;
};
}  // namespace acap_runtime
