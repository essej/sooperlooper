#! /usr/bin/env python2
import testbed
import random
import time

def isListEmpty(inList):
    if isinstance(inList, list): # Is a list
        return all( map(isListEmpty, inList) )
    return False # Not a list

class TwoWayDict(dict):
    def __len__(self):
        return dict.__len__(self) / 2

    def __setitem__(self, key, value):
        dict.__setitem__(self, key, value)
        dict.__setitem__(self, value, key)

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
                ,"DELAY"
                ,"REDO_TOG"
                ,"QUANT_TOG"
                ,"ROUND_TOG"
                ,"ONESHOT"
                ,"TRIGGER"
                ,"SUBSTITUTE"
                ,"UNDO_ALL"
                ,"REDO_ALL"
                ,"MUTE_ON"
                ,"MUTE_OFF"
                ,"PAUSE"
                ,"PAUSE_ON"
                ,"PAUSE_OFF"
                )

        self.states = TwoWayDict() 
        for member in testbed.__dict__.iteritems():
            if "LooperState" in member[0]:
                self.states[member[1]] = member[0][11:]

        self.ignore_states = [ self.states["Unknown"]
                             , self.states["WaitStart"] 
                             , self.states["WaitStop"] 
                             , self.states["TriggerPlay"]
                             , self.states["OneShot"]
                             , self.states["Undo"]
                             , self.states["UndoAll"]
                             , self.states["Redo"]
                             , self.states["RedoAll"]
                             , self.states["Scratching"]
                             ]

        self.commands_per_state = {}
        for key in self.states:
            if type(key) == int:
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
          #time.sleep(0.0001)

    def run(self):
        current_state = int(self.looper.get_control_value(testbed.State))
        print "current state: ", self.states[current_state]
        if (self.commands_per_state[current_state] != []):
            command = self.commands_per_state[current_state].pop()
            self.request(command)
            return True
        else:
            for key, item in self.commands_per_state.iteritems():
                if len(item) > 0 and (key not in self.ignore_states):
                    break
            else:
                return False

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
    


