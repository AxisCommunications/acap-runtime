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

#include "keyvaluestore.grpc.pb.h"
#include <axsdk/axparameter.h>

#ifdef TEST
#define APP_NAME "acapruntimetest"
#else
#define APP_NAME "acapruntime"
#endif

namespace acap_runtime {

// Logic and data behind the server's behavior.
class Parameter final : public keyvaluestore::KeyValueStore::Service {
  public:
    using Request = keyvaluestore::Request;
    using Response = keyvaluestore::Response;
    using ServerContext = grpc::ServerContext;
    using Status = grpc::Status;

    Parameter(bool verbose);
    ~Parameter();

  private:
    Status GetValues(ServerContext* context, const Request* request, Response* response) override;

    AXParameter* ax_parameter;
    bool _verbose;
};
}  // namespace acap_runtime
