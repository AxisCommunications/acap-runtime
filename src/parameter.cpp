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
      const char *parameter_value= NULL;
      const char *parhand_cmd = "parhandclient get ";
      const char *parameter_key = request.key().c_str();
      char *str = new char[strlen(parhand_cmd) + strlen(parameter_key)+ 1];
      strcpy(str, parhand_cmd);
      strcat(str, parameter_key);

      FILE *fp = popen(str, "r"); 
      std::string value;
      while ( fgets( parhand_result, BUFSIZ, fp ) != NULL ) {
        value = parhand_result;
        std::remove(value.begin(), value.end(), '\"');
        parameter_value = value.c_str();
      }
      if (parameter_value != nullptr){
        TRACELOG << request.key().c_str() << ": " << parameter_value << endl;
      }
      else {
        parameter_value = "";
        TRACELOG << request.key().c_str() << ":: " << parameter_value << endl;
      }

      Response response;
      response.set_value(parameter_value);
      stream->Write(response);
      pclose(fp);
    }

    return Status::OK;
  }
}  // namespace acap_runtime