/* Copyright 2021 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include <axsdk/ax_parameter.h>
#include "parameter.h"

using namespace std;

#define ERRORLOG cerr << "ERROR in Parameter: "
#define TRACELOG if (_verbose) cout << "TRACE in Parameter: "

namespace acap_runtime {

// Initialize Parameter Service
bool Parameter::Init(const bool verbose) {
  _verbose = verbose;
  TRACELOG << "Init" << endl;
  return true;
}

// Logic and data behind the server's behavior.
Status Parameter::GetValues(ServerContext* context,
    ServerReaderWriter<Response, Request>* stream) {
    GError *error = NULL;

    AXParameter *ax_parameter = ax_parameter_new(APP_NAME, &error);
    if (ax_parameter == NULL) {
      ERRORLOG << "Error when creating axparameter: " << error->message << endl;
      g_clear_error(&error);
      return Status::CANCELLED;
    }

    Request request;
    while (stream->Read(&request)) {
      char *parameter_value = NULL;
      if (!ax_parameter_get(ax_parameter, request.key().c_str(), &parameter_value, &error)) {
        parameter_value =  g_strdup("");
      }

      TRACELOG << request.key().c_str() << ": " << parameter_value << endl;
      Response response;
      response.set_value(parameter_value);
      stream->Write(response);
      free(parameter_value);
    }

    ax_parameter_free(ax_parameter);
    g_clear_error(&error);
    return Status::OK;
}
}  // namespace acap_runtime