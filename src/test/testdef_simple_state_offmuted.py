import time
import testdef_simple_state

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

    def testAllOffMuted(self):
        self._testAll("OffMuted", ignore=[ "RECORD","DELAY","MUTE_OFF","MUTE"])

