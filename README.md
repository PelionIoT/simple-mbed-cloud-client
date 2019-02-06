# Mbed Device Management

(aka Simple Pelion DM Client, SPDMC, previously called Simple Mbed Cloud Client)

A simple way of connecting Mbed OS 5 devices to Arm Pelion Device Management IoT platform.

It's designed to:
* Enable applications to connect and perform firmware updates in few lines of code.
* Run separate from your main application, it does not take over your main loop.
* Provide LWM2M resources, essentially variables that are automatically synced through Pelion Device Management Client.
* Help users avoid doing blocking network operations in interrupt contexts, by automatically defering actions to a separate thread.
* Provide end to end Greentea tests for Pelion Device Management Client

This library is aiming to making it trivial to expose sensors, actuators and other variables to a cloud service. For a complete Pelion Device Management CLient API, check our [documentation](https://cloud.mbed.com/docs/current/mbed-cloud-client/index.html).


## Table of Contents

1. [Adding device management feature to your application](#adding-device-management-feature-to-your-application)
2. [Device management configuration](#device-management-configuration)
3. [Validation and testing](#validation-and-testing)
4. [Known issues](#known-issues)

## Device management for your Mbed OS 5 application

First and foremost, it's important to note that not every device (microcontroller, module, board) is capable of running device management features. And while some hardware capabilities can be easily added or extended - like connectivity, storage, TRNG - others are impossible or inconvenient to extend (e.g. RAM/Flash).

Here's a list of minimum of requirements to add device management feature to your application:
* RAM: 128K or more
* Flash: 512K or more
* Real Time Clock (RTC)
* (optional but recommended) True Random Number Generator (TRNG)
* A storage device - SDcard, SPI Flash, QSPI Flash, Data Flash
* IP connectivity - Ethernet, Wi-Fi, Cellular, 6LoWPAN, Thread

Additionally, the device and any additional complementary hardware components should be supported and provided in the latest releases of Mbed OS, or have support for the APIs provided in the latest releases of Mbed OS.

#### Useful references

* Check which Mbed OS platforms are supported in the [Pelion Device Management quick-start guide](https://cloud.mbed.com/quick-start).
* Check which storage options are available [here](https://os.mbed.com/docs/latest/reference/storage.html).
* Check which network options are available [here](https://os.mbed.com/docs/latest/reference/network-socket.html).


### Adding device management feature to your application

1. Add this library to your Mbed OS project:

    ```
    $ mbed add https://github.com/ARMmbed/simple-mbed-cloud-client
    ```

    If you do not have an Mbed OS project to add, you can create one with `mbed new <your_application_name>` and then the `mbed add` step above.

2. Configure the API key for your Pelion Portal account.

     If you don't have an API key available, then login in [Pelion IoT Platform portal](https://portal.mbedcloud.com/), navigate to 'Access Management', 'API keys' and create a new one. Then specify the API key as global `mbed` configuration:

    ```
    $ mbed config -G CLOUD_SDK_API_KEY <your-api-key>
    ```

3. Install the device management certificate:

    ```
    $ mbed dm init -d "company.com" --model-name "product-model" -q --force
    ```

4. Reference the library from your `main.cpp` file, add network and storage drivers; finally initialize the SimpleMbedCloudClient library. The is the architecture of a generic device management application with Mbed OS:

    ```cpp
    #include "simple-mbed-cloud-client.h"
    #include <Block device>
    #include <Filesystem>
    #include <Network>

    int main() {

        /* Initialize connectivity */
        NetworkInterface *net = NetworkInterface::get_default_instance();
        net->connect();

        /* Initialize storage */
        BlockDevice *bd = BlockDevice::get_default_instance();
        <Filesystem> fs("fs", &bd);

        /* Initialize Simple Pelion DM Client */
        SimpleMbedCloudClient client(net, &bd, &fs);
        client.init();

        /* Create resource */
        MbedCloudClientResource *variable;
        variable = client.create_resource("3201/0/5853", "variable");
        variable->set_value("assign new value");
        variable->methods(M2MMethod::GET | M2MMethod::PUT);

    }
    ```

### Example applications

  To help you start quickly, please refer to the following [generic application example](https://github.com/ARMmbed/pelion-ready-example). It demonstrates how to connect to the Pelion IoT Platform service, register resources and get ready to receive a firmware update.

  Also, there are a number of board-specific applications that focus on providing more elaborate hardware features with Mbed OS and the Pelion IoT Platform. These are available in the Pelion [Quick-Start](https://cloud.mbed.com/quick-start).


## Device management configuration

### Context and terminology 

The device management configuration can be divided into 5 distinct areas:
 * Connectivity related - the transport type for the device to connect to the device management service.
 * Storage related - the storage type and writing which will be used for both the credentials and the firmware storage.
 * Flash geometry related - the device flash "sandbox" for bootloader, application header, application image and also 2 SOTP regions.
 * SOTP related - the address and size of the SOTP regions in flash which will be used to store the device special key used to decrypt the credentials storage.
 * Bootloader related - the bootloader image, application and header offset.

Except for connectivity, the majority of the configuration is shared between the application and bootloader, which is to ensure that the bootloader can correctly find, verify, authorize and apply an update to the device application.

For full documentation about bootloaders and firmware update, read the following documents:
- [Introduction to Mbed OS bootloaders](https://os.mbed.com/docs/latest/porting/bootloader.html)
- [Creating and using an Mbed OS bootloader](https://os.mbed.com/docs/latest/tutorials/bootloader.html)
- [Bootloader configuration in Mbed OS](https://os.mbed.com/docs/latest/tools/configuring-tools.html)
- [Mbed Bootloader for Pelion Device Management Client](https://github.com/ARMmbed/mbed-bootloader)
- [Updating devices with Mbed CLI](https://os.mbed.com/docs/latest/tools/cli-update.html)

To speed up things up, you can copy the configuration from the [generic application example](https://github.com/ARMmbed/pelion-ready-example/blob/master/mbed_app.json) as basis for your application configuration.

### 1. Create new application configuration for your target

Edit the `mbed_app.json` and create a new entry under `target_overrides` with the target name for your device:
   * **Connectivity** - specify default connectivity type for your target. It's essential with targets that lack default connectivity set in    `targets.json` or for targets that support multiple connectivity options. Example:
   ```
            "target.network-default-interface-type" : "ETHERNET",
   ```
   At the time of this writing, the possible options are `ETHERNET`, `WIFI`, `CELLULAR`.
   
   Depending on connectivity type, you might have to specify more config options, e.g. see already defined `CELLULAR` targets in `mbed_app.json`.

   * **Storage** - specify storage blockdevice type, which dynamically adds the blockdevice driver you specified at compile time. Example:
   ```
            "target.components_add" : ["SD"],
   ```
   Valid options are `SD`, `SPIF`, `QSPIF`, `FLASHIAP` (not recommended). Check more available options under https://github.com/ARMmbed/mbed-os/tree/master/components/storage/blockdevice.

   You will also have to specify blockdevice pin configuration, which may be very different from one blockdevice type to another. Here's an example for `SD`:
   ```
            "sd.SPI_MOSI"                      : "PE_6",
            "sd.SPI_MISO"                      : "PE_5",
            "sd.SPI_CLK"                       : "PE_2",
            "sd.SPI_CS"                        : "PE_4",
   ```
   
   * **Flash** - define the basics for the microcontroller flash, e.g.:
   ```
            "device_management.flash-start-address"              : "0x08000000",
            "device_management.flash-size"                       : "(2048*1024)",
   ```
   
   * **SOTP** - define 2 SOTP/NVStore regions which will be used for Mbed OS Device Management to store it's special keys which are used to encrypt the data stored on the storage. Use the last 2 Flash sectors (if possible) to ensure that they don't get overwritten when new firmware is applied. Example:
   ```
            "device_management.sotp-section-1-address"            : "(MBED_CONF_APP_FLASH_START_ADDRESS + MBED_CONF_APP_FLASH_SIZE - 2*(128*1024))",
            "device_management.sotp-section-1-size"               : "(128*1024)",
            "device_management.sotp-section-2-address"            : "(MBED_CONF_APP_FLASH_START_ADDRESS + MBED_CONF_APP_FLASH_SIZE - 1*(128*1024))",
            "device_management.sotp-section-2-size"               : "(128*1024)",
            "device_management.sotp-num-sections" : 2
   ```
   `*-address` defines the start of the Flash sector and `*-size` defines the actual sector size. Currently `sotp-num-sections` should always be set to `2`.

At this point, it's recommended that you run the "connect" test suite, which verifies that the device can successfully bootstrap, register and send/receive data from Pelion Device Management service with the provided configuration.

If you already configured your Pelion API key and initialized your credentials as described in the [previous section](#adding-device-management-feature-to-your-application), you can compile the "Connect" tests using:
```
$ mbed test -t <TOOLCHAIN> -m <BOARD> -n simple*dev*connect -DMBED_TEST_MODE --compile
```

To run the tests:
```
$ mbed test -t <TOOLCHAIN> -m <BOARD> -n simple*dev*connect --run -v
```

#### 2. Configure and compile bootloader

Assuming that you've successfully passed the "Connect" tests as described above, you can enable firmware update feature by adding a bootloader to your application.

1. Import as a new application the official [mbed-bootloader](https://github.com/ARMmbed/mbed-bootloader/) repository or the [mbed-bootloader-extended](https://github.com/ARMmbed/mbed-bootloader-extended/) repository that builds on top of `mbed-bootloader` and extends the support for filesystems and storage drivers. You can do this with ```mbed import mbed-bootloader-extended```.

1. Inside the imported bootloader application, edit the application configuration, e.g. `mbed-bootloader-extended/mbed_app.json`, add a new target entry similar to the step above and specify:

   * **Flash** - define the basics for the microcontroller flash (the same as in `mbed_app.json`), e.g.:
    ```
            "flash-start-address"              : "0x08000000",
            "flash-size"                       : "(2048*1024)",
    ```

   * **SOTP** - similar to the **SOTP** step above, specify the location of the SOTP key storage. Note that in the bootloader, the variables are named differently. We should try to use the last 2 Flash sectors (if possible) to ensure that they don't get overwritten when new firmware is applied. Example:
    ```
            "nvstore.area_1_address"           : "(MBED_CONF_APP_FLASH_START_ADDRESS + MBED_CONF_APP_FLASH_SIZE - 2*(128*1024))",
            "nvstore.area_1_size"              : "(128*1024)",
            "nvstore.area_2_address"           : "(MBED_CONF_APP_FLASH_START_ADDRESS + MBED_CONF_APP_FLASH_SIZE - 1*(128*1024))", "nvstore.area_2_size" : "(128*1024)",
    ```

    * **Application offset** - specify start address for the application and also the update-client meta info. As these are automatically calculated, you could copy the ones below:
    ```
            "update-client.application-details": "(MBED_CONF_APP_FLASH_START_ADDRESS + 64*1024)",
            "application-start-address"        : "(MBED_CONF_APP_FLASH_START_ADDRESS + 65*1024)",
            "max-application-size"             : "DEFAULT_MAX_APPLICATION_SIZE",
    ```
    
    * **Storage** - specify blockdevice pin configuration, exactly as you defined it in the `mbed_app.json` file. Example:
    ```
            "target.components_add"            : ["SD"],
            "sd.SPI_MOSI"                      : "PE_6",
            "sd.SPI_MISO"                      : "PE_5",
            "sd.SPI_CLK"                       : "PE_2",
            "sd.SPI_CS"                        : "PE_4"
    ```

3. Compile the bootloader using the `bootloader_app.json` configuration you just editted:
   ```
   mbed compile -t <TOOLCHAIN> -m <TARGET> --profile=tiny.json --app-config=<path to pelion-enablement/bootloader/bootloader_app.json>
   ```

Note the following:
 * `mbed-bootloader` is primarily optimized for `GCC_ARM` and therefore you might want to compile it with that toolchain.
 * Before jumping to the next step, you should compile and flash the bootloader, and then connect over the virtual comport to ensure that the bootloader is running correctly. You can ignore errors related to checksum verification or falure to jump to application - these are expected at this stage.

#### 3. Include the bootloader

1. Copy the compiled bootloader from `mbed-bootloader-extended/BUILDS/<TARGET>/<TOOLCHAIN>-TINY/mbed-bootloader.bin` to `<your_application_name>/bootloader/mbed-bootloader-<TARGET>.bin`.

2. Edit `<your_application_name>/mbed_app.json` and modify the target entry to include:
```
            "target.features_add"              : ["BOOTLOADER"],
            "target.bootloader_img"            : "bootloader/mbed-bootloader-<TARGET>.bin",
            "target.app_offset"                : "0x10400",
            "target.header_offset"             : "0x10000",
            "update-client.application-details": "(MBED_CONF_APP_FLASH_START_ADDRESS + 64*1024)",
```
 
   Note that:
   * `update-client.application-details` should be identical in both `bootloader_app.json` and `mbed_app.json`
   * `target.app_offset` is relative offset to `flash-start-address` you specified in the `mbed_app.json` and `bootloader_app.json`, and is the hex value of the offset specified by `application-start-address` in `bootloader_app.json`, e.g. `(MBED_CONF_APP_FLASH_START_ADDRESS+65*1024)` dec equals `0x10400` hex.
   * `target.header_offset` is also relative offset to the `flash-start-address` you specified in the `bootloader_app.json`, and is the hex value of the offset specified by `update-client.application-details`, e.g. `(MBED_CONF_APP_FLASH_START_ADDRESS+64*1024)` dec equals `0x10000` hex.

7. Finally, compile and re-run all tests, including the firmware update ones with:
```
$ mbed test -t <TOOLCHAIN> -m <BOARD> -n simple*dev*connect -DMBED_TEST_MODE --compile

$ mbed test -t <TOOLCHAIN> -m <BOARD> -n simple*dev*connect --run -v
```

Refer to the next section about what tests are being executed.

## Validation and Testing

Mbed Device Management provides built-in tests to help you when define your device management configuration. Before running these tests, it's recommended that you run the `mbed dm init` command, which will install all needed credentials for both Connect and Update Pelion DM features. You can use the following command:
```
$ mbed dm init -d "<your company name.com>" --model-name "<product model identifier>" -q --force
```


### Test suites - Basic

| **Test suite** | **Description** |
| ------------- | ------------- |
| `fs-single` | Filesystem single-threaded tests with write buffer sizes - 1 byte, 4b, 16b, 64b, 256b, 1kb, 4kb, 16kb, 64kb. |
| `fs-multi` | Filesystem multi-threaded test with write buffer sizes - 1 byte, 4b, 16b, 64b, 256b, 1kb, 4kb, 16kb, 64kb. |
| `net-single` | Network single-threaded test with receive buffer sizes - 128 bytes, 256b, 1kb, 2kb, 4kb. |
| `net-multi` | Network multi-threaded test for 1, 2 and 3 download threads with 1kb receive buffer size. |
| `stress-net-fs` | Network and Filesystem single and multi-threaded tests:<ul><li>1 thread (sequential) - 1 download (1kb buffer), 1 file thread (1kb buffer)</li><li>2 parallel threads - 1 download, 1 file thread (1kb buffer)</li><li>3 parallel threads - 1 download, 2 file (256 bytes, 1 kb buffer)</li><li>4 parallel threads - 1 download, 3 file (1 byte, 256 bytes, 1kb buffer)</li></ul> |

### Test cases - Connect

| **Test case** | **Description** |
| ------------- | ------------- |
| `Connect to <Network type>` | Tests the connection to the network via network interface. |
| `Initialize <Blockdevice>+<Filesystem>` | Initializes block device driver and filesystem on top. Usually the test will be stuck at this point if there's problem with the storage device. |
| `Format <Filesystem>` | Tests that the blockdevice can be successfully formatted for the filesystem type. |
| `Initialize Simple PDMC ` | Verifies that Simple PDMC can be initialized with the given network, storage, and filesystem configuration. This is where the FCU and KCM configuration is written to storage and the Root of Trust is written to SOTP.
| `Pelion DM Bootstrap & Register` | Bootstraps the device and registers it for first time with Pelion Device Management. |
| `Pelion DM Directory` | Verifies that a registered device appears in the Device Directory in Pelion Device Management. |
| `Pelion DM Re-register` | Resets the device and re-registers with Pelion Device Management with previously bootstrapped credentials. |
| `Post-reset Identity` | Verifies that the device identity is preserved over device reset, confirming that Root of Trust is stored in SOTP correctly. |
| `ResourceLwM2M GET` | Verifies that Pelion DM API client can perform a GET request on an LwM2M resource. |
| `ResourceLwM2M SET Test` | Sets/changes value from the device and verifies that Pelion DM API client can observe the value changing. |
| `ResourceLwM2M PUT Test` | Verifies that Pelion DM API client can perform a PUT request on an LwM2M resource by setting a new value. |
| `Resource LwM2M POST Test` | Verifies that Pelion DM API client can execute a POST on an LwM2M resource and the callback function on the device is called. |

### Test cases - Update

| **Test case** | **Description** |
| ------------- | ------------- |
| `Connect to <Network type>` | Tests the connection to the network via network interface. |
| `Initialize <Blockdevice>+<Filesystem>` | Initializes block device driver and filesystem on top. Usually the test will be stuck at this point if there's problem with the storage device. |
| `Format <Filesystem>` | Tests that the blockdevice can be successfully formatted for the filesystem type. |
| `Initialize Simple PDMC ` | Verifies that Simple PDMC can be initialized with the given network, storage, and filesystem configuration. This is where the FCU and KCM configuration is written to storage and the Root of Trust is written to SOTP.
| `Pelion DM Bootstrap & Register` | Bootstraps the device and registers it for first time with Pelion Device Management. |
| `Pelion DM Directory` | Verifies that a registered device appears in the Device Directory in Pelion Device Management. |
| `Firmware Prepare` | Prepares the firmware on the host side and calls `mbed dm` to initiate Pelion Device Management update campaign. |
| `Firmware Download` | Downloads the firmware onto the device. |
| `Firmware Update` | Reset the device, verifies that the firmware has correct checksum, applies it and re-verifies the applied firmware checksum. |
| `Pelion DM Re-register` | Re-registers the device with Pelion Device Management using the new firmware and previously bootstrapped credentials. |
| `Post-update Identity` | Verifies that the device identity is preserved over firmware update and device reset, confirming that Root of Trust is stored in SOTP correctly. |

### Requirements
Mbed Device Management tests rely on the Python SDK to test the end to end solution. To install the Python SDK:
```
 $ pip install mbed-cloud-sdk
```
**Note:** The Python SDK requires Python 2.7.10+ / Python 3.4.3+, built with SSL support.

### Testing Setup

1. Import an Simple Pelion DM Client application that contains the corresponding configuration in `mbed_app.json`. The application will include this Simple Pelion DM Client library.

    For examples of platform configuration, see the applications available in the [Quick-start](https://cloud.mbed.com/quick-start).

2. Set a global `mbed config` variable `CLOUD_SDK_API_KEY` on the host machine valid for the account that your device will connect to. For example:
```
$ mbed config -G CLOUD_SDK_API_KEY <API_KEY>
```

For instructions on how to generate an API key, please [see the documentation](https://cloud.mbed.com/docs/current/integrate-web-app/api-keys.html#generating-an-api-key).

3. Initialize your Pelion DM credentials (once per project):
```
$ mbed dm init -d "<your organization>" --model-name "<product model>" -q --force
```

This will create your private/public key pair and also initialize various .c files with these credentials, so you can use Pelion DM connect and (firmware) update features.

4. Remove the `main.cpp` application from the project, or ensure the content of the file is wrapped with `#ifndef MBED_TEST_MODE`.
 
5. Compile the tests with the `MBED_TEST_MODE` compilation flag.
    
```
$ mbed test -t <toolchain> -m <platform> --app-config mbed_app.json -n simple-mbed-cloud-client-tests-* -DMBED_TEST_MODE --compile
```

5. Run the Simple Pelion Device Management Client tests from the application directory:

```
$ mbed test -t <toolchain> -m <platform> --app-config mbed_app.json -n simple-mbed-cloud-client-tests-* --run -v
```

### Troubleshooting
Below are a list of common issues and fixes for using Simple Pelion DM Client.

#### Autoformatting failed with error -5005
This is due to an issue with the storage block device. If using an SD card, ensure that the SD card is seated properly.

#### SYNC_FAILED during testing
Occasionally, if the test failed during a previous attempt, the SMCC Greentea tests will fail to sync. If this is the case, please replug your device to the host PC. Additionally, you may need to update your DAPLink or ST-Link interface firmware.

#### Device identity is inconsistent
If your device ID in Pelion Device Management is inconsistent over a device reset, it could be because it is failing to open the credentials on the storage held in the Enhanced Secure File System. Typically, this is because the device cannot access the Root of Trust stored in SOTP.

One way to verify this is to see if Simple Pelion DM Client autoformats the storage after a device reset when `format-storage-layer-on-error` is set to `1` in `mbed_app.json`.  It would appear on the serial terminal output from the device as the following:
```
[SMCC] Initializing storage.
[SMCC] Autoformatting the storage.
[SMCC] Reset storage to an empty state.
[SMCC] Starting developer flow
```

When this occurs, you should look at the SOTP sectors defined in `mbed_app.json`:
```
"sotp-section-1-address"           : "0xFE000",
"sotp-section-1-size"              : "0x1000",
"sotp-section-2-address"           : "0xFF000",
"sotp-section-2-size"              : "0x1000",
```
Ensure that the sectors are correct according to the flash layout of your device, and they are not being overwritten during the programming of the device. ST-Link devices will overwrite these sectors when using drag-and-drop of .bin files. Thus, moving the SOTP sectors to the end sectors of flash ensure that they will not be overwritten.

#### Stack Overflow
If you receive a stack overflow error, increase the Mbed OS main stack size to at least 6000. This can be done by modifying the following parameter in `mbed_app.json`:
```
 "MBED_CONF_APP_MAIN_STACK_SIZE=6000",
```

#### Device failed to register
Check the device allocation on your Pelion account to see if you are allowed additional devices to connect. You can delete development devices, after being deleted they will not count towards your allocation.


### Known issues
Check open issues on [GitHub](https://github.com/ARMmbed/simple-mbed-cloud-client/issues)
