#include "mbed.h"
#include "FATFileSystem.h"
#include "simple-mbed-cloud-client.h"
#include "greentea-client/test_env.h"

DigitalOut led1(LED1);
DigitalOut led2(LED2);
void led_thread() {
    led1 = 1;
    led2 = 0;
    while (true) {
        led1 = !led1;
        led2 = !led2;
        wait(0.5);
    }
}

// Default storage definition.
BlockDevice* bd = BlockDevice::get_default_instance();
FATFileSystem fs("sd", bd);

void wait_nb(uint16_t ms) {
    while (ms > 0) {
        ms--;
        wait_ms(1);
    }
}

void logger(const char* message, const char* decor) {
    wait_ms(10);
    printf(message, decor);
    wait_ms(10);
}
void logger(const char* message) {
    wait_ms(10);
    printf(message);
    wait_ms(10);
}

static const ConnectorClientEndpointInfo* endpointInfo;
void registered(const ConnectorClientEndpointInfo *endpoint) {
    printf("[INFO] Connected to Pelion Device Management. Device ID: %s\n",
            endpoint->internal_endpoint_name.c_str());
    endpointInfo = endpoint;
}

void post_test_callback(MbedCloudClientResource *resource, const uint8_t *buffer, uint16_t size) {
    logger("[INFO] POST test callback executed.\r\n");
    greentea_send_kv("device_lwm2m_post_test_result", 0);
}

void spdmc_testsuite_connect(void) {
    int iteration = 0;
    char _key[20] = { };
    char _value[128] = { };

    greentea_send_kv("device_ready", true);
    while (1) {
        greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

        if (strcmp(_key, "iteration") == 0) {
            iteration = atoi(_value);
            break;
        }
    }

    // provide manifest to greentea so it can correct show skipped and failed tests
    if (iteration == 0) {
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_COUNT, 10);
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "Connect to Network");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "Format Storage");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "Simple PDMC Initialization");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "Pelion DM Register");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "Pelion DM Directory");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "Consistent Identity");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "LwM2M GET Test");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "LwM2M SET Test");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "LwM2M PUT Test");
        greentea_send_kv(GREENTEA_TEST_ENV_TESTCASE_NAME, "LwM2M POST Test");
    }


    // Start network connection test.
    GREENTEA_TESTCASE_START("Connect to Network");
    logger("[INFO] Attempting to connect to network.\r\n");

    // Connection definition.
    NetworkInterface *net = NetworkInterface::get_default_instance();
    nsapi_error_t net_status = net->connect();

    // Report status to console.
    if (net_status != 0) {
        logger("[ERROR] Device failed to connect to Network.\r\n");
        greentea_send_kv("fail_test", 0);
    } else {
        logger("[INFO] Connected to network successfully. IP address: %s\n", net->get_ip_address());
    }

    GREENTEA_TESTCASE_FINISH("Connect to Network", (net_status == 0), (net_status != 0));

    // Instantiate SimpleMbedCloudClient.
    SimpleMbedCloudClient client(net, bd, &fs);

    // This must be done on the first iteration to ensure that we can test writing of new credentials. It may
    // happen twice if the reset storage flag is set to 1.
    if (iteration == 0) {
        logger("[INFO] Resetting storage to a clean state for test.\n");

        GREENTEA_TESTCASE_START("Format Storage");
        //int storage_status = 0;
        int storage_status = client.reformat_storage();

        // Report status to console.
        if (storage_status == 0) {
            logger("[INFO] Storage format successful.\r\n");
        } else {
            logger("[ERROR] Storage format failed.\r\n");
            greentea_send_kv("fail_test", 0);
        }

        GREENTEA_TESTCASE_FINISH("Format Storage", (storage_status == 0), (storage_status != 0));
    }

    // SimpleMbedCloudClient initialization must be successful.
    GREENTEA_TESTCASE_START("Simple PDMC Initialization");
    int client_status = client.init();

    // Report status to console.
    if (client_status == 0) {
        logger("[INFO] Simple Mbed Cloud Client initialization successful.\r\n");
    } else {
        logger("[ERROR] Simple Mbed Cloud Client failed to initialize.\r\n");
        // End the test early, cannot continue without successful cloud client initialization.
        greentea_send_kv("fail_test", 0);
    }

    GREENTEA_TESTCASE_FINISH("Simple PDMC Initialization", (client_status == 0), (client_status != 0));


    //Create LwM2M resources
    MbedCloudClientResource *res_get_test;
    res_get_test = client.create_resource("5000/0/1", "get_resource");
    res_get_test->observable(true);
    res_get_test->methods(M2MMethod::GET);
    res_get_test->set_value("test0");

    MbedCloudClientResource *res_put_test;
    res_put_test = client.create_resource("5000/0/2", "put_resource");
    res_put_test->methods(M2MMethod::PUT | M2MMethod::GET);
    res_put_test->set_value(1);

    MbedCloudClientResource *res_post_test;
    res_post_test = client.create_resource("5000/0/3", "post_resource");
    res_post_test->methods(M2MMethod::POST);
    res_post_test->attach_post_callback(post_test_callback);

    // Set client callback to report endpoint name.
    client.on_registered(&registered);

    // Register to Pelion Device Management.
    GREENTEA_TESTCASE_START("Pelion DM Register");

    client.register_and_connect();

    int timeout = 30000;
    while (timeout && !client.is_client_registered()) {
        timeout--;
        wait_ms(1);
    }

    // Get registration status.
    bool client_registered = client.is_client_registered();
    if (client_registered) {
        client_status = 0;
        logger("[INFO] Device successfully registered to Pelion DM.\r\n");
    } else {
        logger("[ERROR] Device failed to register.\r\n");
        client_status = -1;
        greentea_send_kv("fail_test", 0);
    }
    GREENTEA_TESTCASE_FINISH("Pelion DM Register", (client_status == 0), (client_status != 0));

    if (iteration == 0) {
        //Start registration status test
        GREENTEA_TESTCASE_START("Pelion DM Directory");
        int reg_status;

        logger("[INFO] Wait 5 seconds for Device Directory to update after initial registration.\r\n");
        wait_nb(5000);

        // Start host tests with device id
        logger("[INFO] Starting Pelion DM verification using Python SDK...\r\n");
        greentea_send_kv("device_api_registration", endpointInfo->internal_endpoint_name.c_str());
        while (1) {
            greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

            if (strcmp(_key, "registration") == 0) {
                if (atoi(_value)) {
                    reg_status = 0;
                    logger("[INFO] Device is registered in the Device Directory.\r\n");
                } else {
                    reg_status = -1;
                    logger("[ERROR] Device could not be verified as registered in Device Directory.\r\n");
                }
                break;
            }
        }

        GREENTEA_TESTCASE_FINISH("Pelion DM Directory", (reg_status == 0), (reg_status != 0));

        logger("[INFO] Resetting device.\r\n");
        greentea_send_kv("advance_test", 0);
        while (1) {
            greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

            if (strcmp(_key, "reset") == 0) {
                system_reset();
                break;
            }
        }
    } else {
        //Start consistent identity test.
        GREENTEA_TESTCASE_START("Consistent Identity");
        int identity_status;

        logger("[INFO] Wait 2 seconds for Device Directory to update after reboot.\r\n");
        wait_nb(2000);

        // Wait for Host Test to verify consistent device ID (blocking here)
        logger("[INFO] Verifying consistent Device ID...\r\n");
        greentea_send_kv("device_verification", endpointInfo->internal_endpoint_name.c_str());
        while (1) {
            greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

            if (strcmp(_key, "verification") == 0) {
                if (atoi(_value)) {
                    identity_status = 0;
                    logger("[INFO] Device ID consistent, SOTP and Secure Storage is preserved correctly.\r\n");
                } else {
                    identity_status = -1;
                    logger("[ERROR] Device ID is inconsistent. SOTP and Secure Storage was not preserved.\r\n");
                }
                break;
            }
        }

        GREENTEA_TESTCASE_FINISH("Consistent Identity", (identity_status == 0), (identity_status != 0));

        // LwM2M tests
        logger("[INFO] Beginning LwM2M resource tests.\r\n");


        wait_nb(200);
        // ---------------------------------------------
        // GET test
        GREENTEA_TESTCASE_START("LwM2M GET Test");
        int get_status;
        // Read original value of /5000/0/1 and wait for Host Test to verify it read the value and send it back.
        greentea_send_kv("device_lwm2m_get_test", "/5000/0/1");
        while (1) {
            greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

            if (strcmp(_key, "get_value") == 0) {
                if (strcmp(_value, "test0") == 0) {
                    get_status = 0;
                    logger("[INFO] Original value of LwM2M resource /5000/0/1 is read correctly\r\n");
                } else {
                    get_status = -1;
                    logger("[ERROR] Wrong value reported in Pelion DM.\r\n");
                }
                break;
            } else if (strcmp(_key, "timeout") == 0) {
                get_status = -1;
                logger("[ERROR] Observation of LwM2M resource /5000/0/1 timed out.\r\n");
                break;
            }
        }
        GREENTEA_TESTCASE_FINISH("LwM2M GET Test", (get_status == 0), (get_status != 0));


        wait_nb(200);
        // ---------------------------------------------
        // SET test
        GREENTEA_TESTCASE_START("LwM2M SET Test");
        int set_status;
        // Update resource /5000/0/1 from client and observe value
        res_get_test->set_value("test1");

        greentea_send_kv("device_lwm2m_set_test", "/5000/0/1");
        while (1) {
            greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

            if (strcmp(_key, "set_value") == 0) {
                if (strcmp(_value, "test1") == 0) {
                    set_status = 0;
                    logger("[INFO] Changed value of LwM2M resource /5000/0/1 is observed correctly\r\n");
                } else {
                    set_status = -1;
                    logger("[ERROR] Wrong value observed in Pelion DM.\r\n");
                }
                break;
            } else if (strcmp(_key, "timeout") == 0) {
                set_status = -1;
                logger("[ERROR] Observation of LwM2M resource /5000/0/1 timed out.\r\n");
                break;
            }
        }
        GREENTEA_TESTCASE_FINISH("LwM2M SET Test", (set_status == 0), (set_status != 0));


        wait_nb(200);
        // ---------------------------------------------
        // PUT Test
        GREENTEA_TESTCASE_START("LwM2M PUT Test");
        int put_status;
        int current_res_value;
        int updated_res_value;

        // Observe resource /5000/0/2 from cloud, add +5, and confirm value is correct on client
        greentea_send_kv("device_lwm2m_put_test", "/5000/0/2");
        while (1) {
            greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

            if (strcmp(_key, "res_set") == 0) {
                // Get updated value from host test.
                updated_res_value = atoi(_value);
                // Get current value from resource.
                current_res_value = res_put_test->get_value_int();

                if (updated_res_value == current_res_value) {
                    put_status = 0;
                    logger("[INFO] Value of resource /5000/0/2 successfully changed from the cloud using PUT.\r\n");
                } else {
                    put_status = -1;
                    logger("[ERROR] Wrong value read from device after resource update.\r\n");
                }
                break;
            } else if (strcmp(_key, "timeout") == 0) {
                put_status = -1;
                logger("[ERROR] PUT of LwM2M resource /5000/0/2 timed out.\r\n");
                break;
            }
        }

        GREENTEA_TESTCASE_FINISH("LwM2M PUT Test", (put_status == 0), (put_status != 0));


        wait_nb(200);
        // ---------------------------------------------
        // POST test
        GREENTEA_TESTCASE_START("LwM2M POST Test");
        int post_status;

        logger("[INFO] Executing POST on /5000/0/3 and waiting for callback function\r\n.");
        greentea_send_kv("device_lwm2m_post_test", "/5000/0/3");
        while (1) {
            greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

            if (strcmp(_key, "post_test_executed") == 0) {
                int result = atoi(_value);
                if (result == 0) {
                    post_status = 0;
                    logger("[INFO] Callback on resource /5000/0/3 executed successfully.\r\n");
                } else {
                    post_status = -1;
                    logger("[ERROR] Callback on resource /5000/0/3 failed.\r\n");
                }
                break;
            } else if (strcmp(_key, "timeout") == 0) {
                post_status = -1;
                logger("[ERROR] POST of LwM2M resource /5000/0/3 timed out.\r\n");
                break;
            }
        }

        GREENTEA_TESTCASE_FINISH("LwM2M POST Test", (post_status == 0), (post_status != 0));

        GREENTEA_TESTSUITE_RESULT((get_status == 0) && (set_status == 0) && (put_status == 0) && (post_status == 0));

        while (1) {
            wait(1);
        }
    }
}

int main(void) {
    //Create a thread to blink an LED and signal that the device is alive
    Thread thread;
    thread.start(led_thread);

    GREENTEA_SETUP(210, "sdk_host_tests");
    spdmc_testsuite_connect();

    return 0;
}
