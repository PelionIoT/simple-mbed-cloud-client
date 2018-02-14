#include "mbed.h"
#include "mbed_cloud_client_resource.h"
#include "simple-mbed-cloud-client.h"

void path_to_ids(const char* path, unsigned int *object_id,
                 unsigned int *instance_id, unsigned int *resource_id) {
    int len = strlen(path);
    char *buffer = new char[len + 1];
    buffer[len] = '\0';
    strncpy(buffer, path, len);
    unsigned int index = 0;
    char * pch = strtok (buffer, "/");

    unsigned int *ptr;
    while (pch != NULL && index < 3) {
        switch (index) {
            case 0:
                ptr = object_id;
            break;

            case 1:
                ptr = instance_id;
            break;

            case 2:
                ptr = resource_id;
            break;
        }

        *ptr = atoi(pch);
        pch = strtok (NULL, "/");
        index++;
    }

    delete[] buffer;
}

MbedCloudClientResource::MbedCloudClientResource(SimpleMbedCloudClient *client, const char *path, const char *name)
: client(client), resource(NULL), path(path), name(name), putCallback(NULL),
  postCallback(NULL), notificationCallback(NULL) {
}

void MbedCloudClientResource::observable(bool observable) {
    this->isObservable = observable;
}

void MbedCloudClientResource::methods(unsigned int methodMask) {
    this->methodMask = methodMask;
}

void MbedCloudClientResource::attach_put_callback(Callback<void(const char*)> callback) {
    this->putCallback = callback;
}

void MbedCloudClientResource::attach_post_callback(Callback<void(void*)> callback) {
    this->postCallback = callback;
}

void MbedCloudClientResource::attach_notification_callback(Callback<void(const M2MBase&, const NoticationDeliveryStatus)> callback) {
    this->notificationCallback = callback;
}

void MbedCloudClientResource::detach_put_callback() {
    this->putCallback = NULL;
}

void MbedCloudClientResource::detach_post_callback() {
    this->postCallback = NULL;
}

void MbedCloudClientResource::detach_notification_callback() {
    this->notificationCallback = NULL;
}

void MbedCloudClientResource::set_value(int value) {
    this->value = "";
    this->value.append_int(value);

    if (this->resource) {
        this->resource->set_value((uint8_t*)this->value.c_str(), this->value.size());
    }
}

void MbedCloudClientResource::set_value(char *value) {
    this->value = value;

    if (this->resource) {
        this->resource->set_value((uint8_t*)this->value.c_str(), strlen(value));
    }
}

String MbedCloudClientResource::get_value() {
    if (this->resource) {
        return this->resource->get_value_string();
    } else {
        return this->value;
    }
}

void MbedCloudClientResource::get_data(mcc_resource_def *resourceDef) {
    path_to_ids(this->path.c_str(), &(resourceDef->object_id), &(resourceDef->instance_id), &(resourceDef->resource_id));
    resourceDef->name = this->name;
    resourceDef->method_mask = this->methodMask;
    resourceDef->observable = this->isObservable;
    resourceDef->value = this->get_value();
    resourceDef->put_callback = &(this->putCallback);
    resourceDef->post_callback = &(this->postCallback);
    resourceDef->notification_callback = &(this->notificationCallback);
}

void MbedCloudClientResource::set_resource(M2MResource *res) {
    this->resource = res;
}
