/* Copyright 2022 Axis Communications AB. All Rights Reserved.
==============================================================================*/

#include "video_capture.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <vdo-buffer.h>
#include <vdo-map.h>
#include <vdo-types.h>

#include <sstream>

using namespace std;

#define ERRORLOG cerr << "ERROR in VideoCapture: "
#define TRACELOG \
  if (verbose) cout << "TRACE in VideoCapture: "

namespace acap_runtime {

// Initialize Parameter Service
bool Capture::Init(const bool verbose) {
  this->verbose = verbose;
  TRACELOG << "Init" << endl;

  if (pthread_mutex_init(&mutex, NULL) != 0) {
    ERRORLOG << "Init mutex failed" << endl;
    return false;
  }

  return true;
}

Status Capture::NewStream(ServerContext *context,
                          const NewStreamRequest *request,
                          NewStreamResponse *response) {
  TRACELOG << "Creating VDO stream" << endl;

  GError *error = nullptr;
  const StreamSettings &settings = request->settings();

  VdoMap *settingsMap = vdo_map_new();
  vdo_map_set_uint32(settingsMap, "format", settings.format());
  vdo_map_set_uint32(settingsMap, "buffer.strategy",
                     VDO_BUFFER_STRATEGY_INFINITE);
  vdo_map_set_uint32(settingsMap, "width", settings.width());
  vdo_map_set_uint32(settingsMap, "height", settings.height());
  vdo_map_set_uint32(settingsMap, "framerate", settings.framerate());

  VdoStream *stream = vdo_stream_new(settingsMap, nullptr, &error);
  if (!stream) {
    return OutputError("Stream creation failed", StatusCode::INTERNAL, error);
  }
  if (verbose) {
    PrintStreamInfo(stream);
  }

  unsigned int streamId = vdo_stream_get_id(stream);
  streams.emplace(streamId, Stream{stream, deque<Buffer>{}});

  if (!vdo_stream_start(stream, &error)) {
    return OutputError("Starting stream failed", StatusCode::INTERNAL, error);
  }

  response->set_stream_id(streamId);

  return Status::OK;
}

Status Capture::DeleteStream(ServerContext *context,
                             const DeleteStreamRequest *request,
                             DeleteStreamResponse *response) {
  TRACELOG << "Deleting VDO stream: " << request->stream_id() << endl;

  auto currentStream = streams.find(request->stream_id());
  if (currentStream == streams.end()) {
    return OutputError("Deleting stream failed: stream not found",
                       StatusCode::FAILED_PRECONDITION);
  }

  VdoStream *stream = currentStream->second.vdo_stream;

  streams.erase(currentStream);
  vdo_stream_stop(stream);
  g_object_unref(stream);

  return Status::OK;
}

// Capture a frame from a stream
bool Capture::GetImgDataFromStream(unsigned int stream, void **data,
                                   size_t &size, uint32_t &frameRef) {
  GError *error = nullptr;

  TRACELOG << "Getting frame from stream " << stream << endl;

  auto currentStream = streams.find(stream);
  if (currentStream == streams.end()) {
    ERRORLOG << "Stream " << stream << " not found" << endl;
    return false;
  }

  VdoStream *vdoStream = currentStream->second.vdo_stream;
  if (verbose) {
    PrintStreamInfo(vdoStream);
  }

  MaybeUnrefOldestFrame(currentStream->second);

  VdoBuffer *buffer = vdo_stream_get_buffer(vdoStream, &error);
  if (buffer == nullptr) {
    ERRORLOG << "Unable to get VDO buffer. Stream: " << stream << endl;
    return false;
  }
  VdoFrame *frame = vdo_buffer_get_frame(buffer);
  size = vdo_frame_get_size(frame);

  void *newData = vdo_buffer_get_data(buffer);
  if (nullptr == newData) {
    if (!(vdo_stream_buffer_unref(vdoStream, &buffer, &error)))
      ERRORLOG << "Unreferencing buffer failed" << endl;
    return false;
  }

  *data = newData;

  // *data = malloc(size);
  // memcpy(*data, new_data, size);

  frameRef = SaveFrame(currentStream->second, buffer, size);

  return true;
}

// Save a frame in memory so that a client can request it later
uint32_t Capture::SaveFrame(Stream &stream, VdoBuffer *vdoBuffer, size_t size) {
  pthread_mutex_lock(&mutex);

  // Increment frame reference by 1 or start at 1
  uint32_t frameRef = stream.buffers.empty() ? 1 : stream.buffers.back().id + 1;

  // Add to saved buffers
  stream.buffers.push_back(Buffer{frameRef, vdoBuffer, size});

  TRACELOG << "Queue size: " << stream.buffers.size() << endl;
  TRACELOG << "Last frame reference: " << stream.buffers.back().id << endl;

  pthread_mutex_unlock(&mutex);

  return frameRef;
}

// Remove the oldest frame if the queue is getting full
void Capture::MaybeUnrefOldestFrame(Stream &stream) {
  pthread_mutex_lock(&mutex);

  if (stream.buffers.size() >= MAX_NBR_SAVED_FRAMES) {
    auto oldestBuffer = stream.buffers.front();
    TRACELOG << "Unreferencing buffer: " << oldestBuffer.id << endl;
    stream.buffers.pop_front();

    // Free the buffer
    if (!(vdo_stream_buffer_unref(stream.vdo_stream, &oldestBuffer.vdo_buffer,
                                  NULL))) {
      ERRORLOG << "Unreferencing buffer failed" << endl;
    }
  }

  pthread_mutex_unlock(&mutex);
}

// Find a frame based on the frame reference and put its data into the response
bool Capture::GetDataFromSavedFrame(Stream &stream, uint32_t frameRef,
                                    GetFrameResponse *response) {
  pthread_mutex_lock(&mutex);

  // Find the buffer
  auto buffer = find_if(stream.buffers.begin(), stream.buffers.end(),
                        [&](const Buffer &buf) { return buf.id == frameRef; });
  if (buffer == stream.buffers.end()) {
    ERRORLOG << "Frame reference " << frameRef << " not found" << endl;
    return false;
  }

  TRACELOG << "Found saved VDO buffer. ID: " << buffer->id << endl;

  auto data = vdo_buffer_get_data(buffer->vdo_buffer);
  if (nullptr == data) {
    ERRORLOG << "Getting data from saved buffer failed" << endl;
    return false;
  }

  response->set_data(data, buffer->size);
  response->set_size(buffer->size);

  pthread_mutex_unlock(&mutex);

  return true;
}

Status Capture::GetFrame(ServerContext *context, const GetFrameRequest *request,
                         GetFrameResponse *response) {
  GError *error = nullptr;

  auto currentStream = streams.find(request->stream_id());
  if (currentStream == streams.end()) {
    return OutputError("Getting frame failed. Stream not found",
                       StatusCode::FAILED_PRECONDITION);
  }
  VdoStream *stream = currentStream->second.vdo_stream;
  if (verbose) {
    PrintStreamInfo(stream);
  }

  uint32_t frameRef = request->frame_reference();
  if (frameRef > 0) {
    if (!GetDataFromSavedFrame(currentStream->second, frameRef, response)) {
      return OutputError("Getting frame from previous inference call failed",
                         StatusCode::NOT_FOUND);
    } else {
      TRACELOG << "Getting frame " << frameRef
               << " from previous inference call" << endl;
      return Status::OK;
    }
  }

  VdoBuffer *buffer = vdo_stream_get_buffer(stream, &error);
  if (buffer == nullptr) {
    return OutputError("Unable to get VDO buffer", StatusCode::INTERNAL, error);
  }
  VdoFrame *frame = vdo_buffer_get_frame(buffer);
  gsize size = vdo_frame_get_size(frame);

  void *bufferData = vdo_buffer_get_data(buffer);
  if (nullptr == bufferData) {
    return OutputError("Get buffer failed", StatusCode::INTERNAL);
  }

  response->set_data(bufferData, size);
  response->set_timestamp(vdo_frame_get_timestamp(frame));
  response->set_custom_timestamp(vdo_frame_get_custom_timestamp(frame));
  response->set_size(size);
  response->set_type(GetTypeString(frame));
  response->set_sequence_nbr(vdo_frame_get_sequence_nbr(frame));

  if (!(vdo_stream_buffer_unref(stream, &buffer, &error))) {
    return OutputError("Unreferencing buffer failed", StatusCode::INTERNAL,
                       error);
  }

  return Status::OK;
}

void Capture::PrintStreamInfo(VdoStream *stream) {
  GError *error = nullptr;
  VdoMap *info = vdo_stream_get_info(stream, &error);

  if (!info) {
    ERRORLOG << "Getting stream info failed" << endl;
    goto end;
    // return OutputError("Getting stream info failed", StatusCode::INTERNAL,
    //                    error);
  }

  TRACELOG << "Stream info:" << vdo_map_get_uint32(info, "width", 0) << 'x'
           << vdo_map_get_uint32(info, "height", 0) << ' '
           << vdo_map_get_uint32(info, "framerate", 0) << "fps" << endl;

end:
  g_clear_object(&info);
  g_clear_error(&error);
}

Status Capture::OutputError(const char *msg, StatusCode code) {
  return OutputError(msg, code, nullptr);
}

Status Capture::OutputError(const char *msg, StatusCode code, GError *error) {
  stringstream ss;
  ss << " " << msg
     << ((nullptr != error) ? (string(" (") + error->message + ")") : "");

  ERRORLOG << ss.str() << endl;

  g_clear_error(&error);

  return Status(code, ss.str());
}

string Capture::GetTypeString(VdoFrame *frame) {
  GEnumClass *cls = reinterpret_cast<GEnumClass *>(
      g_type_class_ref(vdo_frame_type_get_type()));
  int type = vdo_frame_get_frame_type(frame);

  return cls->values[type].value_nick;
}

}  // namespace acap_runtime
