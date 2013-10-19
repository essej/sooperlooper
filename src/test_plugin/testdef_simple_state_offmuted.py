import time
from nose_parameterized import parameterized

import testdef_simple_state
import engine_wrapper


class simpleStateOffMuted(testdef_simple_state.simpleStateTest):
    def setUp(self):
        testdef_simple_state.simpleStateTest.setUp(self)
        self.engine.request("MUTE")

    def testOffMuted(self):
        time.sleep(0.001)
        self.assertState("OffMuted")

    def testMute(self):
        self.engine.request("MUTE")
        time.sleep(0.001)
        self.assertState("Off")

    def testMuteOff(self):
        self.engine.request("MUTE_OFF")
        time.sleep(0.001)
        self.assertState("Off")

    def testDelay(self):
        self.engine.request("DELAY")
        time.sleep(0.001)
        self.assertState("Delay")

    @parameterized.expand([(c,) for c in engine_wrapper.commands if c not in [ "RECORD","DELAY","MUTE_OFF","MUTE"]])
    def testAllOffMuted(self, c):
        self.engine.request(c)
        time.sleep(0.001)
        self.assertState("OffMuted")

