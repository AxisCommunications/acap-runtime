/* Copyright 2022 Axis Communications AB. All Rights Reserved.
==============================================================================*/

#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

#include <vdo-buffer.h>
#include <vdo-stream.h>

#include <deque>
#include <map>

#include "videocapture.grpc.pb.h"

using namespace std;
using namespace grpc;
using namespace videocapture::v1;

namespace acap_runtime {

struct Buffer {
  uint32_t id;
  VdoBuffer* vdo_buffer;
  size_t size;
};

struct Stream {
  VdoStream* vdo_stream;
  deque<Buffer> buffers;
};

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
  uint32_t SaveFrame(Stream& stream, VdoBuffer* vdoBuffer, size_t size);

  bool GetDataFromSavedFrame(Stream& stream, uint32_t frameRef,
                             GetFrameResponse* response);

  void MaybeUnrefOldestFrame(Stream& stream);

  void PrintStreamInfo(VdoStream* stream);

  Status OutputError(const char* msg, StatusCode code);
  Status OutputError(const char* msg, StatusCode code, GError* error);

  string GetTypeString(VdoFrame *frame);

  map<unsigned int, Stream> streams;
  bool verbose;
  const uint32_t MAX_NBR_SAVED_FRAMES = 3;
  pthread_mutex_t mutex;
};
}  // namespace acap_runtime

#endif
