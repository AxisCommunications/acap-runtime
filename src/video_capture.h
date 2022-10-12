#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

/* Copyright 2022 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include <vdo-buffer.h>
#include <vdo-stream.h>

#include <map>
#include <queue>

#include "videocapture.grpc.pb.h"

using namespace std;
using namespace grpc;
using namespace videocapture;

namespace acap_runtime {

typedef struct {
  VdoStream* vdo_stream;
  VdoBuffer* vdo_buffer;
  size_t size;
  //void* data;
} frame;

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

  uint32_t SaveFrame(VdoStream* stream, VdoBuffer* buffer, size_t size);
  bool SetResponseToSavedFrame(uint32_t frame_ref, GetFrameResponse *response);

  Status OutputError(const char* msg, StatusCode code);
  Status OutputError(const char* msg, StatusCode code, GError* error);

  string GetTypeString(VdoFrame *frame);

  bool _verbose;
  map<uint, VdoStream*> streams;
  map<string, VdoBuffer*> buffers;

  map<uint32_t, frame> frame_map;
  queue<uint32_t> frame_queue;
  const uint32_t MAX_NBR_SAVED_FRAMES = 3;

  pthread_mutex_t mutex;
};
}  // namespace acap_runtime

#endif
