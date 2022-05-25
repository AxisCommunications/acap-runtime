/* Copyright 2022 Axis Communications AB. All Rights Reserved.
==============================================================================*/

#include "video_capture.h"

#include <vdo-buffer.h>
#include <vdo-map.h>
#include <vdo-types.h>

#include <sstream>

using namespace std;

#define ERRORLOG cerr << "ERROR in VideoCapture: "
#define TRACELOG \
  if (_verbose) cout << "TRACE in VideoCapture: "

namespace acap_runtime {

// Initialize Parameter Service
bool Capture::Init(const bool verbose) {
  _verbose = verbose;
  TRACELOG << "Init" << endl;
  return true;
}

// Logic and data behind the server's behavior.
Status Capture::GetValues(ServerContext *context,
                          ServerReaderWriter<Response, Request> *stream) {
  Request request;
  while (stream->Read(&request)) {
    const char *reply = "Hello World!";

    TRACELOG << request.key().c_str() << ": " << reply << endl;
    Response response;
    response.set_value(reply);
    stream->Write(response);
  }

  return Status::OK;
}

Status Capture::VdoStreamNew(ServerContext *context,
                             const VdoStreamNewRequest *request,
                             VdoStreamNewResponse *response) {
  TRACELOG << "Creating VDO stream" << endl;

  GError *error = nullptr;

  const StreamSettings &settings = request->settings();

  VdoMap *settings_map = vdo_map_new();

  vdo_map_set_uint32(settings_map, "format", settings.format());
  vdo_map_set_uint32(settings_map, "buffer.strategy",
                     VDO_BUFFER_STRATEGY_INFINITE);
  vdo_map_set_uint32(settings_map, "width", settings.width());
  vdo_map_set_uint32(settings_map, "height", settings.height());
  vdo_map_set_uint32(settings_map, "framerate", settings.framerate());
  vdo_map_set_uint16(settings_map, "timestamp.type", settings.timestamp_type());

  VdoStream *stream = vdo_stream_new(settings_map, nullptr, &error);
  if (!stream) {
    ERRORLOG << "Stream creation failed";
    return Status::CANCELLED;
  }

  VdoMap *info = vdo_stream_get_info(stream, &error);
  if (!info) {
    ERRORLOG << "Getting stream info failed";
    g_clear_object(&stream);
    return Status::CANCELLED;
  }

  TRACELOG << "Starting stream:" << vdo_map_get_uint32(info, "width", 0) << 'x'
           << vdo_map_get_uint32(info, "height", 0) << ' '
           << vdo_map_get_uint32(info, "framerate", 0) << "fps" << endl;

  g_clear_object(&info);

  uint stream_id = vdo_stream_get_id(stream);
  streams.emplace(stream_id, stream);

  if (!vdo_stream_start(stream, &error)) {
    ERRORLOG << "Starting stream failed";
    return Status::CANCELLED;
  }

  response->set_stream_id(stream_id);
  return Status::OK;
}

Status Capture::VdoStreamGetFrame(ServerContext *context,
                                  const VdoStreamGetFrameRequest *request,
                                  VdoStreamGetFrameResponse *response) {
  (void)context;
  GError *error = nullptr;

  auto currentStream = streams.find(request->stream_id());
  if (currentStream == streams.end()) {
    return OutputError("Stream not found", StatusCode::FAILED_PRECONDITION);
  }
  VdoStream *stream = currentStream->second;

  VdoMap *info = vdo_stream_get_info(stream, &error);
  if (!info) {
    return OutputError("Getting stream info failed", StatusCode::INTERNAL,
                       error);
  }

  TRACELOG << "Stream info:" << vdo_map_get_uint32(info, "width", 0) << 'x'
           << vdo_map_get_uint32(info, "height", 0) << ' '
           << vdo_map_get_uint32(info, "framerate", 0) << "fps" << endl;

  g_clear_object(&info);

  // VdoBuffer *buffer = vdo_stream_get_buffer(stream, &error);
  // if (buffer == nullptr) {
  //   PrintError("Unable to get VDO buffer", error);
  //   return Status::CANCELLED;
  // }
  // VdoFrame *frame = vdo_buffer_get_frame(buffer);
  // gsize size = vdo_frame_get_size(frame);

  // stringstream ss;
  // ss << "/s" << vdo_stream_get_id(stream) << '-'
  //    << vdo_frame_get_timestamp(frame);

  // LOG(INFO) << "file name: " << ss.str().c_str() << endl;

  // int fd = shm_open(ss.str().c_str(), O_CREAT | O_RDWR, S_IWUSR);
  // if (fd == -1) {
  //   PrintError("Unable to to open shared memory object", nullptr);
  //   LOG(ERROR) << "errno=" << errno << endl;
  //   return Status::CANCELLED;
  // }
  // ftruncate(fd, size);

  // void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  // if (addr == MAP_FAILED) {
  //   PrintError("Failed to creates mapping in the virtual address space",
  //              nullptr);
  //   return Status::CANCELLED;
  // }

  // void *buffer_data = vdo_buffer_get_data(buffer);
  // if (nullptr == buffer_data) {
  //   PrintError("Get buffer failed", nullptr);
  //   return Status::CANCELLED;
  // }

  // LOG(INFO) << "After mmap, size: " << size << ", addr: " << addr
  //           << "buffer_data: " << buffer_data << endl;

  // memcpy(addr, buffer_data, size);

  // LOG(INFO) << "After memcpy" << endl;

  // vdo_stream_buffer_unref(stream, &buffer, &error);

  // LOG(INFO) << "After vdo_stream_buffer_unref" << endl;
  // response->set_file_name(ss.str());
  return Status::OK;
}

Status Capture::OutputError(const char *msg, StatusCode code) {
  return OutputError(msg, code, nullptr);
}

Status Capture::OutputError(const char *msg, StatusCode code, GError *error) {
  stringstream ss;
  ss << " ";
  ss << msg
     << ((nullptr != error) ? (string(" (") + error->message + ")") : "");

  ERRORLOG << ss.str() << endl;

  return Status(code, ss.str());
}

}  // namespace acap_runtime
