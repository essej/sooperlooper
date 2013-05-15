import time
import test_simple_state

class simpleStateOff(test_simple_state.simpleStateTest):

    def testOff(self):
        self.assertState("Off")

    def testRecord(self):
        self.engine.request("RECORD")
        time.sleep(0.001)
        self.assertState("Recording")

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

    def testPause(self):
        self.engine.request("PAUSE")
        time.sleep(0.001)
        self.assertState("Off")

