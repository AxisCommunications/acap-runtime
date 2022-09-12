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
  if (_verbose) cout << "TRACE in VideoCapture: "

namespace acap_runtime {

// Initialize Parameter Service
bool Capture::Init(const bool verbose) {
  _verbose = verbose;
  TRACELOG << "Init" << endl;
  return true;
}

Status Capture::NewStream(ServerContext *context,
                          const NewStreamRequest *request,
                          NewStreamResponse *response) {
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

  VdoStream *stream = vdo_stream_new(settings_map, nullptr, &error);
  if (!stream) {
    return OutputError("Stream creation failed", StatusCode::INTERNAL, error);
  }

  VdoMap *info = vdo_stream_get_info(stream, &error);
  if (!info) {
    g_clear_object(&stream);
    return OutputError("Getting stream info failed", StatusCode::INTERNAL,
                       error);
  }

  TRACELOG << "Starting stream:" << vdo_map_get_uint32(info, "width", 0) << 'x'
           << vdo_map_get_uint32(info, "height", 0) << ' '
           << vdo_map_get_uint32(info, "framerate", 0) << "fps" << endl;

  g_clear_object(&info);

  uint stream_id = vdo_stream_get_id(stream);
  streams.emplace(stream_id, stream);

  if (!vdo_stream_start(stream, &error)) {
    return OutputError("Starting stream failed", StatusCode::INTERNAL);
  }

  response->set_stream_id(stream_id);
  return Status::OK;
}

bool Capture::GetImgDataFromStream(unsigned int stream, void **data,
                                   size_t &size, void** buffer_obj, void** stream_obj) {
  GError *error = nullptr;

  auto currentStream = streams.find(stream);
  if (currentStream == streams.end()) {
    return false;
  }
  VdoStream *vdo_stream = currentStream->second;

  VdoMap *info = vdo_stream_get_info(vdo_stream, &error);
  if (!info) {
    ERRORLOG << "Getting stream info failed" << endl;
    return false;
  }

  TRACELOG << "Stream info:" << vdo_map_get_uint32(info, "width", 0) << 'x'
           << vdo_map_get_uint32(info, "height", 0) << ' '
           << vdo_map_get_uint32(info, "framerate", 0) << "fps" << endl;

  g_clear_object(&info);

  VdoBuffer *buffer = vdo_stream_get_buffer(vdo_stream, &error);
  if (buffer == nullptr) {
    ERRORLOG << "Unable to get VDO buffer" << endl;
    return false;
  }
  VdoFrame *frame = vdo_buffer_get_frame(buffer);
  size = vdo_frame_get_size(frame);

  void *new_data = vdo_buffer_get_data(buffer);
  if (nullptr == new_data) {
    if (!(vdo_stream_buffer_unref(vdo_stream, &buffer, &error)))
      ERRORLOG << "Unreferencing buffer failed" << endl;
    return false;
  }

  // *data = new_data;

  *data = malloc(size);
  memcpy(*data, new_data, size);

  // Seems to work even if we use the data after calling unref here...
  if (!(vdo_stream_buffer_unref(vdo_stream, &buffer, &error))) {
    ERRORLOG << "Unreferencing buffer failed" << endl;
    return false;
  }

  // *stream_obj = vdo_stream;
  // *buffer_obj = buffer;

  lastData = *data;
  lastDataSize = size;
  
  return true;
}

// bool Capture::FreeBufferObj(void *stream, void *buffer_obj) {
//   return vdo_stream_buffer_unref((VdoStream *)stream, (VdoBuffer
//   **)&buffer_obj,
//                                  nullptr);
// }

// TODO: finish this
bool Capture::SetResponseFromLastFrame(const uint stream, GetFrameResponse *response) {

  if (lastDataSize == 0 || lastData == nullptr)
    return false;

  response->set_size(lastDataSize);
  response->set_data(lastData, lastDataSize);

  return true;
}

Status Capture::GetFrame(ServerContext *context, const GetFrameRequest *request,
                         GetFrameResponse *response) {
  (void)context;
  GError *error = nullptr;

  auto currentStream = streams.find(request->stream_id());
  if (currentStream == streams.end()) {
    return OutputError("Stream not found", StatusCode::FAILED_PRECONDITION);
  }
  VdoStream *stream = currentStream->second;

  if (request->get_from_last_inference()) {
    SetResponseFromLastFrame(currentStream->first, response);
    TRACELOG << "Getting frame from last inference call" << endl;
    return Status::OK;
  }

  VdoMap *info = vdo_stream_get_info(stream, &error);
  if (!info) {
    return OutputError("Getting stream info failed", StatusCode::INTERNAL,
                       error);
  }

  TRACELOG << "Stream info:" << vdo_map_get_uint32(info, "width", 0) << 'x'
           << vdo_map_get_uint32(info, "height", 0) << ' '
           << vdo_map_get_uint32(info, "framerate", 0) << "fps" << endl;

  g_clear_object(&info);

  VdoBuffer *buffer = vdo_stream_get_buffer(stream, &error);
  if (buffer == nullptr) {
    return OutputError("Unable to get VDO buffer", StatusCode::INTERNAL, error);
  }
  VdoFrame *frame = vdo_buffer_get_frame(buffer);
  gsize size = vdo_frame_get_size(frame);

  stringstream ss;
  ss << "/s" << vdo_stream_get_id(stream) << '-'
     << vdo_frame_get_timestamp(frame);

  // TRACELOG << "file name: " << ss.str().c_str() << endl;

  // int fd = shm_open(ss.str().c_str(), O_CREAT | O_RDWR, S_IWUSR);
  // if (fd == -1) {
  //   return OutputError("Unable to to open shared memory file",
  //   StatusCode::INTERNAL, error);
  // }
  // // TODO Don't ignore the return value
  // (void)(ftruncate(fd, size) + 1);

  // void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  // if (addr == MAP_FAILED) {
  //   return OutputError("Failed to create memory mapping",
  //   StatusCode::INTERNAL);
  // }

  void *buffer_data = vdo_buffer_get_data(buffer);
  if (nullptr == buffer_data) {
    return OutputError("Get buffer failed", StatusCode::INTERNAL);
  }

  // memcpy(addr, buffer_data, size);

  response->set_data(buffer_data, size);
  // response->set_file_name(ss.str());

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

Status Capture::OutputError(const char *msg, StatusCode code) {
  return OutputError(msg, code, nullptr);
}

Status Capture::OutputError(const char *msg, StatusCode code, GError *error) {
  stringstream ss;
  ss << " ";
  ss << msg
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