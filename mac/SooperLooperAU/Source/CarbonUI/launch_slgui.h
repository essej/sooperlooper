//
//  launch_slgui.h
//  SooperLooperAU
//
//  Created by Jesse Chappell on 8/18/05.
//  Copyright 2005 Jesse Chappell. All rights reserved.
//
#ifndef __launch_slgui_h__
#define __launch_slgui_h__


#import <Cocoa/Cocoa.h>
#import "TaskWrapper.h"

//we conform to the ProcessController protocol, as defined in Process.h
@interface LaunchSLguiCocoa : NSObject <TaskWrapperController>
{
    BOOL findRunning;
    TaskWrapper *searchTask;
}
- (void)runGui:(NSArray *)args;
- (void)terminate;
- (BOOL)isRunning;
- (id)init; 
- (void)dealloc;
@end


#include <vector>
#include <string>

class LaunchSLgui
{
public:
	LaunchSLgui(std::string path, std::vector<std::string> args);
	virtual ~LaunchSLgui();
	
	bool launch();
	void terminate();

	bool isRunning();
	
	void set_path(std::string path) { _cmdpath = path; }
	std::string get_path() { return _cmdpath; }
	
	void set_args(std::vector<std::string> args) { _args = args; }
	
	
protected:
		
		id launcher; 
	
	std::string _cmdpath;
	std::vector<std::string> _args;
};


#endif