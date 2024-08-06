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

#include "grpcpp/create_channel.h"
#include "keyvaluestore.grpc.pb.h"
#include "memory_use.h"
#include "milli_seconds.h"
#include "testdata.h"
#include "verbose_setting.h"
#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <thread>

int AcapRuntime(int argc, char* argv[]);

using namespace ::testing;
using namespace std;
using namespace grpc;
using namespace google::protobuf;
using namespace keyvaluestore;

int logTime = -1;

namespace acap_runtime {
namespace parameter_test {

void LogMemory() {
    time_t now = time(NULL);
    struct tm* nTime = localtime(&now);
    int nowTime = nTime->tm_hour;
    if (nowTime != logTime) {
        int mem = memory_use();
        printf("%d/%d %02d:%02d %d kB\n",
               nTime->tm_mday,
               nTime->tm_mon,
               nTime->tm_hour,
               nTime->tm_min,
               mem);
        logTime = nowTime;
    }
}

void Service(int seconds) {
    char timeout[10];
    sprintf(timeout, "%d", seconds);
    const bool verbose = get_verbose_status();
    char const* argv[] = {"acapruntime", verbose ? "-v" : "", "-p", target_port, "-t", timeout};
    const int argc = sizeof(argv) / sizeof(const char*);
    ASSERT_EQ(0, AcapRuntime(argc, (char**)argv));
}

// Requests each key in the vector and displays the key and its corresponding
// value as a pair
vector<pair<string, string>> GetValues(unique_ptr<KeyValueStore::Stub>& stub,
                                       const vector<string>& keys) {
    vector<pair<string, string>> values;
    ClientContext context;
    Request request;
    Response response;

    for (const auto& key : keys) {
        // Key we are sending to the server.
        request.set_key(key);
        auto status = stub->GetValues(&context, request, &response);
        if (!status.ok()) {
            throw std::runtime_error("Error from gRPC: " + status.error_message());
        }
        EXPECT_TRUE(status.ok());

        // Get the value for the sent key
        std::string value;
        value = response.value();
        value.erase(std::remove(value.begin(), value.end(), '\n'), value.end());
        values.push_back(make_pair(key, value));
    }

    return values;
}

TEST(ParameterTest, GetValues) {
    const bool verbose = get_verbose_status();
    vector<string> keys = {"root.Brand.Brand",
                           "root.Brand.WebURL",
                           "root.Image.I0.Enabled",
                           "root.invalid"};

    thread main(Service, 5);

    shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<KeyValueStore::Stub> stub = KeyValueStore::NewStub(channel);

    vector<pair<string, string>> values = GetValues(stub, keys);
    if (verbose) {
        for (pair<string, string> value : values) {
            cout << value.first << " : " << value.second << endl;
        }
    }

    EXPECT_EQ(4, values.size());
    EXPECT_STREQ("AXIS", values[0].second.c_str());
    EXPECT_STREQ("http://www.axis.com", values[1].second.c_str());
    EXPECT_STREQ("yes", values[2].second.c_str());
    EXPECT_STREQ("", values[3].second.c_str());

    main.join();
}

}  // namespace parameter_test
}  // namespace acap_runtime
