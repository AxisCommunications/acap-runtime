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

#include <string.h>
#include <stdexcept>
#include "util.h"

using namespace std;

const char *get_parameter_value(string parameter_name, string app_name)
{
    size_t pos = 0;
    char parhand_result[BUFSIZ];
    const char *parameter_value= NULL;
    string parhand_cmd = "parhandclient get root." + app_name + ".";
    string parhandclient_cmd = parhand_cmd + parameter_name;

    FILE *fp = popen(parhandclient_cmd.c_str(), "r");
    if (!fp) {
        throw runtime_error("popen() failed!");
    }

    string value;
    if ( fgets( parhand_result, BUFSIZ, fp ) != NULL ) {
        // remove trailing newline, or else strcmp of the return value will always fail.
        parhand_result[strcspn(parhand_result, "\r\n")] = 0;
        value.assign(parhand_result);
        while ((pos = value.find('"', pos)) != string::npos) {
            value = value.erase(pos, 1);
        }
        parameter_value = value.c_str();
    }
    if (parameter_value == nullptr) {
        parameter_value = "";
    }
    pclose(fp);
    
    return parameter_value;
}

