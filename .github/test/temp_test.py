import io
import json
import os
import re
import subprocess
import time

import pytest
import requests
from requests.auth import HTTPBasicAuth, HTTPDigestAuth

ACAP_SPECIFIC_NAME = "acapruntimetest"

ACAP_LOG_START_MATCH = "Running main() from"
ACAP_LOG_END_MATCH = "Global test environment tear-down"


def get_env(key):
    """Gets the value of the environment variable identified by key.
    Returns None if variable is not set.
    """
    if os.environ.__contains__(key):
        return os.environ[key]
    else:
        return None


def verify_required_env_variables():
        """Check that all required env variables are set.
        """
        response = []
        for env_var in ['AXIS_TARGET_ADDR','AXIS_TARGET_ARCH','AXIS_TARGET_USER','AXIS_TARGET_PASS','ACAP_DOCKER_IMAGE_NAME']:
            if not get_env(env_var):
                response.append(env_var)
        return response


def run_docker_cmd(cmd):
    """Runs a docker cmd.
    Returns True if successful, False otherwise.
    """
    response = subprocess.run(f"docker {cmd}", shell=True, capture_output=True)
    if response.returncode != 0:
        print(response.returncode)
        print(response.stderr)
        print(response.stdout)
        return False
    return True


def get_eap_file(docker_image_name, rel_path="/build"):
    """Tries to extract an .eap file from docker_image_name.
    If successful, the file is placed in the rel_path directory
    and the full path to the file is returned.
    """
    response = run_docker_cmd(
        f"cp $(docker create {docker_image_name}):/opt/app .{rel_path}"
    )
    if response:
        cwd = os.getcwd()
        for file in os.listdir(f"{cwd}{rel_path}"):
            if file.endswith(".eap"):
                eap_file = os.path.join(f"{cwd}{rel_path}", file)
                print(f"got acap file: {eap_file}")
                return eap_file
    return None


class TestClassAcapRuntimeTest:
    """Class for running pytest.
    Requires the following environment variables to be defined:
    AXIS_TARGET_ADDR - ip of device
    AXIS_TARGET_ARCH - expected arch of device
    AXIS_TARGET_USER - username on device/server
    AXIS_TARGET_PASS - password for user on device/server
    ACAP_DOCKER_IMAGE_NAME - name of docker image containing ACAP Runtime test suite
    AXIS_EXTERNAL_POOL - Set if device is on the external pool.
    """

    arch = None
    base_url = None
    http_session = None
    top_regex = re.compile(
        r"^.*\s\[ INFO\s*\]\sacapruntimetest\[\S*\]:\s\[\s*(?P<bracket>\S*)\s*\]\s*(?P<text>.*)$"
    )
    tests_executed_regex = re.compile(
        r"^Running (?P<nbr_tests_started>\d*) tests from (?P<nbr_test_suites_started>\d*) test suites.|"
        r"(?P<nbr_tests_executed>\d*) tests from (?P<nbr_test_suites_executed>\d*) test suites ran. \(\d* ms total\)$"
    )
    test_suites_executed_regex = re.compile(
        r"^(?P<nbr_tests>\d*) tests? from (?P<test_name>\S*).*$"
    )
    result = {"total": {"test_suites": 0, "tests": 0, "executed": 0, "passed": 0}}

    @pytest.fixture()
    def dut(self):
        # Setup
        yield "dut"
        # Teardown

    def setup_method(self):
        print("\n****Setup****")

        print(f"AXIS_TARGET_ADDR: {get_env('AXIS_TARGET_ADDR')}")
        print(f"AXIS_EXTERNAL_POOL: {get_env('AXIS_EXTERNAL_POOL')}")
        self.arch = get_env('AXIS_TARGET_ARCH')
        print(f"AXIS_TARGET_ARCH: {self.arch}")
        self.init_dut_connection()
        status = self.check_dut_status()
        assert status, f"Could not connect to {get_env('AXIS_TARGET_ADDR')}"
        print("Get properties of DUT")
        properties = self.get_dut_info()
        print(properties)


    def teardown_method(self):
        print("\n****Teardown****")
        print("Stopping ACAP runtime test suite")


    def test_method(self, dut):
        print(f"****Testing ****")



    # --------- ACAP runtime test suite specific methods -----------------------

    def check_acap_test_suite_finished(self, max_time_in_minutes=3):
        """Wait for the ACAP runtime test suite to finish.
        Fail if the execution takes more than \"timeout\" minutes.
        """
        max_time = max_time_in_minutes * 60.0
        start_time = time.time()
        time.sleep(30)
        while True:
            wait_time = time.time() - start_time
            if wait_time > max_time:
                return False
            count = self.find_match_in_log(ACAP_LOG_END_MATCH)
            if count == 1:
                return True
            time.sleep(5)

    def evaluate_acap_runtime_test_suite_result(self):
        """Parse the ACAP Runtime test suite log to see if the tests passed."""

        test_log = self.read_acap_log()
        [self.parse_line(line) for line in test_log.splitlines()]

        if self.result["total"]["passed"] != self.result["total"]["tests"]:
            print("---------Test Suite Failed-------------")
            print(
                f"{self.result['total']['passed']} of \
                {self.result['total']['tests']} tests from \
                {self.result['total']['test_suites']} test suites passed."
            )
            return False

        print("---------Test Suite Passed-------------")
        print(
            f"Ran {self.result['total']['tests']} tests from \
            {self.result['total']['test_suites']} test suites."
        )
        return True

    def parse_line(self, line):
        """Parses contents of a line from the application log and updates the
        result dict with data from the test run."""
        m1 = self.top_regex.match(line)
        if m1:
            bracket = m1.group("bracket")
            text = m1.group("text")
            if "==" in bracket:
                m = self.tests_executed_regex.match(text)
                if m:
                    tmp_total = self.result["total"]
                    if tmp_total["test_suites"] == 0 and m.group("nbr_test_suites_started"):
                        tmp_total.update({"test_suites": m.group("nbr_test_suites_started")})
                    if tmp_total["tests"] == 0 and m.group("nbr_tests_started"):
                        tmp_total.update({"tests": m.group("nbr_tests_started")})
                    if tmp_total["executed"] == 0 and m.group("nbr_tests_executed"):
                        tmp_total.update({"executed": m.group("nbr_tests_executed")})
                    self.result.update({"total": tmp_total})
            elif "--" in bracket:
                m = self.test_suites_executed_regex.match(text)
                if m:
                    tmp_dict = {"nbr": 0, "RUN": [], "OK": [], "FAILED": []}
                    if self.result.__contains__(m.group("test_name")):
                        tmp_dict = self.result[m.group("test_name")]
                    if tmp_dict["nbr"] == 0:
                        tmp_dict.update({"nbr": m.group("nbr_tests")})
                    self.result.update({m.group("test_name"): tmp_dict})
            elif bracket in ["RUN", "OK", "FAILED"]:
                m = re.search(r"^(\S+)\.(\S+).*$", text)
                if m:
                    test_suite_name = m.groups()[0]
                    test_name = m.groups()[1]
                    tmp_dict = {"nbr": 0, "RUN": [], "OK": [], "FAILED": []}
                    if self.result.__contains__(test_suite_name):
                        tmp_dict = self.result[test_suite_name]
                    if not tmp_dict[bracket].__contains__(test_name):
                        tmp_list = tmp_dict[bracket]
                        tmp_list.append(test_name)
                        tmp_dict.update({bracket: tmp_list})
                        self.result.update({test_suite_name: tmp_dict})
            elif bracket == "PASSED":
                nbr_passed = re.search(r"\d+", text).group()
                tmp_dict = self.result["total"]
                tmp_dict.update({"passed": nbr_passed})
                self.result.update({"total": tmp_dict})

    # ----------- Support methods ------------------------------------------------

    def init_dut_connection(self):
        """Initiates a requests Session to be used for http communication with the DUT."""
        self.http_session = requests.Session()
        if get_env("AXIS_EXTERNAL_POOL"):
            print("Using HttpBasicAuth")
            self.http_session.auth = HTTPBasicAuth(
                get_env("AXIS_TARGET_USER"), get_env("AXIS_TARGET_PASS")
            )
        else:
            print("Using HTTPDigestAuth")
            self.http_session.auth = HTTPDigestAuth(
                get_env("AXIS_TARGET_USER"), get_env("AXIS_TARGET_PASS")
            )
        self.http_session.headers = {"Content-Type": "application/json; charset=utf-8"}
        self.base_url = f"http://{get_env('AXIS_TARGET_ADDR')}"
        # return self.check_dut_status()

    def check_dut_status(self, timeout=120, max_time=120):
        """Uses the VAPIX Systemready API to check that the DUT is responding."""
        url = f"{self.base_url}/axis-cgi/systemready.cgi"
        json_body = {
            "apiVersion": "1.2",
            "method": "systemready",
            "params": {"timeout": 20}
        }
        if self.http_session:
            # print(f"Check DUT status. This can take some time, time out set to {max_time} seconds.")
            start_time = time.time()
            while True:
                if time.time() - start_time > max_time:
                    print("DUT Status check timed out")
                    return False
                try:
                    r = self.http_session.post(url, json=json_body, timeout=timeout)
                    if r.status_code == 200:
                        # check if request was ok
                        response_dict = json.loads(r.text)
                        if "data" in response_dict.keys():
                            data_dict = response_dict["data"]
                            if (
                                "systemready" in data_dict.keys()
                                and "needsetup" in data_dict.keys()
                            ):
                                if (
                                    data_dict["systemready"] == "yes"
                                    and data_dict["needsetup"] == "no"
                                ):
                                    return True
                except requests.exceptions.ReadTimeout:
                    print("ReadTimeout while waiting for DUT to respond")
        return False

    def get_dut_info(self):
        """Uses the VAPIX Basic Device Info API to get the properties of the DUT.
        """
        url = f"{self.base_url}/axis-cgi/basicdeviceinfo.cgi"
        json_body = {
            "apiVersion": "1.0",
            "method": "getAllProperties"
        }
        if self.http_session:
            r = self.http_session.post(url, json=json_body)
            if r.status_code == 200:
                response_dict = json.loads(r.text)
                if not "error" in response_dict.keys() and "data" in response_dict.keys():
                    return response_dict["data"]["propertyList"]
        return None

    def acap_ctrl(self, action, wait=0):
        """Static method for controlling ACAP application via docker."""
        docker_image_name = get_env("ACAP_DOCKER_IMAGE_NAME")
        if not get_env("AXIS_EXTERNAL_POOL"):
            # we  can use docker for control
            device_ip = get_env("AXIS_TARGET_ADDR")
            device_pass = get_env("AXIS_TARGET_PASS")

            if not run_docker_cmd(
                f"run --rm {docker_image_name} {device_ip} {device_pass} {action}"
            ):
                print(f"failed to install the app")
                return False
        else:
            if action == "install":
                print('acap_ctrl, action: install')
                eapfile = get_eap_file(docker_image_name)
                if eapfile == None:
                    return False
                url = f"{self.base_url}/axis-cgi/applications/upload.cgi"
                upload_eapfile = {'file': io.open(eapfile, "rb")}
                r = self.http_session.post(url, files = upload_eapfile)
                if r.status_code != 200:
                    print(r.status_code)
                    print(r.text)
                    return False
            elif action in ["start", "stop", "remove"]:
                url = f"{self.base_url}/axis-cgi/applications/control.cgi?action={action}&package={ACAP_SPECIFIC_NAME}"
                r = self.http_session.post(url)
                if r.status_code != 200:
                    return False
            else:
                print(f"action, {action}, not supported.")
                return False
        if wait != 0:
            print(f"wait for {wait} seconds")
            time.sleep(wait)
        return True

    def check_acap_installed(self):
        """Uses the VAPIX Applications API to check if the ACAP is installed."""
        url = f"http://{get_env('AXIS_TARGET_ADDR')}/axis-cgi/applications/list.cgi"
        if self.http_session:
            r = self.http_session.post(url)
            if r.status_code == 200:
                return r.text.find(ACAP_SPECIFIC_NAME) != -1
        return False

    def reboot_device(self, wait):
        """Uses VAPIX Firmware management API to reboot device."""
        url = f"http://{get_env('AXIS_TARGET_ADDR')}/axis-cgi/firmwaremanagement.cgi"
        json_body = {"apiVersion": "1.4", "method": "reboot"}
        if self.http_session:
            r = self.http_session.post(url, json=json_body, timeout=10)
            if r.status_code == 200:
                response_dict = json.loads(r.text)
                if not "error" in response_dict.keys():
                    if wait != 0:
                        print(f"Waiting {wait} seconds for reboot to start.")
                        time.sleep(wait)
                    return True
        return False

    def read_acap_log(self):
        """Reads ACAP part of system log and returns it as as string."""
        url = f"http://{get_env('AXIS_TARGET_ADDR')}/axis-cgi/admin/systemlog.cgi?appname={ACAP_SPECIFIC_NAME}"
        if self.http_session:
            r = self.http_session.get(url)
            return r.text

    def save_acap_log(self, path):
        """Save the test log to disk.
        """
        test_log = self.read_acap_log()
        log_file = open(path,"w")
        log_file.write(test_log)
        log_file.close()

    def find_match_in_log(self, pattern):
        """Check how many times pattern appears in the application log"""
        log_string = str(self.read_acap_log())
        return log_string.count(pattern)
