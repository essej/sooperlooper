#! /usr/bin/env python2
import testbed
import random
import time

def isListEmpty(inList):
    if isinstance(inList, list): # Is a list
        return all( map(isListEmpty, inList) )
    return False # Not a list

class Test ():
    def __init__(self):
        self.commands = (
                 "UNDO"
                ,"REDO" 
                ,"REPLACE" 
                ,"REVERSE" 
                ,"SCRATCH" 
                ,"RECORD" 
                ,"OVERDUB" 
                ,"MULTIPLY" 
                ,"INSERT" 
                ,"MUTE"
                )

        #self.states = (
        #         "OFF"
        #        ,"TRIG_START"
        #        ,"RECORD"
        #        ,"TRIG_STOP"
        #        ,"PLAY"
        #        ,"OVERDUB"
        #        ,"MULTIPLY"
        #        ,"INSERT"
        #        ,"REPLACE"
        #        ,"DELAY"
        #        ,"MUTE"
        #        ,"SCRATCH"
        #        ,"ONESHOT"
        #        )

        self.states = {}
        for member in testbed.__dict__.iteritems():
            if "LooperState" in member[0]:
                self.states[member[1]] = member[0]

        print self.states

        self.commands_per_state = {}
        for key in self.states:
            self.commands_per_state[key] = [x for x in range(len(self.commands))]

        #self.commands_per_state = [[x for x in range(len(self.commands))] for _ in self.states]
        self.TB = testbed.TestBed()  
        self.looper = self.TB.looper
        random.seed()

    def request(self, command):
          print "   requesting: ", self.commands[command]
          self.looper.request_cmd(command)
          while (self.looper.request_pending):
              pass
          #time.sleep(0.001)

    def run(self):
        current_state = int(self.looper.get_control_value(testbed.State))
        print "current state: ", self.states[current_state]
        if (self.commands_per_state[current_state] != []):
            command = self.commands_per_state[current_state].pop()
            self.request(command)
            return True
        else:
            if (isListEmpty(self.commands_per_state)):
                print "done"
                return False
            else:
                command = random.randint(0, len(self.commands) - 1 )
                self.request(command)
                return True



    def __del__(self):
        del self.TB

if __name__ == "__main__":
    t = Test()
    ret = t.run()
    while ret:
        ret = t.run()
    


