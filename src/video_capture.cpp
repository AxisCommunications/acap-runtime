/* Copyright 2022 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include "video_capture.h"

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
}  // namespace acap_runtime
