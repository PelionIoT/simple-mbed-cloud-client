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

#ifndef MBED_CLOUD_CLIENT_RESOURCE_H
#define MBED_CLOUD_CLIENT_RESOURCE_H

#include "mbed.h"
#include "simple-mbed-cloud-client.h"
#include "mbed-client/m2mstring.h"

namespace M2MMethod {

enum M2MMethod {
    GET    = 0x01,
    PUT    = 0x02,
    POST   = 0x04,
    DELETE = 0x08
};

};

struct mcc_resource_def {
    unsigned int object_id;
    unsigned int instance_id;
    unsigned int resource_id;
    String name;
    unsigned int method_mask;
    String value;
    bool observable;
    Callback<void(const char*)> *put_callback;
    Callback<void(void*)> *post_callback;
    Callback<void(const M2MBase&, const NoticationDeliveryStatus)> *notification_callback;
};

class SimpleMbedCloudClient;

class MbedCloudClientResource {
    public:
        MbedCloudClientResource(SimpleMbedCloudClient *client, const char *path, const char *name);

        void observable(bool observable);
        void methods(unsigned int methodMask);
        void attach_put_callback(Callback<void(MbedCloudClientResource*, m2m::String)> callback);
        void attach_post_callback(Callback<void(MbedCloudClientResource*, const uint8_t*, uint16_t)> callback);
        void attach_notification_callback(Callback<void(MbedCloudClientResource*, const NoticationDeliveryStatus)> callback);
        void detach_put_callback();
        void detach_post_callback();
        void detach_notification_callback();
        void set_value(int value);
        void set_value(const char *value);
        void set_value(float value);
        m2m::String get_value();
        int get_value_int();
        float get_value_float();

        void get_data(mcc_resource_def *resourceDef);
        void set_m2m_resource(M2MResource *res);
        M2MResource* get_m2m_resource();

        static const char * delivery_status_to_string(const NoticationDeliveryStatus status);

    private:
        void internal_post_callback(void* params);
        void internal_put_callback(const char* resource);
        void internal_notification_callback(const M2MBase& m2mbase, const NoticationDeliveryStatus status);

        SimpleMbedCloudClient *client;
        M2MResource *resource;
        m2m::String path;
        m2m::String name;
        m2m::String value;
        bool isObservable;
        unsigned int methodMask;

        Callback<void(MbedCloudClientResource*, m2m::String)> putCallback;
        Callback<void(MbedCloudClientResource*, const uint8_t*, uint16_t)> postCallback;
        Callback<void(MbedCloudClientResource*, const NoticationDeliveryStatus)> notificationCallback;
        Callback<void(void*)> internalPostCallback;
        Callback<void(const char*)> internalPutCallback;
        Callback<void(const M2MBase&, const NoticationDeliveryStatus)> internalNotificationCallback;
};

#endif // MBED_CLOUD_CLIENT_RESOURCE_H
