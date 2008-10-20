//
//  launch_slgui.mm
//  SooperLooperAU
//
//  Created by Jesse Chappell on 8/18/05.
//  Copyright 2005 __MyCompanyName__. All rights reserved.
//

#import "launch_slgui.h"

#include <iostream>
#include <vector>
#include <string>
using namespace std;


@implementation LaunchSLguiCocoa


- (id)init
{
	findRunning=NO;
    searchTask=nil;	
	//cerr << "launch init" << endl;
	return self;
}

- (void)dealloc
{
	// Release the memory for this wrapper object
	if (searchTask!=nil) {
		[searchTask stopProcess];
		[searchTask release];
		searchTask=nil;
	}
	//cerr << "launch dealloc" << endl;	
	[super dealloc];
}

- (void)runGui:(NSArray *)args
{
	if (findRunning)
	{
		
	}
	else
	{
		// If the task is still sitting around from the last run, release it
		if (searchTask!=nil)
			[searchTask release];
		// Let's allocate memory for and initialize a new TaskWrapper object, passing
		// in ourselves as the controller for this TaskWrapper object, the path
		// to the command-line tool, and the contents of the text field that 
		// displays what the user wants to search on
		searchTask=[[TaskWrapper alloc] initWithController:self arguments:args];
		// kick off the process asynchronously
		[searchTask startProcess];
	}
}

- (void)terminate
{
	if (findRunning && searchTask != nil)
	{
		// This stops the task and calls our callback (-processFinished)
		[searchTask stopProcess];
		// Release the memory for this wrapper object
		[searchTask release];
		searchTask=nil;
		return;
	}
}


// This callback is implemented as part of conforming to the ProcessController protocol.
// It will be called whenever there is output from the TaskWrapper.
- (void)appendOutput:(NSString *)output
{
    // do nothing
}

// A callback that gets called when a TaskWrapper is launched, allowing us to do any setup
// that is needed from the app side.  This method is implemented as a part of conforming
// to the ProcessController protocol.
- (void)processStarted
{
    findRunning=YES;
    //cerr << "slgui process started" << endl;    
}

// A callback that gets called when a TaskWrapper is completed, allowing us to do any cleanup
// that is needed from the app side.  This method is implemented as a part of conforming
// to the ProcessController protocol.
- (void)processFinished
{
    findRunning=NO;
    
	//cerr << "slgui process finished" << endl;
}

- (BOOL)isRunning
{
	return findRunning;
}

@end


LaunchSLgui::LaunchSLgui(string path, std::vector<std::string> args)
{
	launcher = [[LaunchSLguiCocoa alloc] init];
	
	_cmdpath = path;
	_args = args;
	
}

LaunchSLgui::~LaunchSLgui()
{
	if (launcher != nil) {
		[launcher release];
	}
}
	
bool LaunchSLgui::launch()
{
	NSMutableArray *cargs = [NSMutableArray array];
	
	if (launcher != nil) {
			// convert path and args to NSArray
		[cargs addObject:[NSString stringWithUTF8String:_cmdpath.c_str()]];
		// [cargs addObject:[[inputFile stringValue] lastPathComponent]];        
		for (vector<string>::iterator arg = _args.begin(); arg != _args.end(); ++arg)
		{
			/* set arguments */
			NSString *sarg = [NSString stringWithUTF8String:(*arg).c_str()];
			[cargs addObject:sarg];
		}
		
		[launcher runGui:cargs];
		
		return true;
	}
	
	return false;
}

bool LaunchSLgui::isRunning()
{
		return (bool) [launcher isRunning];
}

void LaunchSLgui::terminate()
{
	if (launcher != nil) {
		[launcher terminate];
	}
}

