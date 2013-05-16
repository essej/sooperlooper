#! /usr/bin/env python2
import time
import unittest

import engine_wrapper

class simpleStateTest(unittest.TestCase):

    def setUp(self):
        self.engine = engine_wrapper.Engine("SL_test")
        self.maxDiff = None

    def tearDown(self):
        del self.engine

    def assertState(self, state, msg=None):
       actual_state = self.engine.getState()
       self.assertEqual(actual_state, self.engine.states[state], self.engine.states[actual_state] + " != " +  state if msg is None else msg)

    def _testAll(self, should_be_state_string, ignore=None):
        should_be_state = self.engine.states[should_be_state_string]
        if ignore is None:
            ignore = []
        should_be = {}
        actual = {}
        for c in self.engine.commands:
            if c not in ignore:
                should_be[c] = should_be_state_string
                self.engine.request(c)
                time.sleep(0.001)
                actual[c] = self.engine.states[self.engine.getState()]
        self.assertDictEqual(should_be, actual)

    def _testAllTypical(self, ignore=None):
        if ignore is None:
            ignore = []
        should_be = {}
        actual = {}

    def testRecord(self):
        #RECORD should always results in a recording state no matter the current state
        self.engine.request("RECORD")
        time.sleep(0.001)
        self.assertState("Recording")

