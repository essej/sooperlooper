import time
from nose_parameterized import parameterized

import testdef_simple_state
import engine_wrapper

class simpleStateTestsRecordUndoAll(testdef_simple_state.simpleStateTest):

    def setUp(self):
        testdef_simple_state.simpleStateTest.setUp(self)
        self.engine.request("RECORD")
        self.engine.request("RECORD")
        self.engine.request("UNDO_ALL")

    def testOff(self):
        time.sleep(0.001)
        self.assertState("Off")

    def testUndo(self):
        self.engine.request("RECORD")
        self.engine.request("UNDO")
        time.sleep(0.001)
        self.assertState("Off")

    def testUndoAll(self):
        self.engine.request("RECORD")
        self.engine.request("UNDO_ALL")
        time.sleep(0.001)
        self.assertState("Off")

    def testMuteOn(self):
        self.engine.request("MUTE_ON")
        time.sleep(0.001)
        self.assertState("OffMuted")

    def testDelay(self):
        self.engine.request("DELAY")
        time.sleep(0.001)
        self.assertState("Delay")

    def testRedo(self):
        self.engine.request("REDO")
        time.sleep(0.001)
        self.assertState("Playing")

    def testRedoAll(self):
        self.engine.request("REDO_ALL")
        time.sleep(0.001)
        self.assertState("Playing")

    @parameterized.expand([(c,) for c in engine_wrapper.commands if c not in [ "RECORD","DELAY","MUTE_ON","MUTE", "REDO", "REDO_ALL"]])
    def testAllOff(self, c):
        self.engine.request(c)
        time.sleep(0.001)
        self.assertState("Off")

