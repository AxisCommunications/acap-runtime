import pytest

class TestClassAcapRuntimeTest:

    @pytest.fixture()
    def dut(self):
        print("setup")
        yield "dut"
        print("teardown")

    def test(self, dut):
        print(f"Testing {dut}")
