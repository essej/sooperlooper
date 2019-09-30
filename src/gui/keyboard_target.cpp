/*
    Copyright (C) 2001-2002 Paul Davis , 2004 Jesse Chappell

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



#include "keyboard_target.hpp"

#include <wx/wx.h>

#include "gui_app.hpp"
#include "main_panel.hpp"
#include "loop_control.hpp"

#include <iostream>
#include <algorithm>

// #define DEBUG_KEYBOARD

using namespace SooperLooperGui;
using std::pair;
using namespace std;

//KeyboardTarget::ActionMap KeyboardTarget::actions;

KeyboardTarget::KeyboardTarget (wxWindow *win, string name)
{
	_window = win;
	_name = name;
	_enabled = true;
	_learning = false;
	
	// todo register some events for the win
}

KeyboardTarget::~KeyboardTarget ()
{
}

void
KeyboardTarget::set_enabled (bool flag)
{
	if (flag != _enabled) {
		_enabled = flag;

		if (_enabled) {
			// clear state
			_state.clear();
		}
	}
}

void
KeyboardTarget::process_key_event (wxKeyEvent &event)
{
	KeyMap::iterator result;
	bool changed = true;

	if (!_enabled) {
		event.Skip();
		return;
	}
	
	if (event.GetEventType() == wxEVT_KEY_DOWN)
	{
		changed = update_state (event);

		if (changed) {
			if (_learning) {
				// do nothing yet
			}
			else if ((result = keymap.find (_state)) != keymap.end()) {
				(*result).second (false);
			}
			else  {
				event.Skip();
			}
		}
		// if it didn't change, it was an autorepeat, which we ignore

	}
	else if (event.GetEventType() == wxEVT_KEY_UP) {

		if (_learning) {
			// this is the first key up while learning, commit the binding
			commit_learn ();
			_learning = false;
			LearningStopped(); // emit
		}
		else if ((result = keymap.find (_state)) != keymap.end()) {
			(*result).second (true);
		}
		else {
			event.Skip();
		}

		update_state (event);
	}
}

bool
KeyboardTarget::update_state (wxKeyEvent &ev)
{
	unsigned int keyval = (unsigned int) ev.GetKeyCode();
	bool changed = true;
	
	if (ev.GetEventType() == wxEVT_KEY_DOWN)
	{
		if (find (_state.begin(), _state.end(), keyval) == _state.end()) {
			_state.push_back (keyval);
			sort (_state.begin(), _state.end());
		}
		else {
			changed = false;
		}
	}
	else if (ev.GetEventType() == wxEVT_KEY_UP) {
		KeyState::iterator i;
		
		if ((i = find (_state.begin(), _state.end(), keyval)) != _state.end()) {
			_state.erase (i);
			sort (_state.begin(), _state.end());
		} 
	}

#ifdef DEBUG_KEYBOARD
	cerr << "STATE: ";
	for (KeyState::iterator i = _state.begin(); i != _state.end(); ++i) {
		cerr << name_from_keycode(*i) << ' ';
	}
	cerr << " changed: " << changed << endl;
#endif

	return changed;
}

int
KeyboardTarget::add_binding (string keystring, string action)
{
	KeyMap::iterator existing;
	BindingMap::iterator existingb;
	KeyState  state;
	KeyAction key_action;
 
	state = translate_key_name (keystring);

	if (keystring.length() == 0) {
		cerr << "KeyboardTarget: empty string passed to add_binding."
		     << endl;
		return -1;
	}
	
	if (state.size() == 0) {
		cerr << "KeyboardTarget: no translation found for " <<  keystring << endl;
		return -1;
	}
	
	if (find_action (action, key_action)) {
		cerr << "KeyboardTarget: unknown action " <<  action << endl;
		return -1;
	}

	/* remove any existing binding */

	if ((existing = keymap.find (state)) != keymap.end()) {
		keymap.erase (existing);
	}
	if ((existingb = bindings.find (keystring)) != bindings.end()) {
		bindings.erase (existingb);
	}
	
	keymap.insert (pair<KeyState,KeyAction> (state, key_action));
	bindings.insert (pair<string,string> (keystring, action));
	return 0;
}

bool
KeyboardTarget::start_learning (string actname)
{
	// the state at the next key-up will be bound to actname
	_learning = true;
	_learn_action = actname;

	cerr << "learning: " << _learn_action << endl;
	
	return true;
}

bool
KeyboardTarget::stop_learning (bool cancel)
{
	if (_learning) {
		if (!cancel) {
			// go ahead and bind current state
			commit_learn();
		}
		
		_learning = false;
		LearningStopped(); // emit
	}

	return true;
}

void
KeyboardTarget::commit_learn ()
{
	wxString keys;
	
	for (KeyState::iterator i = _state.begin(); i != _state.end(); ++i) {
		wxString key = name_from_keycode(*i);
		if (key == wxT("Shift") || key == wxT("Control") || key == wxT("Alt")) {
			keys = key + wxT("-") + keys;
		}
		else {
			keys = keys + key;
		}
	}
	// clear all for this command first, this is debatable
	clear_binding (_learn_action);
	add_binding (string ((const char *)keys.ToAscii()), _learn_action);

}

string
KeyboardTarget::get_binding (string name)
{
	BindingMap::iterator i;
	
	for (i = bindings.begin(); i != bindings.end(); ++i) {

		if (i->second == name) {

			/* convert keystring to GTK format */

			string str = i->first;
			return str;
		}
	}
	return string ();
}

void
KeyboardTarget::clear_binding (string name)
{
	/* clears keys bound to name */
	BindingMap::iterator i = bindings.begin();
	BindingMap::iterator tmpi;
	KeyMap::iterator tmpk;
		
	while (i != bindings.end()) {
		
		if (i->second == name) {
			KeyState keystate = translate_key_name (i->first);

			tmpi = i;
			++i;

			bindings.erase (tmpi);

			//cerr << "erase binding" << endl;
			
			if ((tmpk = keymap.find (keystate)) != keymap.end()) {
				keymap.erase (tmpk);
				//cerr << "erase binding keymap" << endl;
			}
		}
		else {
			++i;
		}
	}

}

void
KeyboardTarget::get_action_names (ActionNameList & nlist)
{
	ActionMap::iterator i;
	
	for (i = actions.begin(); i != actions.end(); ++i) {
		nlist.push_back (i->first);
	}

}

void
KeyboardTarget::show_all_actions ()
{
	ActionMap::iterator i;
	
	for (i = actions.begin(); i != actions.end(); ++i) {
		cout << i->first << endl;
	}
}

int
KeyboardTarget::add_action (string name, KeyAction action)
{
	pair<string,KeyAction> newpair;
	pair<ActionMap::iterator,bool> result;
	newpair.first = name;
	newpair.second = action;

	result = actions.insert (newpair);
	return result.second ? 0 : -1;
}

int
KeyboardTarget::find_action (string name, KeyAction& action)
{
	map<string,KeyAction>::iterator i;

	if ((i = actions.find (name)) != actions.end()) {
		action = i->second;
		return 0;
	} else {
		return -1;
	}
}

int
KeyboardTarget::remove_action (string name)
{
	map<string,KeyAction>::iterator i;

	if ((i = actions.find (name)) != actions.end()) {
		actions.erase (i);
		return 0;
	} else {
		return -1;
	}
}

XMLNode&
KeyboardTarget::get_binding_state () const
{
	XMLNode *node = new XMLNode ("context");
	BindingMap::const_iterator i;

	node->add_property ("name", _name);
       
	for (i = bindings.begin(); i != bindings.end(); ++i) {
		XMLNode *child;

		child = new XMLNode ("binding");
		child->add_property ("keys", i->first);
		child->add_property ("action", i->second);
		node->add_child_nocopy (*child);
	}
	
	return *node;
}
	
int
KeyboardTarget::set_binding_state (const XMLNode& node)
{
	XMLNodeList nlist = node.children();
	XMLNodeConstIterator niter;
	XMLNode *child_node;

	bindings.clear ();
	keymap.clear ();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		child_node = *niter;

		if (child_node->name() == "context") {
			XMLProperty *prop;
			
			if ((prop = child_node->property ("name")) != 0) {
				if (prop->value() == _name) {
					return load_bindings (*child_node);
				}
			}
		}
	}

	return 0;
}

int
KeyboardTarget::load_bindings (const XMLNode& node)
{
	XMLNodeList nlist = node.children();
	XMLNodeConstIterator niter;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		XMLProperty *keys;
		XMLProperty *action;
		
		keys = (*niter)->property ("keys");
		action = (*niter)->property ("action");

		if (!keys || !action) {
			cerr << "malformed binding node - ignored" << endl;
			continue;
		}

		add_binding (keys->value(), action->value());
			
	}

	return 0;
}

KeyboardTarget::KeyState
KeyboardTarget::translate_key_name (const string& name)
{
	string::size_type i;
	string::size_type len;
	bool at_end;
	string::size_type hyphen;
	string keyname;
	string whatevers_left;
	KeyState result;
	unsigned int keycode;
	
	i = 0;
	len = name.length();
	at_end = (len == 0);

	while (!at_end) {

		whatevers_left = name.substr (i);

		if ((hyphen = whatevers_left.find_first_of ('-')) == string::npos) {
			
                        /* no hyphen, so use the whole thing */
			
			keyname = whatevers_left;
			at_end = true;

		} else {

			/* There is a hyphen. */
			
			if (hyphen == 0 && whatevers_left.length() == 1) {
				/* its the first and only character */
			
				keyname = "-";
				at_end = true;

			} else {

				/* use the text before the hypen */
				
				keyname = whatevers_left.substr (0, hyphen);
				
				if (hyphen == len - 1) {
					at_end = true;
				} else {
					i += hyphen + 1;
					at_end = (i >= len);
				}
			}
		}
		
// 		if (keyname.length() == 1 && isupper (keyname[0])) {
// 			result.push_back (WXK_SHIFT);
// 		}
		
		if ((keycode = keycode_from_name (wxString::FromAscii(keyname.c_str()))) == 0) {
			cerr << "KeyboardTarget: keyname " <<  keyname << "  is unknown" << endl;
			result.clear();
			return result;
		}
		
		result.push_back (keycode);
	}

	sort (result.begin(), result.end());

	return result;
}

int
KeyboardTarget::keycode_from_name (const wxString &keyn)
{
	// this sucks that i have to do this
	wxString keyname (keyn);

	int keycode = 0;
	
	// shift

	if (keyname.empty()) {
		// error
	}
	else if (keyname.IsSameAs (wxT("shift"), false)) {
		keycode = WXK_SHIFT;
	}
	else if (keyname.IsSameAs (wxT("control"), false)) {
		keycode = WXK_CONTROL;
	}
	else if (keyname.IsSameAs (wxT("alt"), false)) {
		keycode = WXK_ALT;
	}
	else if (keyname.IsSameAs (wxT("numlock"), false)) {
		keycode = WXK_NUMLOCK;
	}
	else if ( keyname.Len() == 1 ) {
		// it's a letter
                keycode = keyname[0U];
		
                // Only call wxToupper if control, alt, or shift is held down,
                // otherwise lower case accelerators won't work.
                //if (accelFlags != wxACCEL_NORMAL) {
		keycode = wxToupper(keycode);
			//}
	}
	else {
                // is it a function key?
                if ( keyname[0U] == 'f' && isdigit(keyname[1U]) &&
                     (keyname.Len() == 2 ||
                     (keyname.Len() == 3 && isdigit(keyname[2U]))) ) {
                    int n;
                    wxSscanf(keyname.c_str() + 1, wxT("%d"), &n);

                    keycode = WXK_F1 + n - 1;
                }
                else {
                    // several special cases
                    keyname.MakeUpper();
                    if ( keyname == wxT("DEL") ) {
                        keycode = WXK_DELETE;
                    }
                    else if ( keyname == wxT("DELETE") ) {
                        keycode = WXK_DELETE;
                    }
                    else if ( keyname == wxT("INS") ) {
                        keycode = WXK_INSERT;
                    }
                    else if ( keyname == wxT("INSERT") ) {
                        keycode = WXK_INSERT;
                    }
                    else if ( keyname == wxT("ENTER") || keyname == wxT("RETURN") ) {
                        keycode = WXK_RETURN;
                    }
                    else if ( keyname == wxT("PGUP") ) {
                        keycode = WXK_PAGEUP;
                    }
                    else if ( keyname == wxT("PGDN") ) {
                        keycode = WXK_PAGEDOWN;
                    }
                    else if ( keyname == wxT("LEFT") ) {
                        keycode = WXK_LEFT;
                    }
                    else if ( keyname == wxT("RIGHT") ) {
                        keycode = WXK_RIGHT;
                    }
                    else if ( keyname == wxT("UP") ) {
			    keycode = WXK_UP;
                    }
                    else if ( keyname == wxT("DOWN") ) {
			    keycode = WXK_DOWN;
                    }
		    else if ( keyname == wxT("HOME") ) {
			    keycode = WXK_HOME;
                    }
                    else if ( keyname == wxT("END") ) {
			    keycode = WXK_END;
                    }
                    else if ( keyname == wxT("SPACE") ) {
			    keycode = WXK_SPACE;
                    }
                    else if ( keyname == wxT("TAB") ) {
			    keycode = WXK_TAB;
                    }
                    else if ( keyname == wxT("ESCAPE") ) {
			    keycode = WXK_ESCAPE;
                    }
		}
	}

	return keycode;
}

wxString KeyboardTarget::name_from_keycode (int key)
{
	wxString text;

        switch ( key )
        {
	case WXK_SHIFT:
		text += wxT("Shift");
		break;
	case WXK_CONTROL:
		text += wxT("Control");
		break;
	case WXK_ALT:
		text += wxT("Alt");
		break;
		
	case WXK_F1:
	case WXK_F2:
	case WXK_F3:
	case WXK_F4:
	case WXK_F5:
	case WXK_F6:
	case WXK_F7:
	case WXK_F8:
	case WXK_F9:
	case WXK_F10:
	case WXK_F11:
	case WXK_F12:
                text << wxT('F') << key - WXK_F1 + 1;
                break;
	case WXK_DELETE:
		text += wxT("delete");
		break;
	case WXK_INSERT:
		text += wxT("insert");
		break;
	case WXK_RETURN:
		text += wxT("return");
		break;
	case WXK_PAGEUP:
		text += wxT("pageup");
		break;
	case WXK_PAGEDOWN:
		text += wxT("pagedown");
		break;
	case WXK_LEFT:
		text += wxT("left");
		break;
	case WXK_RIGHT:
		text += wxT("right");
		break;
	case WXK_UP:
		text += wxT("up");
		break;
	case WXK_DOWN:
		text += wxT("down");
		break;
	case WXK_HOME:
		text += wxT("home");
		break;
	case WXK_END:
		text += wxT("end");
		break;
	case WXK_SPACE:
		text += wxT("space");
		break;
	case WXK_TAB:
		text += wxT("tab");
		break;
	case WXK_ESCAPE:
		text += wxT("escape");
		break;
	case WXK_NUMLOCK:
		text += wxT("numlock");
		break;	
	default:
                if ( wxIsalnum(key) )
                {
			text << (wxChar)key;
			text.MakeLower();
			break;
                }
		
        }

	return text;

}


