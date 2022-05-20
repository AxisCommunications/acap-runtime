/* Copyright 2022 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include "videocapture.grpc.pb.h"

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

  bool _verbose;
};
}  // namespace acap_runtime
