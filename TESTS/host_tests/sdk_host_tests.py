## ----------------------------------------------------------------------------
## Copyright 2016-2018 ARM Ltd.
##
## SPDX-License-Identifier: Apache-2.0
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
## ----------------------------------------------------------------------------

from mbed_host_tests import BaseHostTest
from mbed_cloud.device_directory import DeviceDirectoryAPI
from mbed_cloud.connect import ConnectAPI
import os
import time
import subprocess
import re

DEFAULT_CYCLE_PERIOD = 1.0

class SDKTests(BaseHostTest):
    
    __result = None
    deviceApi = None
    connectApi = None
    deviceID = None
    iteration = None
    post_timeout = None

    def send_safe(self, key, value):
        #self.send_kv('dummy_start', 0)
        self.send_kv(key, value)
        self.send_kv(key, value)
        self.send_kv(key, value)
        self.send_kv(key, value)
        self.send_kv(key, value)
        #self.send_kv('dummy_end', 1)
    
    def test_steps(self):
        # Step 0 set up test
        system_reset = yield
        
        # Step 1 device connected to Pelion should reset.
        self.send_safe('reset', 0)
        time.sleep(self.program_cycle_s)
        self.send_safe('__sync', 0)
        self.iteration = self.iteration + 1
        system_reset = yield   
        
        #Step 2, finish
        yield True
        
    def _callback_device_ready(self, key, value, timestamp):
        # Send device iteration number after a reset
        self.send_safe('iteration', self.iteration)
    
    def _callback_advance_test(self, key, value, timestamp):
        # Advance test sequence
        try: 
            if self.test_steps_sequence.send(None):
                self.notify_complete(True)
        except (StopIteration, RuntimeError) as exc:
            self.notify_complete(False)
        
    def _callback_device_api_registration(self, key, value, timestamp):
        try:
            #set value for later use
            self.deviceID = value
            
            # Check if device is in Mbed Cloud Device Directory
            device = self.deviceApi.get_device(value)
            
            # Send registraton status to device
            self.send_safe("registration", 1 if device.state == "registered" else 0)
        except:
            # SDK throws an exception if the device is not found (unsuccessful registration) or times out
            self.send_safe("registration", 0)
            
    def _callback_device_verification(self, key, value, timestamp):
        # Send true if old DeviceID is the same as current device is
        self.send_safe("verification", 1 if self.deviceID == value else 0)
        
    def _callback_fail_test(self, key, value, timestamp):
        # Test failed. End it.
        self.notify_complete(False)
        
    def _callback_device_lwm2m_get_verification(self, key, value, timestamp):
        timeout = 0

        # Get resource value from device
        async_response = self.connectApi.get_resource_value_async(self.deviceID, value)
        
        # Set a 30 second timeout here.
        while not async_response.is_done and timeout <= 300:
            time.sleep(0.1)
            timeout += 1
        
        if async_response.is_done:
            # Send resource value back to device
            self.send_safe("get_value", async_response.value)
        else:
            # Request timed out.
            self.send_safe("timeout", 0)
    
    def _callback_device_lwm2m_set_verification(self, key, value, timestamp):
        timeout = 0

        # Get resource value from device
        async_response = self.connectApi.get_resource_value_async(self.deviceID, value)
        
        # Set a 30 second timeout here.
        while not async_response.is_done and timeout <= 300:
            time.sleep(0.1)
            timeout += 1
        
        if async_response.is_done:
            # Send resource value back to device
            self.send_safe("set_value", async_response.value)
        else:
            # Request timed out.
            self.send_safe("timeout", 0)
    
    def _callback_device_lwm2m_put_verification(self, key, value, timestamp):
        timeout = 0
        
        # Get resource value from device and increment it
        resource_value = self.connectApi.get_resource_value_async(self.deviceID, value)
        
        # Set a 30 second timeout here.
        while not resource_value.is_done and timeout <= 300:
            time.sleep(0.1)
            timeout += 1
            
        if not resource_value.is_done:
            self.send_safe("timeout", 0)
            return
        
        updated_value = int(resource_value.value) + 5
        
        # Set new resource value from cloud
        async_response = self.connectApi.set_resource_value_async(self.deviceID, value, updated_value)
        
        # Set a 30 second timeout here.
        while not async_response.is_done and timeout <= 300:
            time.sleep(0.1)
            timeout += 1
            
        if not async_response.is_done:
            self.send_safe("timeout", 0)
        else:
            # Send new resource value to device for verification.
            self.send_safe("res_set", updated_value);
        
    def _callback_device_lwm2m_post_verification(self, key, value, timestamp):
        timeout = 0
        
        # Execute POST function on device
        resource_value = self.connectApi.execute_resource_async(self.deviceID, value)
        
        # Set a 30 second timeout here.
        while not resource_value.is_done and timeout <= 300:
            time.sleep(0.1)
            timeout += 1
            
        if not resource_value.is_done:
            self.send_safe("timeout", 0)
            self.post_timeout = 1
        
    def _callback_device_lwm2m_post_verification_result(self, key, value, timestamp):
        
        # Called from callback function on device, POST function working as expected.
        # If post_timeout is not none, the request took longer than 30 seconds, which is
        # a failure. Don't send this value.
        if not self.post_timeout:
            self.send_safe("post_test_executed", 0)

    def setup(self):
        #Start at iteration 0
        self.iteration = 0
        
        # Register callbacks from GT tests
        self.register_callback('device_api_registration', self._callback_device_api_registration)
        self.register_callback('advance_test', self._callback_advance_test)
        self.register_callback('device_ready', self._callback_device_ready)
        self.register_callback('device_verification', self._callback_device_verification)
        self.register_callback('fail_test', self._callback_fail_test)
        self.register_callback('device_lwm2m_get_test', self._callback_device_lwm2m_get_verification)
        self.register_callback('device_lwm2m_set_test', self._callback_device_lwm2m_set_verification)
        self.register_callback('device_lwm2m_put_test', self._callback_device_lwm2m_put_verification)
        self.register_callback('device_lwm2m_post_test', self._callback_device_lwm2m_post_verification)
        self.register_callback('device_lwm2m_post_test_result', self._callback_device_lwm2m_post_verification_result)
        
        # Setup API config
        try:
            result = subprocess.check_output(["mbed", "config", "--list"], \
                                            stderr=subprocess.STDOUT)
        except Exception, e:
            print "Error: CLOUD_SDK_API_KEY global config is not set: " + str(e)
            return -1

        match = re.search(r'CLOUD_SDK_API_KEY=(.*)\n', result)
        if match == None:
            print "Error: CLOUD_SDK_API_KEY global config is not set."
            return -1

        api_key_val = match.group(1).strip()

        # Get API KEY and remove LF char if included
        print "CLOUD_SDK_API_KEY: " + api_key_val

        api_config = {"api_key" : api_key_val, "host" : "https://api.us-east-1.mbedcloud.com"}
        
        # Instantiate Device and Connect API
        self.deviceApi = DeviceDirectoryAPI(api_config)
        self.connectApi = ConnectAPI(api_config)
        
    def result(self):
        return self.__result

    def teardown(self):
        # Delete device from directory so as not to hit device allocation quota.
        if self.deviceID:
            self.deviceApi.delete_device(self.deviceID)
            
        pass
    
    def __init__(self):
        super(SDKTests, self).__init__()
        cycle_s = self.get_config_item('program_cycle_s')
        self.program_cycle_s = cycle_s if cycle_s is not None else DEFAULT_CYCLE_PERIOD
        
        self.test_steps_sequence = self.test_steps()
        self.test_steps_sequence.send(None)