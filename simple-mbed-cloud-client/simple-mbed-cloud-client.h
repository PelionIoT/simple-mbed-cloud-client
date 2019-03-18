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
#include "mbed-cloud-client-resource.h"
#include "storage-helper/storage-helper.h"
#include "mbed.h"
#include "NetworkInterface.h"

class MbedCloudClientResource;

class SimpleMbedCloudClient {

public:

    /**
     * Initialize SimpleMbedCloudClient
     *
     * @param net A connected network interface
     * @param bd An uninitialized block device to back the file system
     * @param fs An uninitialized file system
     */
    SimpleMbedCloudClient(NetworkInterface *net, BlockDevice *bd, FileSystem *fs);

    /**
     * SimpleMbedCloudClient destructor
     *
     * This deletes all managed resources
     * This does not unregister the client
     */
    ~SimpleMbedCloudClient();

    /**
     * Initialize SimpleMbedCloudClient
     *
     * Sets up factory configurator client, loads root of trust,
     * initializes storage, and loads existing developer credentials (if present)
     *
     * @param format If set to true, will always format the file system
     *
     * @returns 0 if successful, 1 if an error occured
     */
    int init(bool format = false);

    /**
     * Close the connection to Pelion Device Management and unregister the device
     */
    void close();

    /**
    * Sends a registration update message to the Cloud when the client is registered
    * successfully to the Cloud and there is no internal connection error.
    * If the client is not connected and there is some other internal network
    * transaction ongoing, this function triggers an error MbedCloudClient::ConnectNotAllowed.
    */
    void register_update();

    /**
     * Checks registration status
     *
     * @returns true when connected, false when not connected
     */
    bool is_client_registered();

    /**
     * Whether the device has ever tried registering
     *
     * @returns true when registration was tried, false when not
     */
    bool is_register_called();

    /**
     * Register with Pelion Device Management
     * This does *not* initialize the MbedCloudClientResource added through SimpleMbedCloudClient
     * Only call this function after you unregistered first, do NOT use it to register the first time
     *
     * This function returns immediately, observe on_registered and on_error for updates.
     *
     * @returns true if connection started, false if connection was not started
     */
    bool call_register();

    /**
     * Create MbedCloudClientResource on the underlying Mbed Cloud Client object,
     * register with Pelion Device Management.
     *
     * This function can only be called once. If you need to re-register,
     * use `call_register`.
     *
     * This function returns immediately, observe the on_registered and
     * on_error callbacks for updates.
     *
     * @returns true if registration was started, false if registration was not started
     */
    bool register_and_connect();

    /**
     * Get the underlying Mbed Cloud Client
     */
    MbedCloudClient *get_cloud_client();

    /**
     * Create a new resource
     *
     * This does not add the resource to Mbed Cloud Client until `register_and_connect`
     * is called.
     * Resources need to be added before first registration.
     *
     * @param path LwM2M path (in the form of 3200/0/5501)
     * @param name Name of the resource (will be shown in the UI)
     *
     * @returns new instance of MbedCloudClientResource
     */
    MbedCloudClientResource* create_resource(const char *path, const char *name);

    /**
     * Sets the on_registered callback
     * This callback is fired when the device is registered with Pelion Device Management
     *
     * @param cb Callback
     */
    void on_registered(Callback<void(const ConnectorClientEndpointInfo*)> cb);

    /**
     * Sets the on_unregistered callback
     * This callback is fired when the device is unregistered with Pelion Device Management
     *
     * @param cb Callback
     */
    void on_unregistered(Callback<void()> cb);

    /**
     * Sets the update authorization callback
     * This will overwrite the default authorization callback (and thus the logging)
     *
     * @todo, replace this with Callback<>
     *
     * @param cb Callback
     */
    void on_update_authorized(void (*cb)(int32_t request));

    /**
     * Sets the update authorization callback
     * This will overwrite the default authorization callback (and thus the logging)
     *
     * @todo, replace this with Callback<>
     *
     * @param cb Callback
     */
    void on_update_progress(void (*cb)(uint32_t progress, uint32_t total));

    /**
     * Sets the error callback
     * This will overwrite the default error logging
     *
     * @param cb Callback
     */
    void on_error_cb(Callback<void(int, const char*)> cb);

    /**
     * Format the underlying storage
     *
     * returns 0 if successful, non-0 if failed
     */
    int reformat_storage();

private:

    /**
     * Callback from Mbed Cloud Client, fires when device is registered
     */
    void client_registered();

    /**
     * Callback from Mbed Cloud Client, fires when device is unregistered
     */
    void client_unregistered();

    /**
     * Callback from Mbed Cloud Client, fires when error has occured
     */
    void error(int error_code);

    /**
     * Re-mount and re-format the storage layer
     *
     * @returns 0 if successful, non-0 if not successful
     */
    int reset_storage();

    /**
     * Retrigger the developer or FCC flow
     *
     * @param format If set to true, will first format the storage layer
     *
     * @returns 0 if successful, non-0 if not succesful
     */
    int verify_cloud_configuration(bool format);

    M2MObjectList                                       _obj_list;
    MbedCloudClient                                     _cloud_client;
    bool                                                _registered;
    bool                                                _register_called;
    bool                                                _register_and_connect_called;
    Vector<MbedCloudClientResource*>                    _resources;
    Callback<void(const ConnectorClientEndpointInfo*)>  _registered_cb;
    Callback<void()>                                    _unregistered_cb;
    Callback<void(int, const char*)>                    _error_cb;
    NetworkInterface *                                  _net;
    BlockDevice *                                       _bd;
    FileSystem *                                        _fs;
    StorageHelper                                       _storage;
};

#endif // SIMPLEMBEDCLOUDCLIENT_H
