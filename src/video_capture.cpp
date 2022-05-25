/* Copyright 2022 Axis Communications AB. All Rights Reserved.
==============================================================================*/

#include "video_capture.h"

#include <vdo-buffer.h>
#include <vdo-map.h>
#include <vdo-types.h>

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
    // PrintError("Stream creation failed", error);
    return Status::CANCELLED;
  }

  VdoMap *info = vdo_stream_get_info(stream, &error);
  if (!info) {
    // PrintError("Stream info fetching failed", error);
    g_clear_object(&stream);
    return Status::CANCELLED;
  }

  // LOG(INFO) << "Starting stream:" << vdo_map_get_uint32(info, "width", 0) <<
  // 'x'
  //           << vdo_map_get_uint32(info, "height", 0) << ' '
  //           << vdo_map_get_uint32(info, "framerate", 0) << "fps" << endl;

  g_clear_object(&info);

  uint stream_id = vdo_stream_get_id(stream);
  streams.emplace(stream_id, stream);

  if (!vdo_stream_start(stream, &error)) {
    // PrintError("Starting stream failed", error);
    return Status::CANCELLED;
  }

  response->set_stream_id(stream_id);
  return Status::OK;

  // stream = hal_stream_get_compatible(settings_map);
  // if (!stream) {
  //   stream = hal_system_create_stream(hal_system, settings_map, &error);
  // }

  // if (!stream) {
  //   g_error("Stream creation failed %s", error->message);
  //   response->set_error(error->message);
  //   g_clear_error(&error);
  //   return Status::OK;
  // }

  // response->set_stream_id(hal_stream_get_id(stream));
  // return Status::OK;
}
}  // namespace acap_runtime
