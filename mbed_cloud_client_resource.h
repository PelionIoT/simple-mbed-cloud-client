#ifndef MBED_CLOUD_CLIENT_RESOURCE_H
#define MBED_CLOUD_CLIENT_RESOURCE_H

#include "simple-mbed-cloud-client.h"
#include "mbed-client/m2mstring.h"

#define NUM_M2M_METHODS 4

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
    const char* value;
    bool observable;
    void *callback;
    void *notification_callback;
};

class SimpleMbedCloudClient;

class MbedCloudClientResource {
    public:
        MbedCloudClientResource(SimpleMbedCloudClient *client, const char *path, const char *name);

        void observable(bool observable);
        void methods(unsigned int methodMask);
        void attach(M2MMethod::M2MMethod method, void *callback);
        void attach_notification(M2MMethod::M2MMethod method, void *callback);
        void detatch(M2MMethod::M2MMethod method);
        void detatch_notification(M2MMethod::M2MMethod method);
        void set_value(int value);
        void set_value(char *value);
        const char* get_value();

        void get_data(mcc_resource_def *resourceDef);
        void set_resource(M2MResource *resource);

    private:
        SimpleMbedCloudClient *client;
        M2MResource *resource;
        String path;
        String name;
        String value;
        bool isObservable;
        unsigned int methodMask;

        void *callbacks[NUM_M2M_METHODS];
        void *notification_callbacks[NUM_M2M_METHODS];
};

#endif // MBED_CLOUD_CLIENT_RESOURCE_H
