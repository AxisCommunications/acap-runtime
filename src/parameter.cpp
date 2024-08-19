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

#include "parameter.h"
#include <algorithm>
#include <regex>

using namespace std;

#define ERRORLOG cerr << "ERROR in Parameter: "
#define TRACELOG  \
    if (_verbose) \
    cout << "TRACE in Parameter: "

namespace acap_runtime {

// Initialize Parameter Service
bool Parameter::Init(const bool verbose) {
    _verbose = verbose;
    TRACELOG << "Init" << endl;
    return true;
}

// Logic and data behind the server's behavior.
Status Parameter::GetValues(ServerContext* context, ServerReaderWriter<Response, Request>* stream) {
    char parhand_result[BUFSIZ];

    Request request;
    while (stream->Read(&request)) {
        size_t pos = 0;
        const char* parameter_value = NULL;
        string parhand_cmd = "parhandclient get ";
        string parameter_key = request.key().c_str();
        const regex pattern("[a-zA-Z0-9.]+");
        if (regex_match(parameter_key, pattern)) {
            string parhandclient_cmd = parhand_cmd + parameter_key;

            FILE* fp = popen(parhandclient_cmd.c_str(), "r");
            if (!fp) {
                throw std::runtime_error("popen() failed!");
            }
            std::string value;
            if (fgets(parhand_result, BUFSIZ, fp) != NULL) {
                value = parhand_result;
                while ((pos = value.find('"', pos)) != std::string::npos)
                    value = value.erase(pos, 1);
                parameter_value = value.c_str();
            }
            if (parameter_value != nullptr) {
                TRACELOG << request.key().c_str() << ": " << parameter_value << endl;
            } else {
                parameter_value = "";
                TRACELOG << request.key().c_str() << ": " << parameter_value << endl;
            }

            Response response;
            response.set_value(parameter_value);
            stream->Write(response);
            pclose(fp);
        } else {
            TRACELOG << "No valid input request" << endl;
            return Status(StatusCode::INVALID_ARGUMENT, "No valid input request");
        }
    }

    return Status::OK;
}
}  // namespace acap_runtime
