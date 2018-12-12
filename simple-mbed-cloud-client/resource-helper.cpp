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

#include "mbed-cloud-client/MbedCloudClient.h"
#include "m2mresource.h"
#include "mbed-client/m2minterface.h"
#include <stdio.h>
#include <string.h>
#include "mbed.h"

static void notification_delivery_status_cb_thunk(const M2MBase& base,
                                                  const NoticationDeliveryStatus status,
                                                  void *client_args) {
    ((Callback<void(const M2MBase& base, const NoticationDeliveryStatus status)>*)client_args)->call(base, status);
}

M2MResource* add_resource(M2MObjectList *list, uint16_t object_id, uint16_t instance_id,
                          uint16_t resource_id, const char *resource_type, M2MResourceInstance::ResourceType data_type,
                          M2MBase::Operation allowed, const char *value, bool observable, Callback<void(const char*)> *put_cb,
                          Callback<void(void*)> *post_cb,
                          Callback<void(const M2MBase&, const NoticationDeliveryStatus)> *notification_status_cb)
{
    M2MObject *object = NULL;
    M2MObjectInstance* object_instance = NULL;
    M2MResource* resource = NULL;
    char name[6];

    //check if object already exists.
    if (!list->empty()) {
        M2MObjectList::const_iterator it;
        it = list->begin();
        for ( ; it != list->end(); it++ ) {
            if ((*it)->name_id() == object_id) {
                object = (*it);
                break;
            }
        }
    }
    //Create new object if needed.
    if (!object) {
        snprintf(name, 6, "%d", object_id);
        object = M2MInterfaceFactory::create_object(name);
        list->push_back(object);
    } else {
        //check if instance already exists.
        object_instance = object->object_instance(instance_id);
    }
    //Create new instance if needed.
    if (!object_instance) {
        object_instance = object->create_object_instance(instance_id);
    }
    //create the recource.
    snprintf(name, 6, "%d", resource_id);
    resource = object_instance->create_dynamic_resource(name, resource_type, data_type, observable);
    //Set value if available.
    if (strcmp(value, "") != 0) {
        resource->set_value((const unsigned char*)value, strlen(value));
    }
    //Set allowed operations for accessing the resource.
    resource->set_operation(allowed);
    if (observable) {
        resource->set_notification_delivery_status_cb(notification_delivery_status_cb_thunk, notification_status_cb);
    }

    if (put_cb) {
        resource->set_value_updated_function(
            FP1<void, const char*>(put_cb,
                (void (Callback<void(const char*)>::*)(const char*))
                    &Callback<void(const char*)>::call));
    }

    if (post_cb) {
        resource->set_execute_function(FP1<void, void*>(post_cb,
            (void (Callback<void(void*)>::*)(void*))
                &Callback<void(void*)>::call));
    }

    return resource;
}
