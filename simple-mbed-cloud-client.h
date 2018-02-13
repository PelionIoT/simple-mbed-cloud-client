// ----------------------------------------------------------------------------
// Copyright 2016-2017 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------


#ifndef SIMPLEMBEDCLOUDCLIENT_H
#define SIMPLEMBEDCLOUDCLIENT_H
#include <stdio.h>
#include "mbed-cloud-client/MbedCloudClient.h"
#include "m2mdevice.h"
#include "m2mresource.h"
#include "mbed-client/m2minterface.h"
#include "mbed-client/m2mvector.h"
#include "mbed_cloud_client_resource.h"

class MbedCloudClientResource;

class SimpleMbedCloudClient {

public:

    SimpleMbedCloudClient();
    ~SimpleMbedCloudClient();

    bool call_register();
    void close();
    void register_update();
    void client_registered();
    void client_unregistered();
    void error(int error_code);
    bool is_client_registered();
    bool is_register_called();
    void register_and_connect();
    MbedCloudClient& get_cloud_client();
    MbedCloudClientResource* create_resource(const char *path, const char *name);
    M2MResource* add_cloud_resource(uint16_t object_id, uint16_t instance_id,
                              uint16_t resource_id, const char *resource_type,
                              M2MResourceInstance::ResourceType data_type,
                              M2MBase::Operation allowed, const char *value,
                              bool observable, void *cb, void *notification_status_cb);

private:
    M2MObjectList       _obj_list;
    MbedCloudClient     _cloud_client;
    bool                _registered;
    bool                _register_called;
    Vector<MbedCloudClientResource*> _resources;
};

#endif // SIMPLEMBEDCLOUDCLIENT_H
