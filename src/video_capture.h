#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

/* Copyright 2022 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include <vdo-buffer.h>
#include <vdo-stream.h>

#include <deque>
#include <map>

#include "videocapture.grpc.pb.h"

using namespace std;
using namespace grpc;
using namespace videocapture::v1;

namespace acap_runtime {

typedef struct {
  uint32_t id;
  VdoBuffer* buffer;
  size_t size;
} Buffer;

typedef struct {
  VdoStream* vdo_stream;
  deque<Buffer> buffers;
} StreamAndBuffers;

class Capture final : public VideoCapture::Service {
 public:
  bool Init(const bool verbose);

  Status NewStream(ServerContext* context, const NewStreamRequest* request,
                   NewStreamResponse* response);

  Status DeleteStream(ServerContext* context,
                      const DeleteStreamRequest* request,
                      DeleteStreamResponse* response);

  Status GetFrame(ServerContext* context, const GetFrameRequest* request,
                  GetFrameResponse* response);

  bool GetImgDataFromStream(unsigned int stream, void** data, size_t& size,
                            uint32_t& frameRef);

 private:
  uint32_t SaveFrame(StreamAndBuffers& stream, VdoBuffer* vdoBuffer,
                     size_t size);

  bool SetResponseToSavedFrame(StreamAndBuffers& stream, uint32_t frameRef,
                               GetFrameResponse* response);

  void MaybeUnrefOldestFrame(StreamAndBuffers& stream);

  Status OutputError(const char* msg, StatusCode code);
  Status OutputError(const char* msg, StatusCode code, GError* error);

  string GetTypeString(VdoFrame *frame);

  bool _verbose;
  map<uint, StreamAndBuffers> streams;
  const uint32_t MAX_NBR_SAVED_FRAMES = 3;
  pthread_mutex_t mutex;
};
}  // namespace acap_runtime

#endif
