#! /usr/bin/env python2
import engine_wrapper
import unittest

class simpleStateTest(unittest.TestCase):
    def setUp(self):
        self.engine = engine_wrapper.Engine("SL_test")

    def tearDown(self):
        del self.engine

    def assertState(self, state, msg=None):
        self.assertEqual(self.engine.getState(), self.engine.states[state], msg)

