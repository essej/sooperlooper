import test_engine

class TwoWayDict(dict):
    def __len__(self):
        return dict.__len__(self) / 2

    def __setitem__(self, key, value):
        dict.__setitem__(self, key, value)
        dict.__setitem__(self, value, key)

commands = (
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

states = TwoWayDict() 
for member in test_engine.__dict__.iteritems():
    if "LooperState" in member[0]:
        states[member[1]] = member[0][11:]


auto_states = [  states["Unknown"]
        , states["WaitStart"] 
        , states["WaitStop"] 
        , states["TriggerPlay"]
        , states["OneShot"]
        , states["Undo"]
        , states["UndoAll"]
        , states["Redo"]
        , states["RedoAll"]
        ]

typical =   [
        (commands.index("REPLACE"   ), states["Replacing"])
        ,(commands.index("REVERSE"   ), states["Playing"])
        ,(commands.index("SCRATCH"   ), states["Scratching"])
        ,(commands.index("RECORD"    ), states["Recording"])
        ,(commands.index("OVERDUB"   ), states["Overdubbing"])
        ,(commands.index("MULTIPLY"  ), states["Multiplying"])
        ,(commands.index("INSERT"    ), states["Inserting"])
        ,(commands.index("MUTE"      ), states["Muted"])
        ,(commands.index("DELAY"     ), states["Delay"])
        ,(commands.index("ONESHOT"   ), states["Playing"])#XXX or Off
        ,(commands.index("TRIGGER"   ), states["Playing"])
        ,(commands.index("SUBSTITUTE"), states["Substitute"])
        ,(commands.index("UNDO_ALL"  ), states["Off"])
        ,(commands.index("REDO_ALL"  ), states["Playing"])
        ,(commands.index("MUTE_ON"   ), states["Muted"])
        ,(commands.index("MUTE_OFF"  ), states["Playing"])
        ,(commands.index("PAUSE"     ), states["Paused"])
        ,(commands.index("PAUSE_ON"  ), states["Paused"])
        ,(commands.index("PAUSE_OFF" ), states["Playing"])
        ]

class Engine (test_engine.TestEngine):

    def request(self, command):
          #print "   requesting: ", self.commands[command]
          command_no = commands.index(command)
          self.looper.request_cmd(command_no)
          while (self.looper.request_pending):
              pass

    def getState(self):
        return int(self.looper.get_control_value(test_engine.State))
        
