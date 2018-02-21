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
: client(client),
  resource(NULL),
  path(path),
  name(name),
  putCallback(NULL),
  postCallback(NULL),
  notificationCallback(NULL),
  internalPostCallback(this, &MbedCloudClientResource::internal_post_callback),
  internalPutCallback(this, &MbedCloudClientResource::internal_put_callback),
  internalNotificationCallback(this, &MbedCloudClientResource::internal_notification_callback)
{
}

void MbedCloudClientResource::observable(bool observable) {
    this->isObservable = observable;
}

void MbedCloudClientResource::methods(unsigned int methodMask) {
    this->methodMask = methodMask;
}

void MbedCloudClientResource::attach_put_callback(Callback<void(MbedCloudClientResource*, std::string)> callback) {
    this->putCallback = callback;
}

void MbedCloudClientResource::attach_post_callback(Callback<void(MbedCloudClientResource*, const uint8_t*, uint16_t)> callback) {
    this->postCallback = callback;
}

void MbedCloudClientResource::attach_notification_callback(Callback<void(MbedCloudClientResource*, const NoticationDeliveryStatus)> callback) {
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

void MbedCloudClientResource::set_value(const char *value) {
    this->value = value;

    if (this->resource) {
        this->resource->set_value((uint8_t*)this->value.c_str(), strlen(value));
    }
}

std::string MbedCloudClientResource::get_value() {
    if (this->resource) {
        return std::string(this->resource->get_value_string().c_str());
    } else {
        return std::string(this->value.c_str());
    }
}

void MbedCloudClientResource::internal_post_callback(void *params) {
    if (!postCallback) return;

    if (params) { // data can be NULL!
        M2MResource::M2MExecuteParameter* parameters = static_cast<M2MResource::M2MExecuteParameter*>(params);

        // extract the data that was sent
        const uint8_t* buffer = parameters->get_argument_value();
        uint16_t length = parameters->get_argument_value_length();

        postCallback(this, buffer, length);
    }
}

void MbedCloudClientResource::internal_put_callback(const char* resource) {
    if (!putCallback) return;

    putCallback(this, this->get_value());
}

void MbedCloudClientResource::internal_notification_callback(const M2MBase& m2mbase, const NoticationDeliveryStatus status) {
    if (!notificationCallback) return;

    notificationCallback(this, status);
}

const char * MbedCloudClientResource::delivery_status_to_string(const NoticationDeliveryStatus status) {
    switch(status) {
        case NOTIFICATION_STATUS_INIT: return "Init";
        case NOTIFICATION_STATUS_BUILD_ERROR: return "Build error";
        case NOTIFICATION_STATUS_RESEND_QUEUE_FULL: return "Resend queue full";
        case NOTIFICATION_STATUS_SENT: return "Sent";
        case NOTIFICATION_STATUS_DELIVERED: return "Delivered";
        case NOTIFICATION_STATUS_SEND_FAILED: return "Send failed";
        case NOTIFICATION_STATUS_SUBSCRIBED: return "Subscribed";
        case NOTIFICATION_STATUS_UNSUBSCRIBED: return "Unsubscribed";
        default: return "Unknown";
    }
}

M2MResource *MbedCloudClientResource::get_m2m_resource() {
    return resource;
}

void MbedCloudClientResource::get_data(mcc_resource_def *resourceDef) {
    path_to_ids(this->path.c_str(), &(resourceDef->object_id), &(resourceDef->instance_id), &(resourceDef->resource_id));
    resourceDef->name = this->name;
    resourceDef->method_mask = this->methodMask;
    resourceDef->observable = this->isObservable;
    resourceDef->value = m2m::String(this->get_value().c_str());
    resourceDef->put_callback = &(this->internalPutCallback);
    resourceDef->post_callback = &(this->internalPostCallback);
    resourceDef->notification_callback = &(this->internalNotificationCallback);
}

void MbedCloudClientResource::set_m2m_resource(M2MResource *res) {
    this->resource = res;
}
