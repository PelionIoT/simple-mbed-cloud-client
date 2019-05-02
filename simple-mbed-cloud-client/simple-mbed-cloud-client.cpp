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
#include "mbed-trace-helper.h"
#include "resource-helper.h"

#ifdef MBED_CLOUD_DEV_UPDATE_ID
#include "update_client_hub.h"
#endif

#define TRACE_GROUP "SMCC"

#ifdef MBED_CLOUD_CLIENT_USER_CONFIG_FILE
#include MBED_CLOUD_CLIENT_USER_CONFIG_FILE
#endif

#ifdef MBED_CLOUD_CLIENT_SUPPORT_UPDATE
#include "update-helper/update-helper.h"
#endif

#ifdef MBED_HEAP_STATS_ENABLED
#include "memory_tests.h"
#endif

#ifndef DEFAULT_FIRMWARE_PATH
#define DEFAULT_FIRMWARE_PATH       "/fs/firmware"
#endif

SimpleMbedCloudClient::SimpleMbedCloudClient(NetworkInterface *net, BlockDevice *bd, FileSystem *fs) :
    _registered(false),
    _register_called(false),
    _register_and_connect_called(false),
    _registered_cb(NULL),
    _unregistered_cb(NULL),
    _error_cb(NULL),
    _net(net),
    _bd(bd),
    _fs(fs),
    _storage(bd, fs)
{
}

SimpleMbedCloudClient::~SimpleMbedCloudClient() {
    for (int i = 0; i < _resources.size(); i++) {
        delete _resources[i];
    }
}

int SimpleMbedCloudClient::init(bool format) {
    // Requires DAPLink 245+ (https://github.com/ARMmbed/DAPLink/pull/364)
    // Older versions: workaround to prevent possible deletion of credentials:
    wait(1);

#ifdef MBED_CLOUD_DEV_UPDATE_ID

    extern const uint8_t arm_uc_vendor_id[];
    extern const uint16_t arm_uc_vendor_id_size;
    extern const uint8_t arm_uc_class_id[];
    extern const uint16_t arm_uc_class_id_size;

    ARM_UC_SetVendorId(arm_uc_vendor_id, arm_uc_vendor_id_size);
    ARM_UC_SetClassId(arm_uc_class_id, arm_uc_class_id_size);

#endif

    // Initialize Mbed Trace for debugging
    // Create mutex for tracing to avoid broken lines in logs
    if(!mbed_trace_helper_create_mutex()) {
        printf("[SMCC] ERROR - Mutex creation for mbed_trace failed!\n");
        return 1;
    }

    // Initialize mbed trace
    mbed_trace_init();
    mbed_trace_helper_create_mutex();
    mbed_trace_mutex_wait_function_set(mbed_trace_helper_mutex_wait);
    mbed_trace_mutex_release_function_set(mbed_trace_helper_mutex_release);

    // Initialize the FCC
    int status = fcc_init();
    if (status != FCC_STATUS_SUCCESS && status != FCC_STATUS_ENTROPY_ERROR && status != FCC_STATUS_ROT_ERROR) {
        tr_error("Factory Client Configuration failed with status %d", status);
        return 1;
    }

    status = _storage.init();
    if (status != FCC_STATUS_SUCCESS) {
        tr_error("Failed to initialize storage layer (%d)", status);
        return 1;
    }

    status = _storage.sotp_init();
    if (status != FCC_STATUS_SUCCESS) {
        tr_error("Could not initialize SOTP (%d)", status);
        fcc_finalize();
        return 1;
    }

#if RESET_STORAGE
    status = reset_storage();
    if (status != FCC_STATUS_SUCCESS) {
        tr_error("reset_storage (triggered by RESET_STORAGE macro) failed (%d)", status);
        return 1;
    }
    // Reinitialize SOTP
    status = _storage.sotp_init();
    if (status != FCC_STATUS_SUCCESS) {
        tr_error("Could not initialize SOTP (%d)", status);
        return 1;
    }
#endif

    status = verify_cloud_configuration(format);

    if (status != 0) {
    // This is designed to simplify user-experience by auto-formatting the
    // primary storage if no valid certificates exist.
    // This should never be used for any kind of production devices.
#if MBED_CONF_APP_FORMAT_STORAGE_LAYER_ON_ERROR == 1
        tr_info("Could not load certificate (e.g. no certificates or RoT might have changed), resetting storage...");
        status = reset_storage();
        if (status != FCC_STATUS_SUCCESS) {
            return status;
        }
        status = _storage.sotp_init();
        if (status != FCC_STATUS_SUCCESS) {
            return status;
        }
        status = verify_cloud_configuration(format);
        if (status != 0) {
            return status;
        }
#else
        return 1;
#endif
    }

    // Deletes existing firmware images from storage.
    // This deletes any existing firmware images during application startup.
    // This compilation flag is currently implemented only for mbed OS.
#ifdef RESET_FIRMWARE
    palStatus_t status = PAL_SUCCESS;
    status = pal_fsRmFiles(DEFAULT_FIRMWARE_PATH);
    if(status == PAL_SUCCESS) {
        printf("[SMCC] Firmware storage erased\n");
    } else if (status == PAL_ERR_FS_NO_PATH) {
        tr_info("Firmware path not found/does not exist");
    } else {
        tr_error("Firmware storage erasing failed with %" PRId32, status);
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
        tr_error("Client setup failed");
        return false;
    }

#ifdef MBED_CLOUD_CLIENT_SUPPORT_UPDATE
    /* Set callback functions for authorizing updates and monitoring progress.
       Code is implemented in update_ui_example.cpp
       Both callbacks are completely optional. If no authorization callback
       is set, the update process will procede immediately in each step.
    */
    update_helper_set_cloud_client(&_cloud_client);
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

    if (_error_cb) {
        _error_cb(error_code, error);
        return;
    }

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
        tr_info("Register being called");
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

void SimpleMbedCloudClient::on_update_authorized(void (*cb)(int32_t request)) {
    _cloud_client.set_update_authorize_handler(cb);
}

void SimpleMbedCloudClient::on_update_progress(void (*cb)(uint32_t progress, uint32_t total)) {
    _cloud_client.set_update_progress_handler(cb);
}

void SimpleMbedCloudClient::on_error_cb(Callback<void(int, const char*)> cb) {
    _error_cb = cb;
}

int SimpleMbedCloudClient::reformat_storage() {
    return _storage.reformat_storage();
}

MbedCloudClient *SimpleMbedCloudClient::get_cloud_client() {
    return &_cloud_client;
}

MbedCloudClientResource* SimpleMbedCloudClient::create_resource(const char *path, const char *name) {
    MbedCloudClientResource *resource = new MbedCloudClientResource(this, path, name);
    _resources.push_back(resource);
    return resource;
}

int SimpleMbedCloudClient::reset_storage() {
    tr_info("Resetting storage to an empty state...");
    int status = fcc_storage_delete();
    if (status != FCC_STATUS_SUCCESS) {
        tr_debug("Failed to delete FCC storage (%d), formatting...", status);

        status = _storage.reformat_storage();
        if (status == 0) {
            tr_debug("Storage reformatted, resetting storage again...");
            // Try to reset storage again after format.
            // It is required to run fcc_storage_delete() after format.
            status = fcc_storage_delete();
            if (status != FCC_STATUS_SUCCESS) {
                tr_warn("Failed to delete FCC storage (again) (%d)", status);
            }
            else {
                tr_debug("Deleted FCC storage");
            }
        }
    }

    if (status == FCC_STATUS_SUCCESS) {
        tr_info("OK - Reset storage to an empty state...");
    }

    return status;
}

int SimpleMbedCloudClient::verify_cloud_configuration(bool format) {
    int status;

#if MBED_CONF_DEVICE_MANAGEMENT_DEVELOPER_MODE == 1
    tr_debug("Starting developer flow");
    if( format ) {
        status = reset_storage();
        if (status != FCC_STATUS_SUCCESS) {
            tr_debug("Failed to reset storage");
            return status;
        }
    }
    status = fcc_developer_flow();
    if (status == FCC_STATUS_KCM_FILE_EXIST_ERROR) {
        tr_debug("Developer credentials already exist on storage layer, verifying credentials...");
    } else if (status != FCC_STATUS_SUCCESS) {
        tr_debug("No developer credentials on storage layer yet");
        return status;
    }
#endif
    status = fcc_verify_device_configured_4mbed_cloud();
    return status;
}
