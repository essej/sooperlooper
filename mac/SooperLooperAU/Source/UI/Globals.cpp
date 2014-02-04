//
//  Globals.c
//  SooperLooperAU
//
//  Created by Jesse Chappell on 1/16/14.
//
//


#include "pbd/error.h"

// hack needed for pbd/midi stuff
Transmitter  warning (Transmitter::Warning);
Transmitter  error (Transmitter::Error);
Transmitter  fatal (Transmitter::Fatal);
Transmitter  info (Transmitter::Info);

