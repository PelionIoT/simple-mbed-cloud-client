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
        /**
         * Create a new resource, this function should not be called directly!
         *
         * Always create a new resource via 'create_resource' on the SimpleMbedCloudClient object.
         *
         * @param client Instance of SimpleMbedCloudClient
         * @param path LwM2M path (in the form of 3200/0/5501)
         * @param name Name of the resource (will be shown in the UI)
         */
        MbedCloudClientResource(SimpleMbedCloudClient *client, const char *path, const char *name);

        /**
         * Sets whether the resource can be observed
         * When set, Pelion Device Management can subscribe for updates
         *
         * @param observable
         */
        void observable(bool observable);

        /**
         * Sets the methods that can be applied on this resource
         *
         * @param methodMask Mask of objects of type M2MMethod, e.g. 'M2MMethod::GET | M2MMethod::POST'
         */
        void methods(unsigned int methodMask);

        /**
         * Set a callback when a PUT action on this resource happens
         * Fires whenever someone writes to the resource from Pelion Device Management
         *
         * @params callback
         */
        void attach_put_callback(Callback<void(MbedCloudClientResource*, m2m::String)> callback);

        /**
         * Set a callback when a POST action on this resource happens
         * Fires whenever someone executes the resource from Pelion Device Management
         *
         * @params callback
         */
        void attach_post_callback(Callback<void(MbedCloudClientResource*, const uint8_t*, uint16_t)> callback);

        /**
         * Set a callback when a POST action on this resource happens
         * Fires whenever a notification (e.g. subscribed to resource) was sent
         * from Pelion Device Management
         *
         * @params callback
         */
        void attach_notification_callback(Callback<void(MbedCloudClientResource*, const NoticationDeliveryStatus)> callback);

        /**
         * Clear the PUT callback
         */
        void detach_put_callback();

        /**
         * Clear the POST callback
         */
        void detach_post_callback();

        /**
         * Clear the notification callback
         */
        void detach_notification_callback();

        /**
         * Set the value of the resource to an integer.
         * Underneath all values in Pelion Device Management are strings, so this will serialize the value
         *
         * @param value New value
         */
        void set_value(int value);

        /**
         * Set the value of the resource to a string.
         *
         * @param value New value
         */
        void set_value(const char *value);

        /**
         * Set the value of the resource to a float.
         * Underneath all values in Pelion Device Management are strings, so this will serialize the value
         *
         * @param value New value
         */
        void set_value(float value);

        /**
         * Get the value of the resource as a string
         *
         * @returns Current value
         */
        m2m::String get_value();

        /**
         * Get the value of the resource as an integer
         * Underneath all values in Pelion Device Management are strings, so this will de-serialize the value
         *
         * @returns Current value
         */
        int get_value_int();

        /**
         * Get the value of the resource as a float
         * Underneath all values in Pelion Device Management are strings, so this will de-serialize the value
         *
         * @returns Current value
         */
        float get_value_float();

        /**
         * Get a structure with all the data from the resource,
         * useful to serialize a resource, or for debugging purposes
         *
         * @param resourceDef a pointer to a mcc_resource_def structure
         */
        void get_data(mcc_resource_def *resourceDef);

        /**
         * Set the underlying M2MResource
         *
         * @param res Pointer to an instance of M2MResource
         */
        void set_m2m_resource(M2MResource *res);

        /**
         * Get the underlying M2MResource
         *
         * @returns Current M2MResource that manages this resource
         *          Lifetime will be the same as this object.
         */
        M2MResource* get_m2m_resource();

        /**
         * Convert the delivery status of a notification callback to a string
         */
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
