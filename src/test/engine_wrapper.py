import test_engine

class TwoWayDict(dict):
    def __len__(self):
        return dict.__len__(self) / 2

    def __setitem__(self, key, value):
        dict.__setitem__(self, key, value)
        dict.__setitem__(self, value, key)

class Engine (test_engine.TestEngine):
    def __init__(self, name):
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
                #,"REDO_TOG" XXX what effect do they have exactly?
                #,"QUANT_TOG"
                #,"ROUND_TOG"

        self.states = TwoWayDict() 
        for member in test_engine.__dict__.iteritems():
            if "LooperState" in member[0]:
                self.states[member[1]] = member[0][11:]

        self.auto_states = [  self.states["Unknown"]
                            , self.states["WaitStart"] 
                            , self.states["WaitStop"] 
                            , self.states["TriggerPlay"]
                            , self.states["OneShot"]
                            , self.states["Undo"]
                            , self.states["UndoAll"]
                            , self.states["Redo"]
                            , self.states["RedoAll"]
                                                         ]

        self.typical =   {
                 self.commands.index("REPLACE"   ): self.states["Replacing"]
                ,self.commands.index("REVERSE"   ): self.states["Playing"]
                ,self.commands.index("SCRATCH"   ): self.states["Scratching"]
                ,self.commands.index("RECORD"    ): self.states["Recording"]
                ,self.commands.index("OVERDUB"   ): self.states["Overdubbing"]
                ,self.commands.index("MULTIPLY"  ): self.states["Multiplying"]
                ,self.commands.index("INSERT"    ): self.states["Inserting"]
                ,self.commands.index("MUTE"      ): self.states["Muted"]
                ,self.commands.index("DELAY"     ): self.states["Delay"]
                ,self.commands.index("ONESHOT"   ): self.states["Playing"]#XXX or Off
                ,self.commands.index("TRIGGER"   ): self.states["Playing"]
                ,self.commands.index("SUBSTITUTE"): self.states["Substitute"]
                ,self.commands.index("UNDO_ALL"  ): self.states["Off"]
                ,self.commands.index("REDO_ALL"  ): self.states["Playing"]
                ,self.commands.index("MUTE_ON"   ): self.states["Muted"]
                ,self.commands.index("MUTE_OFF"  ): self.states["Playing"]
                ,self.commands.index("PAUSE"     ): self.states["Paused"]
                ,self.commands.index("PAUSE_ON"  ): self.states["Paused"]
                ,self.commands.index("PAUSE_OFF" ): self.states["Playing"]
                                                                               }

        #self.commands_per_state = {}
        #for key in self.states:
        #    if type(key) == int:
        #        self.commands_per_state[key] = [x for x in range(len(self.commands))]

        test_engine.TestEngine.__init__(self, name)

    def request(self, command):
          #print "   requesting: ", self.commands[command]
          command_no = self.commands.index(command)
          self.looper.request_cmd(command_no)
          while (self.looper.request_pending):
              pass
    def getState(self):
        return int(self.looper.get_control_value(test_engine.State))
        
