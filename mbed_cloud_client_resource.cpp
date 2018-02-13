#include "mbed_cloud_client_resource.h"
#include "simple-mbed-cloud-client.h"

unsigned int m2m_method_to_index(M2MMethod::M2MMethod method) {
    switch (method) {
        case M2MMethod::GET:
            return 0;
        break;

        case M2MMethod::PUT:
            return 1;
        break;

        case M2MMethod::POST:
            return 2;
        break;

        case M2MMethod::DELETE:
            return 3;
        break;
    }
}

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
: client(client), resource(NULL), path(path), name(name) {
}

void MbedCloudClientResource::observable(bool observable) {
    this->isObservable = observable;
}

void MbedCloudClientResource::methods(unsigned int methodMask) {
    this->methodMask = methodMask;
}

void MbedCloudClientResource::attach(M2MMethod::M2MMethod method, void *callback) {
    this->callbacks[m2m_method_to_index(method)] = callback;
}

void MbedCloudClientResource::attach_notification(M2MMethod::M2MMethod method, void *callback) {
    this->notification_callbacks[m2m_method_to_index(method)] = callback;
}

void MbedCloudClientResource::detatch(M2MMethod::M2MMethod method) {
    this->callbacks[m2m_method_to_index(method)] = NULL;
}

void MbedCloudClientResource::detatch_notification(M2MMethod::M2MMethod method) {
    this->notification_callbacks[m2m_method_to_index(method)] = NULL;
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

const char* MbedCloudClientResource::get_value() {
    if (this->resource) {
        return this->resource->get_value_string().c_str();
    } else {
        return this->value.c_str();
    }
}

void MbedCloudClientResource::get_data(mcc_resource_def *resourceDef) {
    path_to_ids(this->path.c_str(), &(resourceDef->object_id), &(resourceDef->instance_id), &(resourceDef->resource_id));
    resourceDef->name = this->name;
    resourceDef->method_mask = this->methodMask;
    resourceDef->observable = this->isObservable;
    resourceDef->value = this->value.c_str();

    // TODO make these actual values
    resourceDef->callback = NULL;
    resourceDef->notification_callback = NULL;
}

void MbedCloudClientResource::set_resource(M2MResource *resource) {
    this->resource = resource;
}
