/*
    Copyright (C) 2001 Paul Davis, 2004 Jesse Chappell

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#ifndef __ardour_keyboard_target_h__
#define __ardour_keyboard_target_h__

#include <map>
#include <string>
#include <vector>
#include <sigc++/sigc++.h>

#include <wx/window.h>
#include <wx/event.h>

//#include <gdk/gdk.h>
//#include <gtk--/window.h>

#include "xml++.hpp"

//#include "keyboard.h"


using std::map;
using std::string;

namespace SooperLooperGui {


class KeyboardTarget 
{
  public:
	KeyboardTarget(wxWindow *win, string name);
	virtual ~KeyboardTarget();

	// first arg is key_release bool
	typedef SigC::Slot1<void, bool> KeyAction;

	typedef std::vector<unsigned int> KeyState;
	
	string name() const { return _name; }

	void process_key_event (wxKeyEvent &ev);

	static KeyState  translate_key_name (const string&);
	static int keycode_from_name (const wxString &);
	static wxString name_from_keycode (int key);
	
	int add_binding (string keys, string name);
	string get_binding (string name); /* returns keys bound to name */

	XMLNode& get_binding_state () const;
	int set_binding_state (const XMLNode&);

	static int add_action (string, KeyAction);
	static int find_action (string, KeyAction&);
	static int remove_action (string);
	static void show_all_actions();

	wxWindow * window() const { return _window; }
	
  protected:
	typedef map<KeyState,KeyAction> KeyMap;
	typedef map<string,string> BindingMap;

	bool update_state (wxKeyEvent &ev);

	
	KeyMap     keymap;
	BindingMap bindings;

	KeyState      _state;
	
  private:
	typedef map<string,KeyAction> ActionMap; 
	static ActionMap actions;
	string _name;
	wxWindow * _window;

	int load_bindings (const XMLNode&);
};

};
	
#endif /* __ardour_keyboard_target_h__ */

