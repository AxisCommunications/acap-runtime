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
#define TRACELOG  \
    if (_verbose) \
    cout << "TRACE in VideoCapture: "

namespace acap_runtime {

// Initialize the capture service
bool Capture::Init(const bool verbose) {
    this->_verbose = verbose;
    TRACELOG << "Init" << endl;

    if (pthread_mutex_init(&_mutex, NULL) != 0) {
        ERRORLOG << "Init mutex failed" << endl;
        return false;
    }

    return true;
}

// Create a new stream
Status Capture::NewStream(ServerContext* context,
                          const NewStreamRequest* request,
                          NewStreamResponse* response) {
    TRACELOG << "Creating VDO stream" << endl;

    GError* error = nullptr;
    const StreamSettings& settings = request->settings();

    VdoMap* settingsMap = vdo_map_new();
    vdo_map_set_uint32(settingsMap, "format", settings.format());
    vdo_map_set_uint32(settingsMap, "buffer.strategy", VDO_BUFFER_STRATEGY_INFINITE);
    vdo_map_set_uint32(settingsMap, "width", settings.width());
    vdo_map_set_uint32(settingsMap, "height", settings.height());
    vdo_map_set_uint32(settingsMap, "framerate", settings.framerate());

    VdoStream* stream = vdo_stream_new(settingsMap, nullptr, &error);
    if (!stream) {
        return OutputError("Stream creation failed", StatusCode::INTERNAL, error);
    }
    if (_verbose) {
        PrintStreamInfo(stream);
    }

    unsigned int streamId = vdo_stream_get_id(stream);
    _streams.emplace(streamId, Stream{stream, deque<Buffer>{}});

    if (!vdo_stream_start(stream, &error)) {
        return OutputError("Starting stream failed", StatusCode::INTERNAL, error);
    }

    response->set_stream_id(streamId);

    return Status::OK;
}

// Delete a stream
Status Capture::DeleteStream(ServerContext* context,
                             const DeleteStreamRequest* request,
                             DeleteStreamResponse* response) {
    TRACELOG << "Deleting VDO stream: " << request->stream_id() << endl;

    auto currentStream = _streams.find(request->stream_id());
    if (currentStream == _streams.end()) {
        return OutputError("Deleting stream failed: stream not found",
                           StatusCode::FAILED_PRECONDITION);
    }

    VdoStream* stream = currentStream->second.vdo_stream;

    _streams.erase(currentStream);
    vdo_stream_stop(stream);
    g_object_unref(stream);

    return Status::OK;
}

// Capture a frame from a specific stream, possibly from a previously saved
// frame
Status Capture::GetFrame(ServerContext* context,
                         const GetFrameRequest* request,
                         GetFrameResponse* response) {
    GError* error = nullptr;

    TRACELOG << "Getting frame from stream " << request->stream_id() << endl;

    auto currentStream = _streams.find(request->stream_id());
    if (currentStream == _streams.end()) {
        return OutputError("Getting frame failed. Stream not found",
                           StatusCode::FAILED_PRECONDITION);
    }
    VdoStream* stream = currentStream->second.vdo_stream;
    if (_verbose) {
        PrintStreamInfo(stream);
    }

    uint32_t frameRef = request->frame_reference();
    if (frameRef > 0) {
        if (!GetDataFromSavedFrame(currentStream->second, frameRef, response)) {
            return OutputError("Getting frame from previous inference call failed",
                               StatusCode::NOT_FOUND);
        } else {
            TRACELOG << "Getting frame " << frameRef << " from previous inference call" << endl;
            return Status::OK;
        }
    }

    VdoBuffer* buffer = vdo_stream_get_buffer(stream, &error);
    if (buffer == nullptr) {
        return OutputError("Unable to get VDO buffer", StatusCode::INTERNAL, error);
    }
    VdoFrame* frame = vdo_buffer_get_frame(buffer);
    gsize size = vdo_frame_get_size(frame);

    void* bufferData = vdo_buffer_get_data(buffer);
    if (nullptr == bufferData) {
        if (!(vdo_stream_buffer_unref(stream, &buffer, &error)))
            ERRORLOG << "Unreferencing buffer failed" << endl;
        return OutputError("Getting buffer failed", StatusCode::INTERNAL);
    }

    response->set_data(bufferData, size);
    response->set_timestamp(vdo_frame_get_timestamp(frame));
    response->set_custom_timestamp(vdo_frame_get_custom_timestamp(frame));
    response->set_size(size);
    response->set_type(GetTypeString(frame));
    response->set_sequence_nbr(vdo_frame_get_sequence_nbr(frame));

    if (!(vdo_stream_buffer_unref(stream, &buffer, &error))) {
        return OutputError("Unreferencing buffer failed", StatusCode::INTERNAL, error);
    }

    return Status::OK;
}

// Capture a frame from a specific stream
bool Capture::GetImgDataFromStream(unsigned int stream,
                                   void** data,
                                   size_t& size,
                                   uint32_t& frameRef) {
    GError* error = nullptr;

    TRACELOG << "Getting frame from stream " << stream << endl;

    auto currentStream = _streams.find(stream);
    if (currentStream == _streams.end()) {
        ERRORLOG << "Stream " << stream << " not found" << endl;
        return false;
    }

    VdoStream* vdoStream = currentStream->second.vdo_stream;
    if (_verbose) {
        PrintStreamInfo(vdoStream);
    }

    pthread_mutex_lock(&_mutex);

    MaybeDeleteOldestFrame(currentStream->second);

    VdoBuffer* buffer = vdo_stream_get_buffer(vdoStream, &error);
    if (buffer == nullptr) {
        ERRORLOG << "Unable to get VDO buffer. Stream: " << stream << endl;
        return false;
    }
    VdoFrame* frame = vdo_buffer_get_frame(buffer);
    size = vdo_frame_get_size(frame);

    void* bufferData = vdo_buffer_get_data(buffer);
    if (nullptr == bufferData) {
        if (!(vdo_stream_buffer_unref(vdoStream, &buffer, &error)))
            ERRORLOG << "Unreferencing buffer failed" << endl;
        return false;
    }

    *data = bufferData;

    frameRef = SaveFrame(currentStream->second, buffer, size);

    pthread_mutex_unlock(&_mutex);

    return true;
}

// Save a frame in memory so that a client can request it later
uint32_t Capture::SaveFrame(Stream& stream, VdoBuffer* vdoBuffer, size_t size) {
    // Increment frame reference by 1 or start at 1
    uint32_t frameRef = stream.buffers.empty() ? 1 : stream.buffers.back().id + 1;

    // Add to saved buffers
    stream.buffers.push_back(Buffer{frameRef, vdoBuffer, size});

    TRACELOG << "Queue size: " << stream.buffers.size() << endl;
    TRACELOG << "Last frame reference: " << stream.buffers.back().id << endl;

    return frameRef;
}

// Delete the oldest frame if the queue is getting full
void Capture::MaybeDeleteOldestFrame(Stream& stream) {
    if (stream.buffers.size() >= MAX_NBR_SAVED_FRAMES) {
        auto oldestBuffer = stream.buffers.front();
        TRACELOG << "Unreferencing buffer: " << oldestBuffer.id << endl;
        stream.buffers.pop_front();

        // Free the buffer
        if (!(vdo_stream_buffer_unref(stream.vdo_stream, &oldestBuffer.vdo_buffer, NULL))) {
            ERRORLOG << "Unreferencing buffer failed" << endl;
        }
    }
}

// Find a frame based on the frame reference and put its data into the response
bool Capture::GetDataFromSavedFrame(Stream& stream, uint32_t frameRef, GetFrameResponse* response) {
    pthread_mutex_lock(&_mutex);
    bool ret = false;
    gpointer data = NULL;

    // Find the buffer
    auto buffer = find_if(stream.buffers.begin(), stream.buffers.end(), [&](const Buffer& buf) {
        return buf.id == frameRef;
    });
    if (buffer == stream.buffers.end()) {
        ERRORLOG << "Frame reference " << frameRef << " not found" << endl;
        goto end;
    }

    TRACELOG << "Found saved VDO buffer. ID: " << buffer->id << endl;

    data = vdo_buffer_get_data(buffer->vdo_buffer);
    if (nullptr == data) {
        ERRORLOG << "Getting data from saved buffer failed" << endl;
        goto end;
    }

    response->set_data(data, buffer->size);
    response->set_size(buffer->size);

    ret = true;

end:
    pthread_mutex_unlock(&_mutex);
    return ret;
}

// Print dimensions and fps of the stream
void Capture::PrintStreamInfo(VdoStream* stream) {
    GError* error = nullptr;
    VdoMap* info = vdo_stream_get_info(stream, &error);

    if (!info) {
        ERRORLOG << "Getting stream info failed: " << error->message << endl;
        goto end;
    }

    TRACELOG << "Stream info: " << vdo_map_get_uint32(info, "width", 0) << 'x'
             << vdo_map_get_uint32(info, "height", 0) << ' '
             << vdo_map_get_uint32(info, "framerate", 0) << "fps" << endl;

end:
    g_clear_object(&info);
    g_clear_error(&error);
}

// Return a Status and write the error message to ERRORLOG
Status Capture::OutputError(const char* msg, StatusCode code) {
    return OutputError(msg, code, nullptr);
}

// Return a Status and write the error message to ERRORLOG
Status Capture::OutputError(const char* msg, StatusCode code, GError* error) {
    stringstream ss;
    ss << " " << msg << ((nullptr != error) ? (string(" (") + error->message + ")") : "");

    ERRORLOG << ss.str() << endl;

    g_clear_error(&error);

    return Status(code, ss.str());
}

// Get the image format of the frame
string Capture::GetTypeString(VdoFrame* frame) {
    GEnumClass* cls = reinterpret_cast<GEnumClass*>(g_type_class_ref(vdo_frame_type_get_type()));
    int type = vdo_frame_get_frame_type(frame);

    return cls->values[type].value_nick;
}

}  // namespace acap_runtime
