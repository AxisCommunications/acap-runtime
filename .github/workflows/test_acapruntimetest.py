import json
import os
import pytest
import requests
from requests.auth import HTTPDigestAuth
import subprocess
import time

AXIS_TARGET_ADDR="172.27.64.8"
AXIS_TARGET_USER="root"
AXIS_TARGET_PASS="pass"

ACAP_DOCKER_IMAGE_NAME=r"axisecp/acap-runtime:armv7hf-test"
ACAP_SPECIFIC_NAME = "acapruntimetest"

def acap_ctrl(action, wait = 0,
        device_ip=AXIS_TARGET_ADDR,
        device_pass=AXIS_TARGET_PASS,
        docker_image_name=ACAP_DOCKER_IMAGE_NAME):
    """Static method for controlling ACAP application via docker.
    """
    with open('/tmp/output.log','a') as output:
        subprocess.call(
        f"docker run --rm {docker_image_name} {device_ip} {device_pass} {action}",
        shell=True, stdout=output, stderr=output)
    if wait != 0:
        time.sleep(wait)

class TestClassAcapRuntimeTest:

    http_session = None

    @pytest.fixture()
    def dut(self):
        # Setup
        yield "dut"
        # Teardown

    def setup_method(self):
        print("\n****Setup****")
        self.init_dut_connection()
        status = self.check_dut_status()
        assert status, f"Could not connect to {AXIS_TARGET_ADDR}"
        print("Installing ACAP runtime test suite")
        acap_ctrl("install")
        installed = self.check_acap_installed()
        assert installed, "Failed to install ACAP runtime test suite"

    def teardown_method(self):
        print("\n****Teardown****")
        print("Stopping ACAP runtime test suite")
        acap_ctrl("stop", 1)
        print("Removing ACAP runtime test suite")
        acap_ctrl("remove", 1)
        installed = self.check_acap_installed()
        assert not installed, "Failed to remove ACAP runtime test suite."
        print("Rebooting device. This will take some time.")
        reboot = self.reboot_device(2)
        assert reboot, "Failed to reboot DUT after test."
        status = self.check_dut_status(max_time=360)
        assert status, "Failed to get status of DUT after reboot."

    def test_method(self, dut):
        print(f"****Testing {ACAP_SPECIFIC_NAME}****")
        assert 1 == 1 , "This should always pass"


#----------- Support methods ------------------------------------------------

    def init_dut_connection(self):
        """Initiates a requests Session to be used for http communication with the DUT."""
        self.http_session = requests.Session()
        self.http_session.auth = HTTPDigestAuth(AXIS_TARGET_USER, AXIS_TARGET_PASS)
        self.http_session.headers ={"Content-Type": "application/json; charset=utf-8"}
        return self.check_dut_status()

    def check_dut_status(self, timeout=120, max_time=120):
        """Uses the VAPIX Systemready API to check that the DUT is responding."""
        url = f"http://{AXIS_TARGET_ADDR}/axis-cgi/systemready.cgi"
        json_body = { "apiVersion": "1.2", "method":"systemready", "params": { "timeout": 20 }}
        if self.http_session:
            #print(f"Check DUT status. This can take some time, time out set to {max_time} seconds.")
            start_time = time.time()
            while True:
                if time.time()-start_time > max_time:
                    print("DUT Status check timed out")
                    return False
                try:
                    r = self.http_session.post(url, json=json_body, timeout=timeout)
                    if r.status_code == 200:
                        # check if request was ok
                        response_dict = json.loads(r.text)
                        if "data" in response_dict.keys():
                            data_dict = response_dict["data"]
                            if "systemready" in data_dict.keys() and "needsetup" in data_dict.keys():
                                if data_dict["systemready"] == "yes" and data_dict["needsetup"] == "no":
                                    return True
                except requests.exceptions.ReadTimeout:
                    print("ReadTimeout while waiting for DUT to respond")
        return False

    def check_acap_installed(self):
        """Uses the VAPIX Applications API to check if the ACAP is installed."""
        url = f"http://{AXIS_TARGET_ADDR}/axis-cgi/applications/list.cgi"
        if self.http_session:
            r = self.http_session.post(url)
            if r.status_code == 200:
                return r.text.find(ACAP_SPECIFIC_NAME) != -1
        return False

    def reboot_device(self, wait):
        """Uses VAPIX Firmware management API to reboot device."""
        url = f"http://{AXIS_TARGET_ADDR}/axis-cgi/firmwaremanagement.cgi"
        json_body = {"apiVersion": "1.4", "method":"reboot"}
        if self.http_session:
            r = self.http_session.post(url, json=json_body, timeout=10)
            if r.status_code == 200:
                response_dict = json.loads(r.text)
                if not "error" in response_dict.keys():
                    if wait != 0:
                        time.sleep(wait)
                    return True
        return False
