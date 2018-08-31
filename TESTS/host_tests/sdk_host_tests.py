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
import os
import time

DEFAULT_CYCLE_PERIOD = 1.0

class SDKTests(BaseHostTest):
    
    __result = None
    deviceApi = None
    deviceID = None
    iteration = None
    
    def test_steps(self):
        # Step 0 set up test
        global iteration
        system_reset = yield
        
        # Step 1 device connected to Pelion should reset.
        self.send_kv('reset', 0)
        time.sleep(self.program_cycle_s)
        self.send_kv('__sync', 0)
        iteration = iteration + 1
        system_reset = yield   
        
        #Step 2, finish
        yield True
        
    def _callback_device_ready(self, key, value, timestamp):
        # Send device iteration number after a reset
        global iteration
        self.send_kv('iteration', iteration)
    
    def _callback_advance_test(self, key, value, timestamp):
        # Advance test sequence
        try: 
            if self.test_steps_sequence.send(None):
                self.notify_complete(True)
        except (StopIteration, RuntimeError) as exc:
            self.notify_complete(False)
        
    def _callback_device_api_registration(self, key, value, timestamp):
        global deviceID
        try:
            #set value for later use
            deviceID = value
            
            # Check if device is in Mbed Cloud Device Directory
            device = self.deviceApi.get_device(value)
            
            # Send registraton status to device
            self.send_kv("registration_status", device.state)
            
        except:
            # SDK throws an exception if the device is not found (unsuccessful registration) or times out
            self.send_kv("registration_status", "error")
            
    def _callback_device_verification(self, key, value, timestamp):
        global deviceID
        # Send true if old DeviceID is the same as current device is
        self.send_kv("verification", (deviceID == value))
        
    def _callback_fail_test(self, key, value, timestamp):
        # Test failed. End it.
        self.notify_complete(False)

    def setup(self):
        #Start at iteration 0
        global iteration
        iteration = 0
        
        # Register callbacks from GT tests
        self.register_callback('device_api_registration', self._callback_device_api_registration)
        self.register_callback('advance_test', self._callback_advance_test)
        self.register_callback('device_ready', self._callback_device_ready)
        self.register_callback('device_verification', self._callback_device_verification)
        self.register_callback('fail_test', self._callback_fail_test)
        
        # Setup API config
        api_key_val = os.environ.get("MBED_CLOUD_API_KEY")            
        api_config = {"api_key" : api_key_val, "host" : "https://api.us-east-1.mbedcloud.com"}
        
        # Instantiate Device API
        self.deviceApi = DeviceDirectoryAPI(api_config)
        
        
    def result(self):
        return self.__result

    def teardown(self):
        pass
    
    def __init__(self):
        super(SDKTests, self).__init__()
        cycle_s = self.get_config_item('program_cycle_s')
        self.program_cycle_s = cycle_s if cycle_s is not None else DEFAULT_CYCLE_PERIOD
        
        self.test_steps_sequence = self.test_steps()
        self.test_steps_sequence.send(None)
