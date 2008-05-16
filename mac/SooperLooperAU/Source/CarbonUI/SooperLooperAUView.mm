/*
*	File:		SooperLooperAUView.cpp
**	
*	Version:	1.0
* 
*	Created:	7/2/05
*	
*	Copyright:  Copyright � 2005 __MyCompanyName__, All Rights Reserved
* 
*	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. ("Apple") in 
*				consideration of your agreement to the following terms, and your use, installation, modification 
*				or redistribution of this Apple software constitutes acceptance of these terms.  If you do 
*				not agree with these terms, please do not use, install, modify or redistribute this Apple 
*				software.
*
*				In consideration of your agreement to abide by the following terms, and subject to these terms, 
*				Apple grants you a personal, non-exclusive license, under Apple's copyrights in this 
*				original Apple software (the "Apple Software"), to use, reproduce, modify and redistribute the 
*				Apple Software, with or without modifications, in source and/or binary forms; provided that if you 
*				redistribute the Apple Software in its entirety and without modifications, you must retain this 
*				notice and the following text and disclaimers in all such redistributions of the Apple Software. 
*				Neither the name, trademarks, service marks or logos of Apple Computer, Inc. may be used to 
*				endorse or promote products derived from the Apple Software without specific prior written 
*				permission from Apple.  Except as expressly stated in this notice, no other rights or 
*				licenses, express or implied, are granted by Apple herein, including but not limited to any 
*				patent rights that may be infringed by your derivative works or by other works in which the 
*				Apple Software may be incorporated.
*
*				The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, EXPRESS OR 
*				IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY 
*				AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE 
*				OR IN COMBINATION WITH YOUR PRODUCTS.
*
*				IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL 
*				DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
*				OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, 
*				REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER 
*				UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN 
*				IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#include "SooperLooperAUView.h"
#include "AUCarbonViewBase.h"
#include "AUControlGroup.h"

#include "SLproperties.h"

#include <pthread.h>

#include <iostream>
#include <fstream>
#include <cstdlib>

#include <sys/types.h>
#include <sys/stat.h>

#include "launch_slgui.h"

using namespace std;

#define kDefaultSlguiPath "/Applications/SooperLooper-1.0.8dev/SooperLooper.app"

#define kSLstartCmd 'Strt'
#define kSLbrowseCmd 'Brws'
#define kSLstayOnTopCmd 'Otop'


COMPONENT_ENTRY(SooperLooperAUView)

SooperLooperAUView::SooperLooperAUView(AudioUnitCarbonView auv) : AUCarbonViewBase(auv) 
{ 
	_launcher = 0;
	_slapp_path = kDefaultSlguiPath;
	_winHandler = 0;
	_stay_on_top = 0;
}

SooperLooperAUView::~SooperLooperAUView()
{
	RemoveEventHandler(_winHandler);
	
	//::wxUninitialize();
	if (_launcher) {
	  _launcher->terminate();
		delete _launcher;
	}
}

// ____________________________________________________________________________
//
OSStatus	SooperLooperAUView::CreateUI(Float32 xoffset, Float32 yoffset)
{
    // need offsets as int's:
    int xoff = (int)xoffset;
    int yoff = (int)yoffset;
    
    // for each parameter, create controls
	// inside mCarbonWindow, embedded in mCarbonPane
	
#define kCheckWidth 120
#define kLabelWidth 220
#define kLabelHeight 16
#define kEditTextWidth 40
#define kMinMaxWidth 32
	
	ControlRef newControl;
	ControlFontStyleRec fontStyle;
	fontStyle.flags = kControlUseFontMask | kControlUseJustMask;
	fontStyle.font = kControlFontSmallSystemFont;
	fontStyle.just = teFlushLeft;
	
	Rect r;
	Point labelSize, textSize;
	labelSize.v = textSize.v = kLabelHeight;
	labelSize.h = kMinMaxWidth;
	textSize.h = kEditTextWidth;
	
	int ypos = 10;

	// initialize slapp path
	init_app_path();
	init_stay_on_top();
	
	//cerr << "INITIAL APP PATH is : " << _slapp_path << endl;
	
	/*
	{
		CAAUParameter auvp(mEditAudioUnit, 0, kAudioUnitScope_Global, 0);
		
		// text label
		r.top = ypos + yoff;
        r.bottom = r.top + kLabelHeight;
		r.left = 10 +xoff;
        r.right = r.left + kLabelWidth;
		verify_noerr(CreateStaticTextControl(mCarbonWindow, &r, auvp.GetName(), &fontStyle, &newControl));
		verify_noerr(EmbedControl(newControl));

		r.left = r.right + 4;
		r.right = r.left + 240;
		AUControlGroup::CreateLabelledSliderAndEditText(this, auvp, r, labelSize, textSize, fontStyle);
		
		ypos = r.bottom + 6;
	}
	*/
	
	{
		CAAUParameter auvp(mEditAudioUnit,  kParam_OSCPort, kAudioUnitScope_Global, 0);
		char tmpstr[255];
		snprintf(tmpstr, sizeof(tmpstr), "SooperLooper OSC server port is: %d", (int) auvp.GetValue());
		CFStringRef statstr = CFStringCreateWithCString(0, tmpstr, CFStringGetSystemEncoding());
		// text label with OSC port info
		r.top = ypos + yoff;
        r.bottom = r.top + kLabelHeight;
		r.left = 10 +xoff;
        r.right = r.left + kLabelWidth;

		verify_noerr(CreateStaticTextControl(mCarbonWindow, &r, statstr, &fontStyle, &newControl));
		verify_noerr(EmbedControl(newControl));
		ypos = r.bottom + 8;
		
		// add text edit for slgui path
		r.top = ypos + yoff;
        r.bottom = r.top + kLabelHeight;
		r.left = 10 +xoff;
        r.right = r.left + 80;
		fontStyle.just = teFlushRight;
		verify_noerr(CreateStaticTextControl(mCarbonWindow, &r, CFSTR("GUI Path: "), &fontStyle, &newControl));
		verify_noerr(EmbedControl(newControl));
		r.left = r.right + 4;
		r.right = r.left + 260;
		fontStyle.just = teFlushLeft;
		// shorten it
		string dispstr = _slapp_path;
		if (dispstr.size() > 40) {
			dispstr = dispstr.substr(0, 20) + " ... " +  dispstr.substr(dispstr.size() - 19, 19); 
		}
		verify_noerr(CreateStaticTextControl(mCarbonWindow, &r, CFStringCreateWithCString(0, dispstr.c_str(), CFStringGetSystemEncoding()),
											&fontStyle, &_pathText));
		verify_noerr(EmbedControl(_pathText));
		r.left = r.right + 8;
		r.right = r.left + 100;
		verify_noerr(CreatePushButtonControl (mCarbonWindow, &r, CFSTR("Browse.."), &newControl));
		verify_noerr(EmbedControl(newControl));
		_browseButton = newControl;		
		SetControlCommandID (_browseButton, kSLbrowseCmd);		
		ypos = r.bottom + 8;
		
		r.top = ypos + yoff;
		r.bottom = r.top + kLabelHeight;

		r.left = 3 + xoff;
		r.right = r.left + kCheckWidth;
		verify_noerr(CreateCheckBoxControl (mCarbonWindow, &r, CFSTR("Keep on top"), _stay_on_top, true, &newControl));
		verify_noerr(EmbedControl(newControl));
		_stayOnTopCheck = newControl;
		SetControlCommandID (_stayOnTopCheck, kSLstayOnTopCmd);
		
		r.left = 6 + kCheckWidth + xoff;
		r.right = r.left + kLabelWidth;
		verify_noerr(CreatePushButtonControl (mCarbonWindow, &r, CFSTR("Start GUI"), &newControl));
		verify_noerr(EmbedControl(newControl));
		_startGuiButton = newControl;
		SetControlCommandID (_startGuiButton, kSLstartCmd);
		ypos = r.bottom + 8;
	}
		
	EventTypeSpec myEventSpec = {kEventClassCommand, kEventCommandProcess};
	
	InstallWindowEventHandler(mCarbonWindow, NewEventHandlerUPP (SooperLooperAUView::winEventHandler)
							  , 1, &myEventSpec, this, &_winHandler); 

	// create_slgui();
	
	/*
	
	{
		CAAUParameter auvp(mEditAudioUnit, 1, kAudioUnitScope_Global, 0);
		
		// text label
		r.top = ypos + yoff;
        r.bottom = r.top + kLabelHeight;
		r.left = 10 +xoff;
        r.right = r.left + kLabelWidth;
		verify_noerr(CreateStaticTextControl(mCarbonWindow, &r, auvp.GetName(), &fontStyle, &newControl));
		verify_noerr(EmbedControl(newControl));
		
		r.left = r.right + 4;
		r.right = r.left + 240;
		AUControlGroup::CreateLabelledSliderAndEditText(this, auvp, r, labelSize, textSize, fontStyle);
		ypos = r.bottom + 6;
	}
	*/
	

	// set size of overall pane
	SizeControl(mCarbonPane, mBottomRight.h + 8, mBottomRight.v + 8);
	
	create_slgui();
	return noErr;
}

void SooperLooperAUView::init_app_path()
{
	string guipath;
	
	// use non-empty value from AU property first
	AudioUnit slau = GetEditAudioUnit();
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
		_slapp_path = guipath;
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
		_slapp_path = guipath;
		return;
	}
	
	// finally default to hardcoded value
	_slapp_path = "/Applications/SooperLooper.app";
	
}

void SooperLooperAUView::set_app_path_property(std::string guipath)
{
	AudioUnit slau = GetEditAudioUnit();
	UInt32 datasize;
	Boolean writable;
	if (AudioUnitGetPropertyInfo(slau, kSLguiAppPathProperty, kAudioUnitScope_Global, 0, &datasize, &writable) == noErr)
	{
		if (writable) {
			AudioUnitSetProperty(slau, kSLguiAppPathProperty, kAudioUnitScope_Global, 0, guipath.c_str(), guipath.size());
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
			defappstream.write (guipath.c_str(), guipath.size());
			defappstream.write("\n", 1);
			defappstream.close();
		}
	}
}

void SooperLooperAUView::init_stay_on_top()
{
	short value = 0;
	
	// use non-empty value from AU property first
	AudioUnit slau = GetEditAudioUnit();
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

void SooperLooperAUView::set_stay_on_top_property(short value)
{
	AudioUnit slau = GetEditAudioUnit();
	UInt32 datasize;
	Boolean writable;
	if (AudioUnitGetPropertyInfo(slau, kSLguiStayOnTopProperty, kAudioUnitScope_Global, 0, &datasize, &writable) == noErr)
	{
		if (writable) {
			AudioUnitSetProperty(slau, kSLguiStayOnTopProperty, kAudioUnitScope_Global, 0, &_stay_on_top, sizeof(short));
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
		defpath += "/default_stay_on_top";
		ofstream defappstream(defpath.c_str());
		if (defappstream.is_open()) {
			defappstream << _stay_on_top << endl;
			defappstream.close();
		}
	}
}

void SooperLooperAUView::create_slgui()
{
	// get OSC port (arg 2)
	CAAUParameter auvp(mEditAudioUnit,  kParam_OSCPort, kAudioUnitScope_Global, 0);
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
	  string slguipath = _slapp_path;
    
	  slguipath += "/Contents/MacOS/slgui";
	  
	  snprintf(portbuf, sizeof(portbuf), "%d", (int) auvp.GetValue()); 

	vector<string> args;
	args.push_back("-H");
	args.push_back("127.0.0.1");
	args.push_back("-P");
	args.push_back(portbuf);
	args.push_back("-N");
	
	if (GetControlValue(_stayOnTopCheck)) {
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
	  set_app_path_property(_slapp_path);  
	  _stay_on_top = GetControlValue(_stayOnTopCheck);
	  set_stay_on_top_property(_stay_on_top);
	  //cerr << "set path property" << endl;
  }
	//_slgui_thread(this);
	
	//pthread_t guithread;
	//pthread_create (&guithread, NULL, &SooperLooperAUView::_slgui_thread, this);

}

void SooperLooperAUView::update_stay_on_top()
{
	_stay_on_top = GetControlValue(_stayOnTopCheck);
	set_stay_on_top_property(_stay_on_top);
}

void * SooperLooperAUView::_slgui_thread(void * arg)
{
	return 0;
}

// handle app command events for the buttons
pascal OSStatus SooperLooperAUView::winEventHandler (EventHandlerCallRef myHandler, 
										   EventRef event, void *userData)
{
    OSStatus        result  = eventNotHandledErr;                            // 1
    HICommand       command;
	SooperLooperAUView * auview = (SooperLooperAUView *) userData;
	bool ret;
	
    GetEventParameter (event, kEventParamDirectObject, typeHICommand, NULL, 
					   sizeof (HICommand), NULL, &command);    // 2
    switch (command.commandID)                       
		
	{
        case kSLbrowseCmd:                                             // 3
			//cerr << "got browse button command event" << endl;
			ret = auview->DisplayOpenFileDialog();
			if (ret) {
            
			}
				
			result = noErr;
			break;
		case kSLstartCmd:
			//cerr << "got command start gui" << endl;			
			auview->create_slgui();
			
			result = noErr;
			break;
		case kSLstayOnTopCmd:
			auview->update_stay_on_top();
			result = noErr;
			break;
	}
		
    return result;
}

#if 0
bool  SooperLooperAUView::HandleEvent(EventRef event)
{
	//UInt32 eclass = GetEventClass(event);
	//UInt32 ekind = GetEventKind(event);
	//ControlRef control;
	
	/*
	if (eclass == kEventClassControl) {
		GetEventParameter(event, kEventParamDirectObject, typeControlRef, NULL, sizeof(ControlRef), NULL, &control);
		cerr << "got  control event" << endl;
	
		if (control == _startGuiButton) {
			cerr << "got start gui button event" << endl;
			if (ekind == kEventControlClick) {
				cerr << "got click in start gui" << endl;			
				create_slgui();
				return true;
			}
		}
		else if (control == _browseButton) {
			if (ekind == kEventControlClick) {
				
				bool ret = DisplayOpenFileDialog();
				if (ret) {
						// update text
					cerr << "update text with: " << _slapp_path << endl;
					//verify_noerr(SetControlData(_pathText, kControlEntireControl, kControlEditTextTextTag, _slapp_path.size(), _slapp_path.c_str()));
					//Draw1Control(_pathText);
				}
			}
		}
	}
	*/		
	return AUCarbonViewBase::HandleEvent(event);
	
	//_guiapp->MacHandleOneEvent(event);
	
	//return true;
}
#endif

NavEventUPP  gNavEventHandlerPtr;

bool SooperLooperAUView::DisplayOpenFileDialog()
{
	OSStatus                 err;
	NavDialogRef             openDialog;
	NavDialogCreationOptions dialogAttributes;
	
	err = NavGetDefaultDialogCreationOptions( &dialogAttributes );
	
	//dialogAttributes.parentWindow = mCarbonWindow;
	dialogAttributes.modality = kWindowModalityAppModal;   
	dialogAttributes.optionFlags |= kNavSupportPackages;
	dialogAttributes.optionFlags &= ~kNavAllowPreviews;
	dialogAttributes.message = CFSTR("Select the appropriate SooperLooper application.");
	dialogAttributes.clientName = CFSTR("SLauView");
	
	gNavEventHandlerPtr = NewNavEventUPP( SooperLooperAUView::NavEventCallback );   
	
	err = NavCreateChooseFileDialog( &dialogAttributes, NULL, 
								  gNavEventHandlerPtr, NULL, NULL, 
								  this, &openDialog );
	
	err = NavDialogRun( openDialog );
	
	if ( err != noErr )
	{
		NavDialogDispose( openDialog );
		DisposeNavEventUPP( gNavEventHandlerPtr );
	}

	return true;
}

pascal void SooperLooperAUView::NavEventCallback(
									  NavEventCallbackMessage callBackSelector,
									  NavCBRecPtr             callBackParms, 
									  void*                   callBackUD )
{
	OSStatus     err;
	NavReplyRecord  reply;
	NavUserAction  userAction = 0;   
	SooperLooperAUView * auview = (SooperLooperAUView *) callBackUD;
	//cerr << "Got callback " << endl;
	switch ( callBackSelector )
	{
		case kNavCBUserAction:    
			err = NavDialogGetReply( callBackParms->context, &reply );
			userAction = NavDialogGetUserAction( callBackParms->context );   
			//cerr << "user action : " << userAction << endl;
			switch ( userAction )
			{
				case kNavUserActionChoose:
					// open file here using reply record information
					//cerr << "got open" << endl;
					AEKeyword   theKeyword;
					DescType    actualType;
					Size        actualSize;
					FSRef       theFSRef;
					long count;
					::AECountItems(&reply.selection , &count);
			
					for (long i = 1; i <= count; ++i)
					{
						err = ::AEGetNthPtr(&(reply.selection), i, typeFSRef, &theKeyword, &actualType,
											&theFSRef, sizeof(theFSRef), &actualSize);
						if (err != noErr) {
							break;
						}
						
						CFURLRef fullURLRef;
						fullURLRef = CFURLCreateFromFSRef(NULL, &theFSRef);
						
						CFStringRef cfString = CFURLCopyFileSystemPath(fullURLRef, kCFURLPOSIXPathStyle);
						CFRelease( fullURLRef ) ;
						
						char strbuf[500];
						if (CFStringGetCString(cfString, strbuf, sizeof(strbuf), kCFStringEncodingUTF8)) {
							auview->_slapp_path = strbuf;

							// shorten it
							string dispstr = auview->_slapp_path;
							if (dispstr.size() > 40) {
								dispstr = dispstr.substr(0, 20) + " ... " +  dispstr.substr(dispstr.size() - 19, 19); 
							}
							
							verify_noerr(SetControlData(auview->_pathText, kControlEntireControl, kControlStaticTextTextTag, dispstr.size(), dispstr.c_str()));
							DrawOneControl(auview->_pathText);
						}
						else {
							//cerr << "no path" << endl;
						}
						
						CFRelease(cfString);
						
						break;
					}
					
					break;
			}
				err = NavDisposeReply( &reply );
			break;
			
		case kNavCBTerminate:
			NavDialogDispose( callBackParms->context );
			DisposeNavEventUPP( gNavEventHandlerPtr );
			break;
	}
}
