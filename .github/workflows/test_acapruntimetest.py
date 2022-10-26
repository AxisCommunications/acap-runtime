import json
import os
import pytest
import re
import requests
from requests.auth import HTTPDigestAuth
import subprocess
import time

ACAP_SPECIFIC_NAME = "acapruntimetest"

ACAP_LOG_START_MATCH = "Running main() from"
ACAP_LOG_END_MATCH = "Global test environment tear-down"

def get_env(key):
    if os.environ.__contains__(key):
        return os.environ[key]
    else:
        print(f"{key} not found")

def acap_ctrl(action, wait = 0,
        device_ip=get_env('AXIS_TARGET_ADDR'),
        device_pass=get_env('AXIS_TARGET_PASS'),
        docker_image_name=get_env('ACAP_DOCKER_IMAGE_NAME')):
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
    top_regex = re.compile(r'^.*\s\[ INFO\s*\]\sacapruntimetest\[\S*\]:\s\[\s*(?P<bracket>\S*)\s*\]\s*(?P<text>.*)$')
    tests_executed_regex = re.compile(
        r'^Running (?P<nbr_tests_started>\d*) tests from (?P<nbr_test_suites_started>\d*) test suites.|'
        r'(?P<nbr_tests_executed>\d*) tests from (?P<nbr_test_suites_executed>\d*) test suites ran. \(\d* ms total\)$')
    test_suites_executed_regex = re.compile(r'^(?P<nbr_tests>\d*) tests? from (?P<test_name>\S*).*$')
    result = {'total': {'test_suites': 0, 'tests': 0, 'executed': 0, 'passed': 0}}


    @pytest.fixture()
    def dut(self):
        # Setup
        yield "dut"
        # Teardown

    def setup_method(self):
        print("\n****Setup****")
        self.init_dut_connection()
        status = self.check_dut_status()
        assert status, f"Could not connect to {get_env('AXIS_TARGET_ADDR')}"
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
        print("Start ACAP Runtime test suite")
        acap_ctrl("start", 1)
        running_count = self.find_match_in_log(ACAP_LOG_START_MATCH)
        assert running_count == 1, "The log should indicate that the ACAP Runtime\
             test suite was started once, but {running_count} matches were found"
        print("Wait for ACAP Runtime test suite to finish execution.")
        test_suite_finished = self.check_acap_test_suite_finished()
        assert test_suite_finished, "ACAP runtime test suite execution timed out."
        print("Evaluate ACAP Runtime test suite result.")
        test_suite_passed = self.evaluate_acap_runtime_test_suite_result()
        assert test_suite_passed, "ACAP runtime test suite failed."

#----------- Support methods ------------------------------------------------

    def init_dut_connection(self):
        """Initiates a requests Session to be used for http communication with the DUT."""
        self.http_session = requests.Session()
        self.http_session.auth = HTTPDigestAuth(get_env('AXIS_TARGET_USER'), get_env('AXIS_TARGET_PASS'))
        self.http_session.headers ={"Content-Type": "application/json; charset=utf-8"}
        return self.check_dut_status()

    def check_dut_status(self, timeout=120, max_time=120):
        """Uses the VAPIX Systemready API to check that the DUT is responding."""
        url = f"http://{get_env('AXIS_TARGET_ADDR')}/axis-cgi/systemready.cgi"
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
        url = f"http://{get_env('AXIS_TARGET_ADDR')}/axis-cgi/applications/list.cgi"
        if self.http_session:
            r = self.http_session.post(url)
            if r.status_code == 200:
                return r.text.find(ACAP_SPECIFIC_NAME) != -1
        return False

    def reboot_device(self, wait):
        """Uses VAPIX Firmware management API to reboot device."""
        url = f"http://{get_env('AXIS_TARGET_ADDR')}/axis-cgi/firmwaremanagement.cgi"
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

    def read_acap_log(self):
        """Reads ACAP part of system log and returns it as as string."""
        url = f"http://{get_env('AXIS_TARGET_ADDR')}/axis-cgi/admin/systemlog.cgi?appname={ACAP_SPECIFIC_NAME}"
        if self.http_session:
            r = self.http_session.get(url)
            return r.text

    def find_match_in_log(self, pattern):
        """Check how many times pattern appears in the application log"""
        log_string = str(self.read_acap_log())
        return log_string.count(pattern)

    def check_acap_test_suite_finished(self, max_time_in_minutes=3):
        """Wait for the ACAP runtime test suite to finish.
        Fail if the execution takes more than \"timeout\" minutes.
        """
        max_time = max_time_in_minutes*60.0
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

        if self.result['total']['passed'] != self.result['total']['tests']:
            print("---------Test Suite Failed-------------")
            print(f"{self.result['total']['passed']} of \
                {self.result['total']['tests']} tests from \
                {self.result['total']['test_suites']} test suites passed.")
            return False

        print("---------Test Suite Passed-------------")
        print(f"Ran {self.result['total']['tests']} tests from \
            {self.result['total']['test_suites']} test suites.")
        return True

    def parse_line(self, line):
        """Parses contents of a line from the application log and updates the
            result dict with data from the test run."""
        m1 = self.top_regex.match(line)
        if m1:
            bracket = m1.group('bracket')
            text = m1.group('text')
            if '==' in bracket:
                m = self.tests_executed_regex.match(text)
                if m:
                    tmp_total = self.result['total']
                    if tmp_total['test_suites'] == 0 and m.group('nbr_test_suites_started'):
                        tmp_total.update({'test_suites': m.group('nbr_test_suites_started')})
                    if tmp_total['tests'] == 0 and m.group('nbr_tests_started'):
                        tmp_total.update({'tests': m.group('nbr_tests_started')})
                    if tmp_total['executed'] == 0 and m.group('nbr_tests_executed'):
                        tmp_total.update({'executed': m.group('nbr_tests_executed')})
                    self.result.update({'total': tmp_total})
            elif '--' in bracket:
                m = self.test_suites_executed_regex.match(text)
                if m:
                    tmp_dict = {'nbr': 0, 'RUN': [], 'OK': [], 'FAILED': []}
                    if self.result.__contains__(m.group('test_name')):
                        tmp_dict = self.result[m.group('test_name')]
                    if tmp_dict['nbr'] == 0:
                        tmp_dict.update({'nbr': m.group('nbr_tests')})
                    self.result.update({m.group('test_name'): tmp_dict})
            elif bracket in ["RUN", "OK", "FAILED"]:
                m = re.search(r'^(\S+)\.(\S+).*$', text)
                if m:
                    test_suite_name = m.groups()[0]
                    test_name = m.groups()[1]
                    tmp_dict = {'nbr': 0, 'RUN': [], 'OK': [], 'FAILED': []}
                    if self.result.__contains__(test_suite_name):
                        tmp_dict = self.result[test_suite_name]
                    if not tmp_dict[bracket].__contains__(test_name):
                        tmp_list = tmp_dict[bracket]
                        tmp_list.append(test_name)
                        tmp_dict.update({bracket: tmp_list})
                        self.result.update({test_suite_name: tmp_dict})
            elif bracket == "PASSED":
                nbr_passed = re.search(r'\d+', text).group()
                tmp_dict = self.result['total']
                tmp_dict.update({'passed': nbr_passed})
                self.result.update({'total': tmp_dict})