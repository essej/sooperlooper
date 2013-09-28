#! /usr/bin/env python2
import time
import unittest

import engine_wrapper


class simpleStateTest(unittest.TestCase):

    def setUp(self):
        self.engine = engine_wrapper.Engine("SL_test")
        self.maxDiff = None
        #print "set-up"

    def tearDown(self):
        del self.engine

    def assertState(self, state, msg=None):
       actual_state = self.engine.getState()
       self.assertEqual(actual_state, engine_wrapper.states[state], engine_wrapper.states[actual_state] + " != " +  state if msg is None else msg)

    def testRecord(self):
        #RECORD should always results in a recording state no matter the current state
        self.engine.request("RECORD")
        time.sleep(0.001)
        self.assertState("Recording")

