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

MbedCloudClientResource::MbedCloudClientResource(SimpleMbedCloudClient *client, const char *path, const char *name)
: client(client) {
    this->path = path;
    this->name = name;
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
    // TODO
}

void MbedCloudClientResource::set_value(char *value) {
    // TODO
}

char* MbedCloudClientResource::get_value() {
    // TODO
    return (char*)"test";
}
