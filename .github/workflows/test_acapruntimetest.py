import json
import pytest
import requests
from requests.auth import HTTPDigestAuth
import time


AXIS_TARGET_ADDR="172.27.64.8"
AXIS_TARGET_USER="user"
AXIS_TARGET_PASS="pass"

class TestClassAcapRuntimeTest:

    http_session = None

    @pytest.fixture()
    def dut(self):
        print("\nSetup")
        yield "dut"
        print("\nTeardown")

    def setup_method(self):
        #print("Running setup")
        self.init_dut_connection()
        status = self.check_dut_status()
        assert status, f"Could not connect to {AXIS_TARGET_ADDR}"

    def init_dut_connection(self):
        self.http_session = requests.Session()
        self.http_session.auth = HTTPDigestAuth(AXIS_TARGET_USER, AXIS_TARGET_PASS)
        self.http_session.headers ={"Content-Type": "application/json; charset=utf-8"}
        return self.check_dut_status()

    def check_dut_status(self, timeout=120, max_time=120):
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
                    print(r.text)
                    if r.status_code == 200:
                        # check if request was ok
                        response_dict = json.loads(r.text)
                        if "data" in response_dict.keys():
                            if "systemready" in response_dict["data"].keys() and "needsetup" in response_dict["data"].keys():
                                if response_dict["data"]["systemready"] == "yes" and response_dict["data"]["needsetup"] == "no":
                                    return True
                except requests.exceptions.ReadTimeout:
                    print("Read timeout while waiting for DUT to respond")
        return False

    def test(self, dut):
        print(f"Testing {dut}")
        assert 1 == 1 , "This should always pass"
