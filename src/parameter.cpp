/* Copyright 2021 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include <algorithm>
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

    char parhand_result[BUFSIZ];

    Request request;
    while (stream->Read(&request)) {
      size_t pos = 0;
      const char *parameter_value= NULL;
      string parhand_cmd = "parhandclient get ";
      string parameter_key = request.key().c_str();
      string parhandclient_cmd = parhand_cmd + parameter_key;

      FILE *fp = popen(parhandclient_cmd.c_str(), "r"); 
      if (!fp){
        throw std::runtime_error("popen() failed!");
      }
      std::string value;
      if ( fgets( parhand_result, BUFSIZ, fp ) != NULL ) {
        value = parhand_result;
        while ((pos = value.find('"', pos)) != std::string::npos)
        value = value.erase(pos, 1);
        parameter_value = value.c_str();
      }
      if (parameter_value != nullptr){
        TRACELOG << request.key().c_str() << ": " << parameter_value << endl;
      }
      else {
        parameter_value = "";
        TRACELOG << request.key().c_str() << ": " << parameter_value << endl;
      }

      Response response;
      response.set_value(parameter_value);
      stream->Write(response);
      pclose(fp);
    }

    return Status::OK;
  }
}  // namespace acap_runtime