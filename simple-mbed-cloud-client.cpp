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

#include <stdio.h>
#include "simple-mbed-cloud-client.h"
#include "mbed-cloud-client/MbedCloudClient.h"
#include "m2mdevice.h"
#include "m2mresource.h"
#include "mbed-client/m2minterface.h"
#include "key_config_manager.h"
#include "resource.h"
#include "mbed-client/m2mvector.h"
#include "mbed_cloud_client_resource.h"
#include "factory_configurator_client.h"

#ifdef MBED_CLOUD_CLIENT_USER_CONFIG_FILE
#include MBED_CLOUD_CLIENT_USER_CONFIG_FILE
#endif

#ifdef MBED_CLOUD_CLIENT_SUPPORT_UPDATE
#include "update_ui_example.h"
#endif

#ifdef MBED_HEAP_STATS_ENABLED
#include "memory_tests.h"
#endif

SimpleMbedCloudClient::SimpleMbedCloudClient(NetworkInterface *net) :
    _registered(false),
    _register_called(false),
    net(net) {
}

SimpleMbedCloudClient::~SimpleMbedCloudClient() {
    for (unsigned int i = 0; _resources.size(); i++) {
        delete _resources[i];
    }
}

int SimpleMbedCloudClient::init() {
    // Initialize the FCC
    fcc_status_e fcc_status = fcc_init();
    if(fcc_status != FCC_STATUS_SUCCESS) {
        printf("fcc_init failed with status %d! - exit\n", fcc_status);
        return 1;
    }

    // Resets storage to an empty state.
    // Use this function when you want to clear storage from all the factory-tool generated data and user data.
    // After this operation device must be injected again by using factory tool or developer certificate.
#ifdef RESET_STORAGE
    printf("Reset storage to an empty state.\n");
    fcc_status_e delete_status = fcc_storage_delete();
    if (delete_status != FCC_STATUS_SUCCESS) {
        printf("Failed to delete storage - %d\n", delete_status);
    }
#endif

    // Deletes existing firmware images from storage.
    // This deletes any existing firmware images during application startup.
    // This compilation flag is currently implemented only for mbed OS.
#ifdef RESET_FIRMWARE
    palStatus_t status = PAL_SUCCESS;
    status = pal_fsRmFiles(DEFAULT_FIRMWARE_PATH);
    if(status == PAL_SUCCESS) {
        printf("Firmware storage erased.\n");
    } else if (status == PAL_ERR_FS_NO_PATH) {
        printf("Firmware path not found/does not exist.\n");
    } else {
        printf("Firmware storage erasing failed with %" PRId32, status);
        return 1;
    }
#endif

#if MBED_CONF_APP_DEVELOPER_MODE == 1
    printf("Start developer flow\n");
    fcc_status = fcc_developer_flow();
    if (fcc_status == FCC_STATUS_KCM_FILE_EXIST_ERROR) {
        printf("Developer credentials already exist\n");
    } else if (fcc_status != FCC_STATUS_SUCCESS) {
        printf("Failed to load developer credentials - exit\n");
        return 1;
    }
#endif
    fcc_status = fcc_verify_device_configured_4mbed_cloud();
    if (fcc_status != FCC_STATUS_SUCCESS) {
        printf("Device not configured for mbed Cloud - exit\n");
        return 1;
    }
}

bool SimpleMbedCloudClient::call_register() {

    _cloud_client.on_registered(this, &SimpleMbedCloudClient::client_registered);
    _cloud_client.on_unregistered(this, &SimpleMbedCloudClient::client_unregistered);
    _cloud_client.on_error(this, &SimpleMbedCloudClient::error);

    printf("Connecting...\n");
    bool setup = _cloud_client.setup(net);
    _register_called = true;
    if (!setup) {
        printf("Client setup failed\n");
        return false;
    }

#ifdef MBED_CLOUD_CLIENT_SUPPORT_UPDATE
    /* Set callback functions for authorizing updates and monitoring progress.
       Code is implemented in update_ui_example.cpp
       Both callbacks are completely optional. If no authorization callback
       is set, the update process will procede immediately in each step.
    */
    update_ui_set_cloud_client(&_cloud_client);
    _cloud_client.set_update_authorize_handler(update_authorize);
    _cloud_client.set_update_progress_handler(update_progress);
#endif
    return true;
}

void SimpleMbedCloudClient::close() {
    _cloud_client.close();
}

void SimpleMbedCloudClient::register_update() {
    _cloud_client.register_update();
}

void SimpleMbedCloudClient::client_registered() {
    _registered = true;
    printf("\nClient registered\n\n");
    static const ConnectorClientEndpointInfo* endpoint = NULL;
    if (endpoint == NULL) {
        endpoint = _cloud_client.endpoint_info();
        if (endpoint) {

#if MBED_CONF_APP_DEVELOPER_MODE == 1
            printf("Endpoint Name: %s\r\n", endpoint->internal_endpoint_name.c_str());
#else
            printf("Endpoint Name: %s\r\n", endpoint->endpoint_name.c_str());
#endif
            printf("Device Id: %s\r\n", endpoint->internal_endpoint_name.c_str());
        }
    }
#ifdef MBED_HEAP_STATS_ENABLED
    heap_stats();
#endif
}

void SimpleMbedCloudClient::client_unregistered() {
    _registered = false;
    _register_called = false;
    printf("\nClient unregistered - Exiting application\n\n");
#ifdef MBED_HEAP_STATS_ENABLED
    heap_stats();
#endif
}

void SimpleMbedCloudClient::error(int error_code) {
    const char *error;
    switch(error_code) {
        case MbedCloudClient::ConnectErrorNone:
            error = "MbedCloudClient::ConnectErrorNone";
            break;
        case MbedCloudClient::ConnectAlreadyExists:
            error = "MbedCloudClient::ConnectAlreadyExists";
            break;
        case MbedCloudClient::ConnectBootstrapFailed:
            error = "MbedCloudClient::ConnectBootstrapFailed";
            break;
        case MbedCloudClient::ConnectInvalidParameters:
            error = "MbedCloudClient::ConnectInvalidParameters";
            break;
        case MbedCloudClient::ConnectNotRegistered:
            error = "MbedCloudClient::ConnectNotRegistered";
            break;
        case MbedCloudClient::ConnectTimeout:
            error = "MbedCloudClient::ConnectTimeout";
            break;
        case MbedCloudClient::ConnectNetworkError:
            error = "MbedCloudClient::ConnectNetworkError";
            break;
        case MbedCloudClient::ConnectResponseParseFailed:
            error = "MbedCloudClient::ConnectResponseParseFailed";
            break;
        case MbedCloudClient::ConnectUnknownError:
            error = "MbedCloudClient::ConnectUnknownError";
            break;
        case MbedCloudClient::ConnectMemoryConnectFail:
            error = "MbedCloudClient::ConnectMemoryConnectFail";
            break;
        case MbedCloudClient::ConnectNotAllowed:
            error = "MbedCloudClient::ConnectNotAllowed";
            break;
        case MbedCloudClient::ConnectSecureConnectionFailed:
            error = "MbedCloudClient::ConnectSecureConnectionFailed";
            break;
        case MbedCloudClient::ConnectDnsResolvingFailed:
            error = "MbedCloudClient::ConnectDnsResolvingFailed";
            break;
#ifdef MBED_CLOUD_CLIENT_SUPPORT_UPDATE
        case MbedCloudClient::UpdateWarningCertificateNotFound:
            error = "MbedCloudClient::UpdateWarningCertificateNotFound";
            break;
        case MbedCloudClient::UpdateWarningIdentityNotFound:
            error = "MbedCloudClient::UpdateWarningIdentityNotFound";
            break;
        case MbedCloudClient::UpdateWarningCertificateInvalid:
            error = "MbedCloudClient::UpdateWarningCertificateInvalid";
            break;
        case MbedCloudClient::UpdateWarningSignatureInvalid:
            error = "MbedCloudClient::UpdateWarningSignatureInvalid";
            break;
        case MbedCloudClient::UpdateWarningVendorMismatch:
            error = "MbedCloudClient::UpdateWarningVendorMismatch";
            break;
        case MbedCloudClient::UpdateWarningClassMismatch:
            error = "MbedCloudClient::UpdateWarningClassMismatch";
            break;
        case MbedCloudClient::UpdateWarningDeviceMismatch:
            error = "MbedCloudClient::UpdateWarningDeviceMismatch";
            break;
        case MbedCloudClient::UpdateWarningURINotFound:
            error = "MbedCloudClient::UpdateWarningURINotFound";
            break;
        case MbedCloudClient::UpdateWarningRollbackProtection:
            error = "MbedCloudClient::UpdateWarningRollbackProtection";
            break;
        case MbedCloudClient::UpdateWarningUnknown:
            error = "MbedCloudClient::UpdateWarningUnknown";
            break;
        case MbedCloudClient::UpdateErrorWriteToStorage:
            error = "MbedCloudClient::UpdateErrorWriteToStorage";
            break;
        case MbedCloudClient::UpdateErrorInvalidHash:
            error = "MbedCloudClient::UpdateErrorInvalidHash";
            break;
#endif
        default:
            error = "UNKNOWN";
    }
    printf("\nError occurred : %s\r\n", error);
    printf("Error code : %d\r\n\n", error_code);
    printf("Error details : %s\r\n\n",_cloud_client.error_description());
}

bool SimpleMbedCloudClient::is_client_registered() {
    return _registered;
}

bool SimpleMbedCloudClient::is_register_called() {
    return _register_called;
}

void SimpleMbedCloudClient::register_and_connect() {
    // TODO this might not work if called more than once...
    mcc_resource_def resourceDef;

    // TODO clean up
    for (unsigned int i = 0; i < _resources.size(); i++) {
        _resources[i]->get_data(&resourceDef);
        _resources[i]->set_resource(add_resource(&_obj_list, resourceDef.object_id, resourceDef.instance_id,
                     resourceDef.resource_id, resourceDef.name.c_str(), M2MResourceInstance::STRING,
                     (M2MBase::Operation)resourceDef.method_mask, resourceDef.value, resourceDef.observable,
                     resourceDef.callback, resourceDef.notification_callback));
    }
    _cloud_client.add_objects(_obj_list);

    // Start registering to the cloud.
    call_register();

    // Print memory statistics if the MBED_HEAP_STATS_ENABLED is defined.
    #ifdef MBED_HEAP_STATS_ENABLED
        printf("Register being called\r\n");
        heap_stats();
    #endif
}

MbedCloudClient& SimpleMbedCloudClient::get_cloud_client() {
    return _cloud_client;
}

MbedCloudClientResource* SimpleMbedCloudClient::create_resource(const char *path, const char *name) {
    MbedCloudClientResource *resource = new MbedCloudClientResource(this, path, name);
    _resources.push_back(resource);
    return resource;
}

M2MResource* SimpleMbedCloudClient::add_cloud_resource(uint16_t object_id, uint16_t instance_id,
                          uint16_t resource_id, const char *resource_type,
                          M2MResourceInstance::ResourceType data_type,
                          M2MBase::Operation allowed, const char *value,
                          bool observable, void *cb, void *notification_status_cb) {
     return add_resource(&_obj_list, object_id, instance_id, resource_id, resource_type, data_type,
                  allowed, value, observable, cb, notification_status_cb);
}
