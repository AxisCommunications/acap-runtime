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

  bool GetImgDataFromStream(unsigned int stream, void** data, size_t& size,
                            uint32_t& frame_ref);

  // bool FreeBufferObj(void* stream, void* buffer_obj);

 private:
  Status NewStream(ServerContext* context,
                      const NewStreamRequest* request,
                      NewStreamResponse* response);

  Status GetFrame(ServerContext* context,
                           const GetFrameRequest* request,
                           GetFrameResponse* response);

  // bool SetResponseFromLastFrame(const uint stream, GetFrameResponse *response);

  uint32_t SaveFrame(void* data, size_t size);
  bool SetResponseToSavedFrame(uint32_t frame_ref, GetFrameResponse *response);

  Status OutputError(const char* msg, StatusCode code);
  Status OutputError(const char* msg, StatusCode code, GError* error);

  string GetTypeString(VdoFrame *frame);

  bool _verbose;
  map<uint, VdoStream*> streams;
  map<string, VdoBuffer*> buffers;

  size_t lastDataSize;
  void* lastData;
};
}  // namespace acap_runtime

#endif
