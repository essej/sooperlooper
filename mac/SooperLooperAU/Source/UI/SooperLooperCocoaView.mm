
 
/*
    SooperLooperCocoaView.mm
    
    View class manufactured by SooperLooperCocoaViewFactory factory class.
    This view is instantiated via nib.
*/

#import "SooperLooperCocoaView.h"

/*
enum {
	kParam_One,
	kParam_Two,
    kParam_Three_Indexed,
	kNumberOfParameters
};
*/

#include <string>

#include <iostream>
#include <fstream>
#include <cstdlib>

#include <sys/types.h>
#include <sys/stat.h>

#include "launch_slgui.h"

#include "CAAUParameter.h"

using namespace std;

#include "SLproperties.h"

#define kDefaultSlguiPath "/Applications/SooperLooper.app"

#define kSLstartCmd 'Strt'
#define kSLbrowseCmd 'Brws'
#define kSLstayOnTopCmd 'Otop'

/*
 AudioUnitParameter parameter[] = {	{ 0, kParam_One, kAudioUnitScope_Global, 0 },
                                    { 0, kParam_Two, kAudioUnitScope_Global, 0 },
                                    { 0, kParam_Three_Indexed, kAudioUnitScope_Global, 0 }	};
*/

static bool gWxIsInitialized = false;


#include "plugin_app.hpp"
#include "main_panel.hpp"
#include "app_frame.hpp"
#include "version.h"
#include "loop_control.hpp"

#include <wx/init.h>
#include <wx/evtloop.h>

@implementation SooperLooperCocoaView


-(void)init_app_path
{
	string guipath;
	
	// use non-empty value from AU property first
	AudioUnit slau = mAU;
	UInt32 datasize;
	Boolean writable;
	if (AudioUnitGetPropertyInfo(slau, kSLguiAppPathProperty, kAudioUnitScope_Global, 0, &datasize, &writable) == noErr)
	{
		char * tmpbuf = new char[datasize + 1];
		AudioUnitGetProperty(slau, kSLguiAppPathProperty, kAudioUnitScope_Global, 0, tmpbuf, &datasize);
		tmpbuf[datasize] = '\0';
		guipath = tmpbuf;
		delete [] tmpbuf;
	}
	
	if (!guipath.empty()) {
		_slapp_path = [NSString stringWithUTF8String:guipath.c_str()];
		return;
	}
	
	// now try loading the default from a pref file
	// ~/.sooperlooper/default_app_path
	char * homedir = getenv("HOME");
	if (homedir) {
		char line[500];
		string defpath(homedir);
		defpath += "/.sooperlooper/default_app_path";
		ifstream defappstream(defpath.c_str());
		if (defappstream.is_open() && !defappstream.eof()) {
			defappstream.getline (line, sizeof(line));
			size_t len = strlen(line);
			if (line[len-1] == '\n') line[len-1] = '\0';
			guipath = line;
			defappstream.close();
		}
	}
    
	if (!guipath.empty()) {
		_slapp_path = [NSString stringWithUTF8String:guipath.c_str()];
		return;
	}
	
	// finally default to hardcoded value
	_slapp_path = @"/Applications/SooperLooper.app";
	
}

-(void) set_app_path_property:(NSString *) guipath
{
	AudioUnit slau = mAU;
	UInt32 datasize;
	Boolean writable;
	if (AudioUnitGetPropertyInfo(slau, kSLguiAppPathProperty, kAudioUnitScope_Global, 0, &datasize, &writable) == noErr)
	{
		if (writable) {
			AudioUnitSetProperty(slau, kSLguiAppPathProperty, kAudioUnitScope_Global, 0, [guipath UTF8String], (UInt32)[guipath length]);
		}
	}
	
	// also write default into file
	char * homedir = getenv("HOME");
	if (homedir) {
		struct stat st;
		string defpath(homedir);
		defpath += "/.sooperlooper";
		// create dir if necessary
		if (::stat(defpath.c_str(), &st) != 0) {
			::mkdir(defpath.c_str(), 0755);
		}
		defpath += "/default_app_path";
		ofstream defappstream(defpath.c_str());
		if (defappstream.is_open()) {
			defappstream.write ([guipath UTF8String], [guipath length]);
			defappstream.write("\n", 1);
			defappstream.close();
		}
	}
}

-(void) init_stay_on_top
{
	short value = 0;
	
	// use non-empty value from AU property first
	AudioUnit slau = mAU;
	UInt32 datasize = sizeof(short);
	Boolean writable;
	if (AudioUnitGetPropertyInfo(slau, kSLguiStayOnTopProperty, kAudioUnitScope_Global, 0, &datasize, &writable) == noErr)
	{
		if (AudioUnitGetProperty(slau, kSLguiStayOnTopProperty, kAudioUnitScope_Global, 0, &value, &datasize) == noErr)
		{
			_stay_on_top = value;
			return;
		}
	}
	
	// now try loading the default from a pref file
	// ~/.sooperlooper/default_stay_on_top
	char * homedir = getenv("HOME");
	if (homedir) {
		char line[500];
		string defpath(homedir);
		defpath += "/.sooperlooper/default_stay_on_top";
		ifstream defappstream(defpath.c_str());
		if (defappstream.is_open() && !defappstream.eof()) {
			defappstream.getline (line, sizeof(line));
			size_t len = strlen(line);
			if (line[len-1] == '\n') line[len-1] = '\0';
			if (sscanf(line, "%hd", &value) == 1) {
				_stay_on_top = value;
			}
			defappstream.close();
		}
	}
	
}


- (void)windowBecameKey:(NSNotification *)notification
{
    wxApp::SetInstance( _app );
    
    // [self windowResized:notification];
}

- (void)windowMoved:(NSNotification *)notification
{
    [self windowResized:notification];
}

- (void)windowResized:(NSNotification *)notification
{
    NSPoint loc = [[self window] frame].origin;
    NSSize size = [[self window] frame].size;
    
    NSSize selfsize = [self frame].size;
    NSPoint selfloc = [self frame].origin;
    
    NSSize screensize = ((NSScreen*)[[NSScreen screens] objectAtIndex:0]).frame.size;
    
    NSLog(@"pos: %g  %g  window width = %f, window height = %f  selfframe: %g %g %g %g  screensize: %g %g", loc.x, loc.y, size.width,
          size.height, self.frame.origin.x, self.frame.origin.y, self.frame.size.width, self.frame.size.height, screensize.width, screensize.height);
    
    if (_appframe) {
        //wxFrame * frame = _app->get_main_frame();
        
        float heightDelta = (size.height - selfsize.height);
        float ourHeight = selfsize.height;
        
        _appframe->SetSize(size.width - 4, ourHeight - 4);
        //frame->Layout();
        _appframe->SetPosition(wxPoint(loc.x + 2, ((screensize.height - loc.y) - size.height) + heightDelta + 2));
        _appframe->Raise();
        

    }
    
}

- (void)viewDidMoveToWindow
{
    NSLog(@"Move to window");
    if (!_appframe) {
#ifdef DO_EMBEDDED
        [self performSelector:@selector(startEmbeddedUI) withObject:NULL afterDelay:0.5];
#endif
        //[self startEmbeddedUI];
    }
}

-(void)viewDidHide
{
    NSLog(@"Viewdidhide");
    if (_appframe) {
        _appframe->Show(false);
    }
}

-(void)viewDidUnhide
{
    NSLog(@"ViewdidUnhide");
    if (_appframe) {
        _appframe->Show(true);
        [self windowResized:NULL];
    }
}

-(void) disposeEmbeddedGui
{
    
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
    if (_appframe) {
        
#if 1
        
        //if (_guarantor) {
        //    delete _guarantor;
        //    _guarantor = NULL;
        //}
        if (_eventloop) {
            delete _eventloop;
            _eventloop = NULL;
        }
        
        //wxApp::SetInstance( NULL );
        //wxFrame * frame = _app->get_main_frame();
        //frame->Destroy();
        //_appframe->Destroy();
        delete _appframe;
        
        _app->cleanup_stuff();
        
        // CANNOT CLEANUP SAFELY due to wx issues.. KNOWN LEAK!
        if (wxApp::GetInstance() == _app) {
            wxApp::SetInstance( NULL );
        }
        
        //_app->CleanUp();
        // delete _app;
        
        //   wxUninitialize();
        
        
        _app = NULL;
        _appframe = NULL;
        
        //SooperLooperGui::MainPanel * panel = _app->get_main_panel();
        //NSView * panelv = ((NSView*)panel->GetHandle());
        //[panelv removeFromSuperview];
        
        //_app->get_main_frame()->AddChild(_app->get_main_panel());
#else
        wxFrame * frame = _app->get_main_frame();
        frame->Hide();
#endif
    }
    
}


-(void) startEmbeddedUI
{
    wxApp::sm_isEmbedded = true;

    [self disposeEmbeddedGui];
    
    if (!_appframe) {
        _app = new SooperLooperGui::PluginApp();
        
        _app->set_stay_on_top((bool)_stay_on_top);
        
        //wxApp::SetInstance( _app );
    
        vector<string> args;
        
        CAAUParameter auvp(mAU,  kParam_OSCPort, kAudioUnitScope_Global, 0);
        _app->set_port((int) auvp.GetValue());
        
        
        
        /*
        std::vector<char *> argv(args.size() + 2);    // one extra for the null
        argv[0] = "SooperLooper UI";
        
        for (std::size_t i = 0; i != args.size(); ++i)
        {
            argv[i+1] = &args[i][0];
        }
        
        int argc = (int) (args.size() + 1);
        */
         
        NSLog(@"About to call initialize on wx: %p", _app);
        
        
        //wxEntryStart(argc, argv.data());
        if (!gWxIsInitialized) {

            wxInitialize();
            gWxIsInitialized = true;
        }
        
        NSLog(@"Called entry start on wx: %p  %p", _app, wxTheApp);
        
        /*
        _appframe = new SooperLooperGui::AppFrame(wxString::Format(wxT("SooperLooper v %s"), wxString::FromAscii(sooperlooper_version).c_str()), wxPoint(100, 100), wxDefaultSize, false, true);
        // Show it and tell the application that it's our main window
        _appframe->SetSizeHints(850, 210);
        _appframe->SetSize(860, 215);
        //SetTopWindow(_frame);
        _appframe->Show(FALSE);
        _appframe->Raise();
        _appframe->Show(TRUE);
        
        
        // override defaults
        SooperLooperGui::LoopControl & loopctrl = _appframe->get_main_panel()->get_loop_control();
        
        loopctrl.get_spawn_config().never_spawn = true;
        loopctrl.get_spawn_config().force_spawn = false;

        loopctrl.get_spawn_config().host = "127.0.0.1";
        loopctrl.get_spawn_config().port = (int) auvp.GetValue();

        // connect
        loopctrl.connect();
         */
        
        //_guarantor = new wxEventLoopGuarantor();
        //_guarantor = new wxEventLoopGuarantor();
        
        _eventloop = new wxEventLoop();
        
        
        _app->OnInit();
        
        _appframe = _app->get_main_frame();

        NSLog(@"Called initialize on wx: %p", _appframe);

    }
    else {
        NSLog(@"Shouldnt be in here");
        //if (_guarantor) delete _guarantor;
        //_guarantor = new wxEventLoopGuarantor();
   
    }
    //dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    // NSLog(@"Calling onrun: %p", _app);

#if 0
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(windowResized:) name:NSWindowDidResizeNotification
                                               object:[self window]];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(windowMoved:) name:NSWindowDidMoveNotification
                                               object:[self window]];

#endif

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(windowBecameKey:) name:NSWindowDidBecomeKeyNotification
                                               object:[self window]];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(viewDidHide) name:NSWindowDidMiniaturizeNotification
                                               object:[self window]];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(viewDidUnhide) name:NSWindowDidDeminiaturizeNotification
                                               object:[self window]];


    
    // [self windowResized:NULL];
    
#if 0
    //SooperLooperGui::MainPanel * panel = _app->get_main_panel();
    //wxPanel * panel = _app->get_top_panel();
    //wxFrame * frame = _app->get_main_frame();
    wxWindow * panel = _appframe;
    
    /*
    wxFrame * frame =  new wxFrame(NULL, -1, "SooperLooper Plugin UI", wxDefaultPosition, wxDefaultSize, 0);
	wxBoxSizer * sizer = new wxBoxSizer(wxVERTICAL);
    
    //frame->Create( GetParent(), nativeWindow );
    
	sizer->Add (panel, 1, wxEXPAND);
	
	frame->SetSizer(sizer);
	frame->SetAutoLayout(true);
	sizer->Fit(frame);
	sizer->SetSizeHints(frame);
	
    frame->SetSize(820,215);
    
    //SetTopWindow(_frame);
    
    frame->Show();
    
    sizer->Detach(panel);
    */

    
    NSView * panelv = ((NSView*)panel->GetHandle());
    
    //frame->RemoveChild(panel);
    [panelv removeFromSuperview];

    [self setAutoresizesSubviews:YES];
    [panelv setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    NSRect frect = panelv.frame;
    NSLog(@"Bounds are %g %g   %g %g", frect.origin.x, frect.origin.y, frect.size.width, frect.size.height);
    
    [self addSubview:panelv];

#endif
    //NSView * framev = ((NSView*)_app->get_main_frame()->GetHandle());
    //[framev setAlphaValue:0.0];
    
    // panel->Show();

    //frame->Hide();
    
    //[((NSView*)frame->GetHandle()) removeFromSuperview];
    //[self addSubview:(NSView*)frame->GetHandle()];

     //   wxTheApp->OnRun();

    //NSLog(@"Calling onexit: %p", _app);
    // wxTheApp->OnExit();

        //_app = nil;

       // NSLog(@"All done with GUI, calling cleanup");

        //dispatch_async(dispatch_get_main_queue(), ^(void){
    //wxEntryCleanup();
    //NSLog(@"All done with GUI");
        //});
        
    //});
    
    
    
    


}


#pragma mark ____ (INIT /) DEALLOC ____
- (void)dealloc {

    NSLog(@"dealloc cocoa view");

#if DO_EMBEDDED
    [self _removeListeners];
	

    [self disposeEmbeddedGui];
#endif
    
    if (_launcher) {
        _launcher->terminate();
		delete _launcher;
        _launcher = NULL;
	}
    
	[super dealloc];
}


-(void) create_slgui
{
	// get OSC port (arg 2)
	CAAUParameter auvp(mAU,  kParam_OSCPort, kAudioUnitScope_Global, 0);
	//char cmdbuf[255];
	//snprintf(cmdbuf, sizeof(cmdbuf), "/Users/jesse/src/sooperlooper/src/gui/slgui -H localhost -P %d -N &", (int) auvp.GetValue());
	//system(cmdbuf);
    
    
	char portbuf[10];
    //char pathbuf[500];
    //Size gotsize = 0 ;
    //pathbuf[0] = 0;
    //string slguipath = "/Users/jesse/src/sooperlooper/src/gui/slgui";
    //verify_noerr(GetControlData(_pathText, kControlEntireControl, kControlStaticTextTextTag, sizeof(pathbuf), pathbuf, &gotsize));
    //pathbuf[gotsize] = 0;
    
    if (!_slapp_path) {
        NSLog(@"No app path!");
        return;
    }
    
    string slguipath = [_slapp_path UTF8String];
    
    slguipath += "/Contents/MacOS/slgui";
    
    snprintf(portbuf, sizeof(portbuf), "%d", (int) auvp.GetValue());
    
	vector<string> args;
	args.push_back("-H");
	args.push_back("127.0.0.1");
	args.push_back("-P");
	args.push_back(portbuf);
	args.push_back("-N");
	
    
#if 0
    
    if (!_appframe) {
        [self startEmbeddedUI];
    }
    
    
#else
    _stay_on_top = ([uiStayOnTopCheck state] == NSOnState) ? 1 : 0;
    
	if (_stay_on_top) {
		args.push_back("-T");
	}
    
    //cerr << "launching " << slguipath << endl;
    if (_launcher == 0)
    {
        _launcher = new LaunchSLgui(slguipath, args);
        _launcher->launch();
    }
    else if (!_launcher->isRunning()) {
        _launcher->set_path(slguipath);
        _launcher->set_args(args);
        _launcher->launch();
    }
	
    if (_launcher->isRunning()) {
        [self set_app_path_property:_slapp_path];
        _stay_on_top = ([uiStayOnTopCheck state] == NSOnState) ? 1 : 0;
        [self set_stay_on_top_property:_stay_on_top ];
        //cerr << "set path property" << endl;
    }
    
#endif
    
}



-(void) set_stay_on_top_property:(short) value
{
	AudioUnit slau = mAU;
	UInt32 datasize;
	Boolean writable;
	if (AudioUnitGetPropertyInfo(slau, kSLguiStayOnTopProperty, kAudioUnitScope_Global, 0, &datasize, &writable) == noErr)
	{
		if (writable) {
			AudioUnitSetProperty(slau, kSLguiStayOnTopProperty, kAudioUnitScope_Global, 0, &_stay_on_top, sizeof(short));
		}
	}
	
    NSLog(@"Set stayontop to %d", value);
    
#ifdef DO_EMBEDDED
    if (_appframe) {
        // set it to be stay on top style or not
        [self startEmbeddedUI];
    }
#endif
    
	// also write default into file
	char * homedir = getenv("HOME");
	if (homedir) {
		struct stat st;
		string defpath(homedir);
		defpath += "/.sooperlooper";
		// create dir if necessary
		if (::stat(defpath.c_str(), &st) != 0) {
			::mkdir(defpath.c_str(), 0755);
		}
		defpath += "/default_stay_on_top";
		ofstream defappstream(defpath.c_str());
		if (defappstream.is_open()) {
			defappstream << _stay_on_top << endl;
			defappstream.close();
		}
	}
}

-(void) update_stay_on_top
{
    _stay_on_top =  ([uiStayOnTopCheck state] == NSOnState) ? 1 : 0;
	//_stay_on_top = GetControlValue(_stayOnTopCheck);
	[self set_stay_on_top_property:_stay_on_top];
}


#pragma mark -
#pragma mark Listener Callback Handling
- (void)_parameterListener:(void *)inObject parameter:(const AudioUnitParameter *)inParameter value:(AudioUnitParameterValue)inValue {
    // inObject ignored in this case.
    
    /*
	switch (inParameter->mParameterID) {
		case kParam_One:
            [uiParam1Slider setFloatValue:inValue];
            [uiParam1TextField setStringValue:[[NSNumber numberWithFloat:inValue] stringValue]];
            break;
		case kParam_Two:
            [uiParam2Slider setFloatValue:inValue];
            [uiParam2TextField setStringValue:[[NSNumber numberWithFloat:inValue] stringValue]];
			break;
		case kParam_Three_Indexed:
            [uiParam3Matrix setState:NSOnState atRow:(inValue - 4) column:0];
			break;
	}
     */
}

void ParameterListenerDispatcher (void *inRefCon, void *inObject, const AudioUnitParameter *inParameter, AudioUnitParameterValue inValue) {
	SooperLooperCocoaView *SELF = (SooperLooperCocoaView *)inRefCon;
    
    [SELF _parameterListener:inObject parameter:inParameter value:inValue];
}

#pragma mark -
#pragma mark Private Functions
- (void)_addListeners {
    
#if 0
	verify_noerr ( AUListenerCreate (	ParameterListenerDispatcher, self, CFRunLoopGetCurrent(),
										kCFRunLoopDefaultMode, 0.100 /* 100 ms */, &mParameterListener	));
	
    int i;
    for (i = 0; i < kNumberOfParameters; ++i) {
        parameter[i].mAudioUnit = mAU;
        verify_noerr ( AUListenerAddParameter (mParameterListener, NULL, &parameter[i]));
    }
#endif
}

- (void)_removeListeners {
#if 0
    int i;
    for (i = 0; i < kNumberOfParameters; ++i) {
        verify_noerr ( AUListenerRemoveParameter(mParameterListener, NULL, &parameter[i]) );
    }
    
	verify_noerr (	AUListenerDispose(mParameterListener) );
#endif
}


- (void)_synchronizeUIWithParameterValues {
	
    [uiPathTextField setStringValue:_slapp_path];
    [uiStayOnTopCheck setState:(_stay_on_top ? NSOnState: NSOffState)];
    
    CAAUParameter auvp(mAU,  kParam_OSCPort, kAudioUnitScope_Global, 0);
    [uiPortTextField setStringValue:[NSString stringWithFormat:@"%d",(int)auvp.GetValue()]];
    
    /*
    AudioUnitParameterValue value;
    int i;
    
    for (i = 0; i < kNumberOfParameters; ++i) {
        // only has global parameters
        verify_noerr (AudioUnitGetParameter(mAU, parameter[i].mParameterID, kAudioUnitScope_Global, 0, &value));
        verify_noerr (AUParameterSet (mParameterListener, self, &parameter[i], value, 0));
        verify_noerr (AUParameterListenerNotify (mParameterListener, self, &parameter[i]));
    }
     */
}



#pragma mark ____ PUBLIC FUNCTIONS ____
- (void)setAU:(AudioUnit)inAU {
	// remove previous listeners
	if (mAU)
		[self _removeListeners];
	
	mAU = inAU;
    
	if (mAU) {
		// add new listeners
		[self _addListeners];
		
        // our setup
        NSLog(@"Cocoa GUI init");
        [self init_app_path];
        [self init_stay_on_top];
        
        NSLog(@"Init app path: %@", _slapp_path);

        
		// initial setup
		[self _synchronizeUIWithParameterValues];
        
        
        //if (!_appframe) {
        //    [self startEmbeddedUI];
        //}
	}
}

#pragma mark ____ INTERFACE ACTIONS ____

- (IBAction)iaBrowseButtonPushed:(id)sender
{
    // Create a File Open Dialog class.
    NSOpenPanel* openDlg = [NSOpenPanel openPanel];
    
    // Set array of file types
    NSArray *fileTypesArray;
    fileTypesArray = [NSArray arrayWithObjects:@"app", NULL];

    // Enable options in the dialog.
    [openDlg setCanChooseFiles:YES];
    [openDlg setAllowedFileTypes:fileTypesArray];
    [openDlg setAllowsMultipleSelection:FALSE];
    [openDlg setCanChooseDirectories:NO];
    
    // Display the dialog box.  If the OK pressed,
    // process the files.
    if ( [openDlg runModal] == NSOKButton ) {
        
        NSURL *file = [openDlg URL];
        _slapp_path = [file path];
        
        NSLog(@"SL app path returned: %@", [file path]);
        
        [uiPathTextField setStringValue:_slapp_path];
    }
    
}

- (IBAction)iaStartGuiButtonPushed:(id)sender
{
    [self create_slgui];
}

- (IBAction)iaStayOnTopChanged:(id)sender
{
    [self update_stay_on_top];
}

/*
- (IBAction)iaParam1Changed:(id)sender {
    float floatValue = [sender floatValue];
	
	verify_noerr (AUParameterSet(mParameterListener, sender, &parameter[0], floatValue, 0));
	
    if (sender == uiParam1Slider) {
        [uiParam1TextField setFloatValue:floatValue];
    } else {
        [uiParam1Slider setFloatValue:floatValue];
    }
}

- (IBAction)iaParam2Changed:(id)sender {
    float floatValue = [sender floatValue];
	
	verify_noerr (AUParameterSet(mParameterListener, sender, &parameter[1], floatValue, 0));
	
    if (sender == uiParam2Slider) {
        [uiParam2TextField setFloatValue:floatValue];
    } else {
        [uiParam2Slider setFloatValue:floatValue];
    }
}

- (IBAction)iaParam3Changed:(id)sender {
	verify_noerr (AUParameterSet(mParameterListener, sender, &parameter[2], [uiParam3Matrix selectedRow] + 4, 0));
}
*/

@end
