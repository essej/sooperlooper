/*
** Copyright (C) 2002 Jesse Chappell <jesse@essej.net>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/

#ifndef __sooperlooper_help_window_hpp__
#define __sooperlooper_help_window_hpp__

#include <wx/wx.h>
#include <wx/html/htmlwin.h>

#include <vector>
#include <string>

namespace SooperLooperGui
{

class HelpWindowHtmlWin
	: public wxHtmlWindow
{
public:
	HelpWindowHtmlWin (wxWindow *parent, wxWindowID id = -1, const wxPoint& pos = wxDefaultPosition,
			   const wxSize& size = wxDefaultSize, long style = wxHW_SCROLLBAR_AUTO, const wxString& name = wxT("htmlWindow"));
	virtual ~HelpWindowHtmlWin() {}

	void OnLinkClicked(const wxHtmlLinkInfo& link);
	bool browseToUrl(const wxString& url);

};
	
class HelpWindow
	: public wxFrame
{
  public:

	HelpWindow(wxWindow * parent, wxWindowID id, const wxString & title,
		     const wxPoint& pos = wxDefaultPosition,
		     const wxSize& size = wxSize(300,400),
		     long style = wxDEFAULT_FRAME_STYLE,
		     const wxString& name = wxT("HelpWin"));

	virtual ~HelpWindow();
	

  protected:

	void init();
	void on_close (wxCloseEvent &ev);

	HelpWindowHtmlWin * _htmlWin;

  private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()

};

};

#endif
