/*
** Copyright (C) 2004 Jesse Chappell <jesse@essej.net>
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

#include <wx/wx.h>
#include <wx/file.h>
#include <wx/filename.h>

#include "looper_panel.hpp"
#include "pix_button.hpp"


using namespace SooperLooperGui;
using namespace std;


enum {
	ID_UndoButton = 8000,
	ID_RedoButton,
	ID_RecordButton,
	ID_OverdubButton,
	ID_MultiplyButton,
	ID_InsertButton,
	ID_ReplaceButton,
	ID_TapButton,
	ID_ReverseButton,
	ID_MuteButton


};

BEGIN_EVENT_TABLE(LooperPanel, wxPanel)

	
END_EVENT_TABLE()

LooperPanel::LooperPanel(wxWindow * parent, wxWindowID id, const wxPoint& pos, const wxSize& size)
	: wxPanel(parent, id, pos, size)
{
	init();
}

LooperPanel::~LooperPanel()
{

}


void
LooperPanel::init()
{
	SetBackgroundColour (*wxBLACK);
	SetThemeEnabled(false);

	wxBoxSizer * mainSizer = new wxBoxSizer(wxHORIZONTAL);

	wxBoxSizer * colsizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer * rowsizer;
	

 	PixButton * bitbutt = new PixButton(this, ID_UndoButton);
	load_bitmaps (bitbutt, wxT("undo"));
 	colsizer->Add (bitbutt, 0, wxTOP, 5);

 	bitbutt = new PixButton(this, ID_RedoButton);
	load_bitmaps (bitbutt, wxT("redo"));
 	colsizer->Add (bitbutt, 0, wxTOP, 5);
	
	mainSizer->Add (colsizer, 0, wxEXPAND|wxBOTTOM|wxLEFT, 5);

	
	colsizer = new wxBoxSizer(wxVERTICAL);

 	bitbutt = new PixButton(this, ID_RecordButton);
	load_bitmaps (bitbutt, wxT("record"));
 	colsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 5);

 	bitbutt = new PixButton(this, ID_OverdubButton);
	load_bitmaps (bitbutt, wxT("overdub"));
 	colsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 5);

 	bitbutt = new PixButton(this, ID_MultiplyButton);
	load_bitmaps (bitbutt, wxT("multiply"));
 	colsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 5);
	
	mainSizer->Add (colsizer, 0, wxEXPAND|wxBOTTOM, 5);


	colsizer = new wxBoxSizer(wxVERTICAL);
	rowsizer = new wxBoxSizer(wxHORIZONTAL);

	colsizer->Add (20, -1, 1);
	
 	bitbutt = new PixButton(this, ID_ReplaceButton);
	load_bitmaps (bitbutt, wxT("replace"));
 	rowsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 5);

 	bitbutt = new PixButton(this, ID_TapButton);
	load_bitmaps (bitbutt, wxT("tap"));
 	rowsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 5);

	colsizer->Add (rowsizer, 0);

	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	
 	bitbutt = new PixButton(this, ID_InsertButton);
	load_bitmaps (bitbutt, wxT("insert"));
 	rowsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 5);

 	bitbutt = new PixButton(this, ID_ReverseButton);
	load_bitmaps (bitbutt, wxT("reverse"));
 	rowsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 5);

	colsizer->Add (rowsizer, 0);
	
	mainSizer->Add (colsizer, 0, wxEXPAND|wxBOTTOM, 5);
	
	
	
	this->SetAutoLayout( true );     // tell dialog to use sizer
	this->SetSizer( mainSizer );      // actually set the sizer
	mainSizer->Fit( this );            // set size to minimum size as calculated by the sizer
	mainSizer->SetSizeHints( this );   // set size hints to honour mininum size

}


wxString
LooperPanel::get_pixmap_path (const wxString & namebase)
{
	wxString filename;
	
	if (wxFile::Exists(wxString::Format("%s%s", PIXMAPDIR, namebase.c_str()))) {
		filename = wxString::Format("%s%s", PIXMAPDIR, namebase.c_str());
	}
	else if (wxFile::Exists(wxString::Format("pixmaps%c%s", wxFileName::GetPathSeparator(), namebase.c_str()))) {
		filename = wxString::Format("pixmaps%c%s", wxFileName::GetPathSeparator(), namebase.c_str());
	}
	else if (wxFile::Exists (namebase)) {
		filename = namebase;
	}
	
	return filename;
}

bool
LooperPanel::load_bitmaps (PixButton * butt, wxString namebase)
{
	
	butt->set_normal_bitmap (wxBitmap(get_pixmap_path(namebase + wxT("_normal.xpm"))));
 	butt->set_selected_bitmap (wxBitmap(get_pixmap_path(namebase + wxT("_selected.xpm"))));
 	butt->set_focus_bitmap (wxBitmap(get_pixmap_path(namebase + wxT("_focus.xpm"))));
 	butt->set_disabled_bitmap (wxBitmap(get_pixmap_path(namebase + wxT("_disabled.xpm"))));

	return true;
}
