#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

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
  bool GetFileDescFromStream(unsigned int stream, int& fd);

 private:
  Status NewStream(ServerContext* context,
                      const NewStreamRequest* request,
                      NewStreamResponse* response);

  Status GetFrame(ServerContext* context,
                           const GetFrameRequest* request,
                           GetFrameResponse* response);

  Status OutputError(const char* msg, StatusCode code);
  Status OutputError(const char* msg, StatusCode code, GError* error);

  string GetTypeString(VdoFrame *frame);

  bool _verbose;
  map<uint, VdoStream*> streams;
  map<string, VdoBuffer*> buffers;
};
}  // namespace acap_runtime

#endif
