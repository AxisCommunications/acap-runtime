/**
 * Copyright (C) 2022 Axis Communications AB, Lund, Sweden
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

#include <vdo-buffer.h>
#include <vdo-stream.h>

#include <deque>
#include <map>

#include "videocapture.grpc.pb.h"

namespace acap_runtime {

struct Buffer {
    uint32_t id;
    VdoBuffer* vdo_buffer;
    size_t size;
};

struct Stream {
    VdoStream* vdo_stream;
    std::deque<Buffer> buffers;
};

class Capture final : public videocapture::v1::VideoCapture::Service {
  public:
    using DeleteStreamRequest = videocapture::v1::DeleteStreamRequest;
    using DeleteStreamResponse = videocapture::v1::DeleteStreamResponse;
    using GetFrameRequest = videocapture::v1::GetFrameRequest;
    using GetFrameResponse = videocapture::v1::GetFrameResponse;
    using NewStreamRequest = videocapture::v1::NewStreamRequest;
    using NewStreamResponse = videocapture::v1::NewStreamResponse;
    using ServerContext = grpc::ServerContext;
    using Status = grpc::Status;
    using StatusCode = grpc::StatusCode;

    bool Init(const bool verbose);

    Status NewStream(ServerContext* context,
                     const NewStreamRequest* request,
                     NewStreamResponse* response) override;

    Status DeleteStream(ServerContext* context,
                        const DeleteStreamRequest* request,
                        DeleteStreamResponse* response) override;

    Status GetFrame(ServerContext* context,
                    const GetFrameRequest* request,
                    GetFrameResponse* response) override;

    bool GetImgDataFromStream(unsigned int stream, void** data, size_t& size, uint32_t& frameRef);

  private:
    uint32_t SaveFrame(Stream& stream, VdoBuffer* vdoBuffer, size_t size);

    bool GetDataFromSavedFrame(Stream& stream, uint32_t frameRef, GetFrameResponse* response);

    void MaybeDeleteOldestFrame(Stream& stream);

    void PrintStreamInfo(VdoStream* stream);

    Status OutputError(const char* msg, StatusCode code);
    Status OutputError(const char* msg, StatusCode code, GError* error);

    std::string GetTypeString(VdoFrame* frame);

    std::map<unsigned int, Stream> _streams;
    bool _verbose;
    const uint32_t MAX_NBR_SAVED_FRAMES = 3;
    pthread_mutex_t _mutex;
};
}  // namespace acap_runtime

#endif
