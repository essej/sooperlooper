import time
import test_simple_state

class simpleStateOffMuted(test_simple_state.simpleStateTest):
    def setUp(self):
        test_simple_state.simpleStateTest.setUp(self)
        self.engine.request("MUTE")

    def testOffMuted(self):
        time.sleep(0.001)
        self.assertState("OffMuted")

    def testRecord(self):
        self.engine.request("RECORD")
        time.sleep(0.001)
        self.assertState("Recording")

    def testUndo(self):
        self.engine.request("RECORD")
        self.engine.request("UNDO")
        time.sleep(0.001)
        self.assertState("OffMuted")

    def testUndoAll(self):
        self.engine.request("RECORD")
        self.engine.request("UNDO_ALL")
        time.sleep(0.001)
        self.assertState("OffMuted")

    def testPause(self):
        self.engine.request("PAUSE")
        time.sleep(0.001)
        self.assertState("OffMuted")

    def testMute(self):
        self.engine.request("MUTE")
        time.sleep(0.001)
        self.assertState("Off")

    def testMute2(self):
        self.engine.request("MUTE")
        self.engine.request("MUTE")
        time.sleep(0.001)
        self.assertState("OffMuted")

