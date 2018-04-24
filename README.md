## Simple Mbed Cloud Client

A simple way of connecting Mbed OS 5 devices to Mbed Cloud. It's designed to:

* Enable Mbed Cloud Connect and mbed Cloud Update to applications in few lines of code.
* Run separate from your main application, it does not take over your main loop.
* Provide LWM2M resources, essentially variables that are automatically synced through Mbed Cloud Connect.
* Help users avoid doing blocking network operations in interrupt contexts, by automatically defering actions to a separate thread.

This library is a simpler interface to Mbed Cloud Client, making it trivial to expose sensors, actuators and other variables to the cloud. For a full Mbed Cloud CLient API, check our [documentation](https://cloud.mbed.com/docs/current/mbed-cloud-client/index.html).

### Usage to Connect to Mbed Cloud

1. Add this library to your Mbed OS project:

    ```
    $ mbed add https://github.com/ARMmbed/simple-mbed-cloud-client
    ```
    
1. Add your Mbed Cloud developer certificate to your project (`mbed_cloud_dev_credentials.c` file).

1. Reference the library from your main.cpp file, add network and storage drivers; finally initialize the Simple Mbed Cloud Client library.

    ```cpp
    #include "simple-mbed-cloud-client.h"
    #include <Block device>
    #include <Filesystem>
    #include <Network>

    int main() {
    
        /* Initialize connectivity */
        <Network> net;
        net.connect();
    
        /* Initialize storage */
        <Block device> sd(...);
        <Filesystem> fs("sd", &sd);

        /* Initialize Simple Mbed Cloud Client */
        SimpleMbedCloudClient client(&net, &sd, &fs);
        client.init();

        /* Create resource */        
        MbedCloudClientResource *variable;
        variable = client.create_resource("3201/0/5853", "variable");
        variable->set_value("assign new value");
        variable->methods(M2MMethod::GET | M2MMethod::PUT);

    }
    ```
   
### Example applications
  
  There are a number of applications that make usage of the Simple Mbed Cloud Client library.
  
  The Mbed Cloud [Quick-Start](https://cloud.mbed.com/quick-start) is an initiative to support Mbed Partner's platforms while delivering a great User Experience to Mbed Developers.  

### Known issues

None
