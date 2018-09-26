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
#include "update_client_hub.h"
#include "mbed-trace/mbed_trace.h"
#include "mbed-trace-helper.h"

#ifdef MBED_CLOUD_CLIENT_USER_CONFIG_FILE
#include MBED_CLOUD_CLIENT_USER_CONFIG_FILE
#endif

#ifdef MBED_CLOUD_CLIENT_SUPPORT_UPDATE
#include "update_ui_example.h"
#endif

#ifdef MBED_HEAP_STATS_ENABLED
#include "memory_tests.h"
#endif

#ifndef DEFAULT_FIRMWARE_PATH
#define DEFAULT_FIRMWARE_PATH       "/sd/firmware"
#endif

BlockDevice *arm_uc_blockdevice;

SimpleMbedCloudClient::SimpleMbedCloudClient(NetworkInterface *net, BlockDevice *bd, FileSystem *fs) :
    _registered(false),
    _register_called(false),
    _register_and_connect_called(false),
    _registered_cb(NULL),
    _unregistered_cb(NULL),
    _net(net),
    _bd(bd),
    _fs(fs)
{
    arm_uc_blockdevice = bd;
}

SimpleMbedCloudClient::~SimpleMbedCloudClient() {
    for (unsigned int i = 0; _resources.size(); i++) {
        delete _resources[i];
    }
}

int SimpleMbedCloudClient::init() {
    // Requires DAPLink 245+ (https://github.com/ARMmbed/DAPLink/pull/364)
    // Older versions: workaround to prevent possible deletion of credentials:
    wait(1);

    extern const uint8_t arm_uc_vendor_id[];
    extern const uint16_t arm_uc_vendor_id_size;
    extern const uint8_t arm_uc_class_id[];
    extern const uint16_t arm_uc_class_id_size;

    ARM_UC_SetVendorId(arm_uc_vendor_id, arm_uc_vendor_id_size);
    ARM_UC_SetClassId(arm_uc_class_id, arm_uc_class_id_size);

    // Initialize Mbed Trace for debugging
    // Create mutex for tracing to avoid broken lines in logs
    if(!mbed_trace_helper_create_mutex()) {
        printf("ERROR - Mutex creation for mbed_trace failed!\n");
        return 1;
    }

    // Initialize mbed trace
    (void) mbed_trace_init();
    mbed_trace_mutex_wait_function_set(mbed_trace_helper_mutex_wait);
    mbed_trace_mutex_release_function_set(mbed_trace_helper_mutex_release);

    // Initialize the FCC
    fcc_status_e fcc_status = fcc_init();
    if(fcc_status != FCC_STATUS_SUCCESS) {
        printf("[SMCC] Factory Client Configuration failed with status %d. \n", fcc_status);
        return 1;
    }

    fcc_status = fcc_verify_device_configured_4mbed_cloud();

    if (fcc_status == FCC_STATUS_KCM_STORAGE_ERROR) {
        int mount_result = mount_storage();
        if (mount_result != 0) {
            printf("[SMCC] Failed to mount file system with status %d. \n", mount_result);
#if !defined(MBED_CONF_APP_FORMAT_STORAGE_LAYER_ON_ERROR) || MBED_CONF_APP_FORMAT_STORAGE_LAYER_ON_ERROR == 0
            return 1;
#endif
        } else {
            // Retry with mounted filesystem.
            fcc_status = fcc_verify_device_configured_4mbed_cloud();
        }
    }

    // This is designed to simplify user-experience by auto-formatting the
    // primary storage if no valid certificates exist.
    // This should never be used for any kind of production devices.
#if defined(MBED_CONF_APP_FORMAT_STORAGE_LAYER_ON_ERROR) && MBED_CONF_APP_FORMAT_STORAGE_LAYER_ON_ERROR == 1
    if (fcc_status != FCC_STATUS_SUCCESS) {
        if (reformat_storage() != 0) {
            return 1;
        }

        reset_storage();
    }
#else
    if (fcc_status != FCC_STATUS_SUCCESS) {
        printf("[SMCC] Device not configured for mbed Cloud - try re-formatting your storage device or set MBED_CONF_APP_FORMAT_STORAGE_LAYER_ON_ERROR to 1\n");
        return 1;
    }
#endif

    // Resets storage to an empty state.
    // Use this function when you want to clear storage from all the factory-tool generated data and user data.
    // After this operation device must be injected again by using factory tool or developer certificate.
#ifdef RESET_STORAGE
    reset_storage();
#endif

    // Deletes existing firmware images from storage.
    // This deletes any existing firmware images during application startup.
    // This compilation flag is currently implemented only for mbed OS.
#ifdef RESET_FIRMWARE
    palStatus_t status = PAL_SUCCESS;
    status = pal_fsRmFiles(DEFAULT_FIRMWARE_PATH);
    if(status == PAL_SUCCESS) {
        printf("[SMCC] Firmware storage erased.\n");
    } else if (status == PAL_ERR_FS_NO_PATH) {
        printf("[SMCC] Firmware path not found/does not exist.\n");
    } else {
        printf("[SMCC] Firmware storage erasing failed with %" PRId32, status);
        return 1;
    }
#endif

#if MBED_CONF_APP_DEVELOPER_MODE == 1
    printf("[SMCC] Starting developer flow\n");
    fcc_status = fcc_developer_flow();
    if (fcc_status == FCC_STATUS_KCM_FILE_EXIST_ERROR) {
        printf("[SMCC] Developer credentials already exist\n");
    } else if (fcc_status != FCC_STATUS_SUCCESS) {
        printf("[SMCC] Failed to load developer credentials - is the storage device active and accessible?\n");
        return 1;
    }
#endif

    return 0;
}

bool SimpleMbedCloudClient::call_register() {
    // need to unregister first before calling this function again
    if (_register_called) return false;

    _cloud_client.on_registered(this, &SimpleMbedCloudClient::client_registered);
    _cloud_client.on_unregistered(this, &SimpleMbedCloudClient::client_unregistered);
    _cloud_client.on_error(this, &SimpleMbedCloudClient::error);

    bool setup = _cloud_client.setup(_net);
    _register_called = true;
    if (!setup) {
        printf("[SMCC] Client setup failed\n");
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
    static const ConnectorClientEndpointInfo* endpoint = NULL;
    if (endpoint == NULL) {
        endpoint = _cloud_client.endpoint_info();
        if (endpoint && _registered_cb) {
            _registered_cb(endpoint);
        }
    }
#ifdef MBED_HEAP_STATS_ENABLED
    heap_stats();
#endif
}

void SimpleMbedCloudClient::client_unregistered() {
    _registered = false;
    _register_called = false;

    if (_unregistered_cb) {
        _unregistered_cb();
    }

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

    // @todo: move this into user space
    printf("\n[SMCC] Error occurred : %s\n", error);
    printf("[SMCC] Error code : %d\n", error_code);
    printf("[SMCC] Error details : %s\n",_cloud_client.error_description());
}

bool SimpleMbedCloudClient::is_client_registered() {
    return _registered;
}

bool SimpleMbedCloudClient::is_register_called() {
    return _register_called;
}

bool SimpleMbedCloudClient::register_and_connect() {
    if (_register_and_connect_called) return false;

    mcc_resource_def resourceDef;

    for (int i = 0; i < _resources.size(); i++) {
        _resources[i]->get_data(&resourceDef);
        M2MResource *res = add_resource(&_obj_list, resourceDef.object_id, resourceDef.instance_id,
                    resourceDef.resource_id, resourceDef.name.c_str(), M2MResourceInstance::STRING,
                    (M2MBase::Operation)resourceDef.method_mask, resourceDef.value.c_str(), resourceDef.observable,
                    resourceDef.put_callback, resourceDef.post_callback, resourceDef.notification_callback);
        _resources[i]->set_m2m_resource(res);
    }
    _cloud_client.add_objects(_obj_list);

    _register_and_connect_called = true;

    // Start registering to the cloud.
    bool retval = call_register();

    // Print memory statistics if the MBED_HEAP_STATS_ENABLED is defined.
    #ifdef MBED_HEAP_STATS_ENABLED
        printf("[SMCC] Register being called\r\n");
        heap_stats();
    #endif

    return retval;
}

void SimpleMbedCloudClient::on_registered(Callback<void(const ConnectorClientEndpointInfo*)> cb) {
    _registered_cb = cb;
}

void SimpleMbedCloudClient::on_unregistered(Callback<void()> cb) {
    _unregistered_cb = cb;
}

MbedCloudClient& SimpleMbedCloudClient::get_cloud_client() {
    return _cloud_client;
}

MbedCloudClientResource* SimpleMbedCloudClient::create_resource(const char *path, const char *name) {
    MbedCloudClientResource *resource = new MbedCloudClientResource(this, path, name);
    _resources.push_back(resource);
    return resource;
}

int SimpleMbedCloudClient::reformat_storage()
{
    int reformat_result = -1;
    printf("[SMCC] Autoformatting the storage.\n");
    if (_bd) {
        reformat_result = _fs->reformat(_bd);
        if (reformat_result != 0) {
            printf("[SMCC] Autoformatting failed with error %d\n", reformat_result);
        }
    }
    return reformat_result;
}

void SimpleMbedCloudClient::reset_storage()
{
    printf("[SMCC] Reset storage to an empty state.\n");
    fcc_status_e delete_status = fcc_storage_delete();
    if (delete_status != FCC_STATUS_SUCCESS) {
        printf("[SMCC] Failed to delete storage - %d\n", delete_status);
    }
}

int SimpleMbedCloudClient::mount_storage()
{
    int mount_result = -1;
    printf("[SMCC] Initializing storage.\n");
    if (_bd) {
        mount_result = _fs->mount(_bd);
    }
    return mount_result;
}
