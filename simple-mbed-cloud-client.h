// ----------------------------------------------------------------------------
// Copyright 2016-2018 ARM Ltd.
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
#include "mbed.h"
#include "NetworkInterface.h"

class MbedCloudClientResource;

class SimpleMbedCloudClient {

public:

    SimpleMbedCloudClient(NetworkInterface *net, BlockDevice *bd, FileSystem *fs);
    ~SimpleMbedCloudClient();

    int init();
    bool call_register();
    void close();
    void register_update();
    void client_registered();
    void client_unregistered();
    void error(int error_code);
    bool is_client_registered();
    bool is_register_called();
    bool register_and_connect();
    MbedCloudClient& get_cloud_client();
    MbedCloudClientResource* create_resource(const char *path, const char *name);
    void on_registered(Callback<void(const ConnectorClientEndpointInfo*)> cb);
    void on_unregistered(Callback<void()> cb);
    int reformat_storage();

private:
    void reset_storage();
    int mount_storage();

    M2MObjectList                                       _obj_list;
    MbedCloudClient                                     _cloud_client;
    bool                                                _registered;
    bool                                                _register_called;
    bool                                                _register_and_connect_called;
    Vector<MbedCloudClientResource*>                    _resources;
    Callback<void(const ConnectorClientEndpointInfo*)>  _registered_cb;
    Callback<void()>                                    _unregistered_cb;
    NetworkInterface *                                  _net;
    BlockDevice *                                       _bd;
    FileSystem *                                        _fs;
};

#endif // SIMPLEMBEDCLOUDCLIENT_H
