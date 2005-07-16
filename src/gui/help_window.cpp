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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <wx/mimetype.h>

#include <iostream>
#include <cstdlib>

#include "help_window.hpp"

using namespace SooperLooperGui;
using namespace std;

HelpWindowHtmlWin::HelpWindowHtmlWin (wxWindow *parent, wxWindowID id, const wxPoint& pos,
					const wxSize& size, long style, const wxString& name)
	: wxHtmlWindow(parent, id, pos, size, style, name)
{
}

void HelpWindowHtmlWin::OnLinkClicked(const wxHtmlLinkInfo& link)
{
	// takes an URL and directs the browser to show it
        wxString cmdstr;
        
#ifndef WIN32
	//if (!browseToUrl(link.GetHref())) {
	// system specific hack workaround
#ifdef __WXMAC__
	cmdstr = wxString::Format(wxT("open %s"), link.GetHref().c_str());
#else
	cmdstr = wxString::Format(wxT("gnome-moz-remote --newwin '%s' &"), link.GetHref().c_str());
#endif
	//}
#else
	// windows has its own special way
	cmdstr = wxString::Format(wxT("rundll32 url.dll,FileProtocolHandler %s"), link.GetHref().c_str());
	
#endif

	std::system((const char *) cmdstr.ToAscii());
	

}

bool HelpWindowHtmlWin::browseToUrl(const wxString& url)
{
	// Not convinced this works at all.  Got from wx-dev mailing list.

	wxFileType *ft = wxTheMimeTypesManager->GetFileTypeFromExtension(wxT("html"));
	if ( !ft )
	{
		//wxLogError(_T("Impossible to determine the file type for extension html. Please edit your MIME types."));
		return false;
	}
	
	wxString cmd;
	bool ok = ft->GetOpenCommand(&cmd,
				     wxFileType::MessageParameters(url, _T("")));
	delete ft;
	
	
	if (!ok)
	{
		// TODO: some kind of configuration dialog here.
		//wxMessageBox(_("Could not determine the command for running the browser."),
		//	     wxT("Browsing problem"), wxOK|wxICON_EXCLAMATION);
		return false;
	}
	
	// GetOpenCommand can prepend file:// even if it already has http://
	if (cmd.Find(wxT("http://")) != -1)
		cmd.Replace(wxT("file://"), wxT(""));
	
	
	ok = (wxExecute(cmd, FALSE) != 0);

	return ok;
}




BEGIN_EVENT_TABLE(SooperLooperGui::HelpWindow, wxFrame)
	EVT_CLOSE(SooperLooperGui::HelpWindow::on_close)
END_EVENT_TABLE()

HelpWindow::HelpWindow(wxWindow * parent, wxWindowID id, const wxString & title,
			   const wxPoint& pos,
			   const wxSize& size,
			   long style,
			   const wxString& name)

	: wxFrame(parent, id, title, pos, size, style, name)
{

	init();
}

HelpWindow::~HelpWindow()
{

}
	

void HelpWindow::init()
{
	wxBoxSizer * mainsizer = new wxBoxSizer(wxVERTICAL);
	
	_htmlWin = new HelpWindowHtmlWin(this, -1, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxHW_SCROLLBAR_AUTO);

	mainsizer->Add (_htmlWin, 1, wxALL|wxEXPAND, 4);

	SetAutoLayout( TRUE );
	mainsizer->Fit( this );  
	mainsizer->SetSizeHints( this );  
	SetSizer( mainsizer );

	const int sizes[] = {7, 8, 10, 12, 16, 22, 30};
	
	_htmlWin->SetFonts(wxT(""), wxT(""), sizes);

	// just show nothing for now
//	wxString helppath = wxString(wxT(HELP_HTML_PATH)) + wxFileName::GetPathSeparator() + wxString(wxT("usagehelp.html")); 
	
// 	if (wxFile::Access(helppath, wxFile::read))
// 	{
// 		_htmlWin->LoadPage(helppath);
// 	}
// 	else {
		_htmlWin->SetPage(wxString(wxT("Currently, online documentation may found on the web at <a href=\"http://essej.net/sooperlooper/\">http://essej.net/sooperlooper/</a>")));
				  
//	}
		
	this->SetSizeHints(200,100);
	this->SetSize(500,200);
}

void HelpWindow::on_close (wxCloseEvent &ev)
{
	if (!ev.CanVeto()) {
		
		Destroy();
	}
	else {
		ev.Veto();
		
		Show(false);
	}
}
