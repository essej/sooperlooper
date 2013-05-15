import time
import testdef_simple_state

class simpleStateTestsAfterRecord(testdef_simple_state.simpleStateTest):
    def setUp(self):
        testdef_simple_state.simpleStateTest.setUp(self)
        self.engine.request("RECORD")
        self.engine.request("RECORD")

    def testRecord(self):
        time.sleep(0.001)
        self.assertState("Playing")

    def testOverdub(self):
        self.engine.request("OVERDUB")
        time.sleep(0.001)
        self.assertState("Overdubbing")

    def testOverdub2(self):
        self.engine.request("OVERDUB")
        self.engine.request("OVERDUB")
        time.sleep(0.001)
        self.assertState("Playing")

    def testUndo(self):
        self.engine.request("UNDO")
        time.sleep(0.001)
        self.assertState("Off")

    def testUndo2(self):
        self.engine.request("OVERDUB")
        self.engine.request("UNDO")
        time.sleep(0.001)
        self.assertState("Playing")

    def testUndo3(self):
        self.engine.request("OVERDUB")
        self.engine.request("OVERDUB")
        self.engine.request("UNDO")
        time.sleep(0.001)
        self.assertState("Playing")

    def testUndo4(self):
        self.engine.request("OVERDUB")
        self.engine.request("OVERDUB")
        self.engine.request("UNDO")
        self.engine.request("UNDO")
        self.engine.request("UNDO")
        self.engine.request("UNDO")
        time.sleep(0.001)
        self.assertState("Playing", "safety undo not enabled")

    def testUndoAll(self):
        self.engine.request("UNDO_ALL")
        time.sleep(0.001)
        self.assertState("Off")

    def testUndoAll2(self):
        self.engine.request("OVERDUB")
        self.engine.request("UNDO_ALL")
        time.sleep(0.001)
        self.assertState("Off")

    def testUndoAll3(self):
        self.engine.request("OVERDUB")
        self.engine.request("OVERDUB")
        self.engine.request("UNDO_ALL")
        time.sleep(0.001)
        self.assertState("Off")
