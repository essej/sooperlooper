#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

#include <vector>
#include <jack/jack.h>
#include <iostream>
#include <sigc++/bind.h>
#include <gtkmm.h>
#include <ladspa.h>

using namespace std;
using namespace SigC;

extern void sl_init ();
extern	void sl_fini ();
extern	const LADSPA_Descriptor* ladspa_descriptor (unsigned long);

enum ControlPort {
	TriggerThreshold = 0,
	DryLevel,
	WetLevel,
	Feedback,
	Rate,
	ScratchPosition,
	Multi,
	TapDelayTrigger,
	MultiTens,
	Quantize,
	Round,
	RedoTap,
};

enum OutputPort {
	State = 12,
	LoopLength,
	LoopPosition,
	CycleLength,
	LoopFreeMemory,
	LoopMemory
};

class SoopInstance 
{
public:
    SoopInstance (jack_client_t*, const LADSPA_Descriptor*);
    ~SoopInstance ();

    bool operator() () const { return _ok; }
    void run (jack_nframes_t nframes);
    void request_mode (int mode);

    void set_port (ControlPort n, LADSPA_Data val) {
	    ports[n] = val;
    }
    
    int requested_mode;
    int last_requested_mode;

    jack_client_t*     jack;
    jack_port_t*       input_port;
    jack_port_t*       output_port;

    LADSPA_Handle      instance;
    const LADSPA_Descriptor* descriptor;
    LADSPA_Data        ports[18];

    bool _ok;
    bool request_pending;
};

SoopInstance::SoopInstance (jack_client_t* j, const LADSPA_Descriptor* d)
	: jack (j),
	  descriptor (d)
	  
{
	_ok = false;
	requested_mode = 0;
	last_requested_mode = 0;
	request_pending = false;
	input_port = 0;
	output_port = 0;

	if ((instance = descriptor->instantiate (descriptor, jack_get_sample_rate (jack))) == 0) {
		return;
	}
	    
	if ((input_port = jack_port_register (jack, "input", JACK_DEFAULT_AUDIO_TYPE,
					      JackPortIsInput, 0)) == 0) {

		cerr << "cannot register loop input port\n";
		return;
	}
	
	if ((output_port = jack_port_register (jack, "output", JACK_DEFAULT_AUDIO_TYPE,
					       JackPortIsOutput, 0)) == 0) {
		cerr << "cannot register loop output port\n";
		return;
	}

	/* connect all scalar ports to data values */

	for (unsigned long n = 0; n < 18; ++n) {
		ports[n] = 0.0f;
		descriptor->connect_port (instance, n, &ports[n]);
	}

	_ok = true;
}

SoopInstance::~SoopInstance ()
{
	if (instance) {
		if (descriptor->deactivate) {
			descriptor->deactivate (instance);
		}
		if (descriptor->cleanup) {
			descriptor->cleanup (instance);
		}
		instance = 0;
	}
	
	if (input_port) {
		jack_port_unregister (jack, input_port);
		input_port = 0;
	}

	if (output_port) {
		jack_port_unregister (jack, output_port);
		output_port = 0;
	}
}

void
SoopInstance::request_mode (int mode)
{
	requested_mode = mode;
	request_pending = true;
}
	
void
SoopInstance::run (jack_nframes_t nframes)
{
	/* maybe change modes */

	if (request_pending) {
		
		if (ports[Multi] == requested_mode) {
			/* defer till next call */
			ports[Multi] = -1;
		} else {
			ports[Multi] = requested_mode;
			request_pending = false;
			cerr << "requested mode " << requested_mode << endl;
		}

	} else if (ports[Multi] >= 0) {
		ports[Multi] = -1;
		cerr << "reset to -1\n";
	}

	/* (re)connect audio ports */

	descriptor->connect_port (instance, 18, (LADSPA_Data*) jack_port_get_buffer (input_port, nframes));
	descriptor->connect_port (instance, 19, (LADSPA_Data*) jack_port_get_buffer (output_port, nframes));

	/* do it */
	
	descriptor->run (instance, nframes);
}

/*----------------------------------------------------------------------*/

class SoopPanel : public Gtk::HBox
{
public: 
    SoopPanel (SoopInstance&);
    ~SoopPanel ();

protected:
    SoopInstance& instance;

    Gtk::Button undo_button;
    Gtk::Button redo_button;
    Gtk::Button replace_button;
    Gtk::Button reverse_button;
    Gtk::Button scratch_button;
    Gtk::Button record_button;
    Gtk::Button overdub_button;
    Gtk::Button multiply_button;
    Gtk::Button insert_button;
    Gtk::Button mute_button;

    void mode_button_toggle (Gtk::Button* button, int mode);
    void adjustment_changed (Gtk::Adjustment*, ControlPort);
    void state_button_toggle (Gtk::ToggleButton* button, ControlPort);
    bool update_state ();

    Gtk::Label  state_label;
    Gtk::Label  loop_length_label;
    Gtk::Label  loop_position_label;
    Gtk::Label  cycle_length_label;
    Gtk::Label  loop_free_memory_label;
    Gtk::Label  loop_memory_label;

    Gtk::Adjustment trigger_adjustment;
    Gtk::HScale trigger_slider;
    Gtk::Adjustment wet_level_adjustment;
    Gtk::VScale wet_level_slider;
    Gtk::Adjustment dry_level_adjustment;
    Gtk::VScale dry_level_slider;
    Gtk::Adjustment feedback_adjustment;
    Gtk::HScale feedback_slider;
    Gtk::Adjustment rate_adjustment;
    Gtk::HScale rate_slider;
    Gtk::Adjustment scratch_adjustment;
    Gtk::HScale scratch_slider;

    Gtk::ToggleButton tap_delay_button;
    Gtk::ToggleButton quantize_button;
    Gtk::ToggleButton round_button;

    Gtk::Table mode_table;
    Gtk::Table toggle_table;
    Gtk::Table slider_table;
    Gtk::Table mix_table;
    Gtk::Table state_table;
};

SoopPanel::SoopPanel (SoopInstance& si)
	: instance (si),

	  undo_button ("undo"),
	  redo_button ("redo"),
	  replace_button ("replace"),
	  reverse_button ("reverse"),
	  scratch_button ("scratch"),
	  record_button ("record"),
	  overdub_button ("overdub"),
	  multiply_button ("multiply"),
	  insert_button ("insert"),
	  mute_button ("mute"),

	  trigger_adjustment (0, 0, 1, 0.001, 0.1),
	  trigger_slider (trigger_adjustment),
	  wet_level_adjustment (1, 0, 1, 0.01, 0.1),
	  wet_level_slider (wet_level_adjustment),
	  dry_level_adjustment (1, 0, 1, 0.01, 0.1),
	  dry_level_slider (dry_level_adjustment),
	  feedback_adjustment (1, 0, 1, 0.001),
	  feedback_slider (feedback_adjustment),
	  rate_adjustment (0, 0, 1, 0.001, 0.1),
	  rate_slider (rate_adjustment),
	  scratch_adjustment (0, 0, 1, 0.0001, 0.1),
	  scratch_slider (scratch_adjustment),

	  tap_delay_button ("tap delay"),
	  quantize_button ("quantize"),
	  round_button ("round"),

	  mode_table (4, 3, true),
	  toggle_table (2, 2, true),
	  slider_table (4, 1),
	  mix_table (1, 2),
	  state_table (6, 1)

{
	set_spacing (5);
	set_border_width (5);

	pack_start (state_table, false, false);
	pack_start (mode_table, false, false);
	pack_start (slider_table);
	pack_start (toggle_table, false, false);
	pack_start (mix_table, false, false);

	mode_table.attach (undo_button, 0, 1, 0, 1);
	mode_table.attach (redo_button, 1, 2, 0, 1);
	mode_table.attach (replace_button, 2, 3, 0, 1);
	mode_table.attach (reverse_button, 0, 1, 1, 2);
	mode_table.attach (scratch_button, 1, 2, 1, 2);
	mode_table.attach (record_button, 2, 3, 1, 2);
	mode_table.attach (overdub_button, 0, 1, 2, 3);
	mode_table.attach (multiply_button, 1, 2, 2, 3);
	mode_table.attach (insert_button, 2, 3, 2, 3);
	mode_table.attach (mute_button, 0, 1, 3, 4);

	toggle_table.attach (tap_delay_button, 0, 2, 0, 1);
	toggle_table.attach (quantize_button,  0, 1, 1, 2);
	toggle_table.attach (round_button,     1, 2, 1, 2);

	slider_table.set_row_spacings (5);
	slider_table.attach (trigger_slider, 0, 1, 0, 1);
	slider_table.attach (feedback_slider, 0, 1, 1, 2);
	slider_table.attach (rate_slider, 0, 1, 2, 3);
	slider_table.attach (scratch_slider, 0, 1, 3, 4);

	mix_table.set_col_spacings (14);
	mix_table.attach (wet_level_slider, 0, 1, 0, 1);
	mix_table.attach (dry_level_slider, 1, 2, 0, 1);

	state_table.attach (state_label, 0, 1, 0, 1);
	state_table.attach (loop_length_label, 0, 1, 1, 2);
	state_table.attach (loop_position_label, 0, 1, 2, 3);
	state_table.attach (cycle_length_label, 0, 1, 3, 4);
	state_table.attach (loop_free_memory_label, 0, 1, 4, 5);
	state_table.attach (loop_memory_label, 0, 1, 5, 6);

	undo_button.signal_clicked().connect (bind (slot (*this, &SoopPanel::mode_button_toggle), &undo_button, 0));
	redo_button.signal_clicked().connect (bind (slot (*this, &SoopPanel::mode_button_toggle), &redo_button, 1));
	replace_button.signal_clicked().connect (bind (slot (*this, &SoopPanel::mode_button_toggle), &replace_button, 2));
	reverse_button.signal_clicked().connect (bind (slot (*this, &SoopPanel::mode_button_toggle), &reverse_button, 3));
	scratch_button.signal_clicked().connect (bind (slot (*this, &SoopPanel::mode_button_toggle), &scratch_button, 4));
	record_button.signal_clicked().connect (bind (slot (*this, &SoopPanel::mode_button_toggle), &record_button, 5));
	overdub_button.signal_clicked().connect (bind (slot (*this, &SoopPanel::mode_button_toggle), &overdub_button, 6));
	multiply_button.signal_clicked().connect (bind (slot (*this, &SoopPanel::mode_button_toggle), &multiply_button, 7));
	insert_button.signal_clicked().connect (bind (slot (*this, &SoopPanel::mode_button_toggle), &insert_button, 8));
	mute_button.signal_clicked().connect (bind (slot (*this, &SoopPanel::mode_button_toggle), &mute_button, 9));

	trigger_adjustment.signal_value_changed().connect (bind (slot (*this, &SoopPanel::adjustment_changed),
							&trigger_adjustment, TriggerThreshold));
	wet_level_adjustment.signal_value_changed().connect (bind (slot (*this, &SoopPanel::adjustment_changed),
							  &wet_level_adjustment, WetLevel));
	dry_level_adjustment.signal_value_changed().connect (bind (slot (*this, &SoopPanel::adjustment_changed),
							  &dry_level_adjustment, DryLevel));
	feedback_adjustment.signal_value_changed().connect (bind (slot (*this, &SoopPanel::adjustment_changed),
							 &feedback_adjustment, Feedback));
	rate_adjustment.signal_value_changed().connect (bind (slot (*this, &SoopPanel::adjustment_changed),
						     &rate_adjustment, Rate));
	scratch_adjustment.signal_value_changed().connect (bind (slot (*this, &SoopPanel::adjustment_changed),
							&scratch_adjustment, ScratchPosition));

	tap_delay_button.signal_toggled().connect (bind (slot (*this, &SoopPanel::state_button_toggle), &tap_delay_button, TapDelayTrigger));
	quantize_button.signal_toggled().connect (bind (slot (*this, &SoopPanel::state_button_toggle), &quantize_button, Quantize));
	round_button.signal_toggled().connect (bind (slot (*this, &SoopPanel::state_button_toggle), &round_button, Round));
	
	
	// this is needed to allow more fine control on slider
	feedback_slider.set_digits (3);
	wet_level_slider.set_digits (2);
	dry_level_slider.set_digits (2);
	rate_slider.set_digits (3);
	scratch_slider.set_digits (4);
	trigger_slider.set_digits (3);
	
	show_all ();

	Glib::signal_timeout().connect( slot(*this, &SoopPanel::update_state), 100);
	
}

SoopPanel::~SoopPanel ()
{
}

bool
SoopPanel::update_state ()
{
	//printf("Update state\n");
	
	int state = (int) instance.ports[State];

	switch (state) {
	case 0:
		state_label.set_text ("Off");
		break;
	case 1:
		state_label.set_text ("Record Wait");
		break;
	case 2:
		state_label.set_text ("Record");
		break;
	case 3:
		state_label.set_text ("Stop Wait");
		break;
	case 4:
		state_label.set_text ("Play");
		break;
	case 5:
		state_label.set_text ("Overdub");
		break;
	case 6:
		state_label.set_text ("Multiply");
		break;
	case 7:
		state_label.set_text ("Insert");
		break;
	case 8:
		state_label.set_text ("Replace");
		break;
	case 9:
		state_label.set_text ("Tap Delay");
		break;
	case 10:
		state_label.set_text ("Mute");
		break;
	case 11:
		state_label.set_text ("Scratch");
		break;
	case 12:
		state_label.set_text ("One Shot");
		break;
	}

	char buf[32];

	snprintf (buf, sizeof (buf), "%.2f secs", instance.ports[LoopLength]);
	loop_length_label.set_text (buf);

	snprintf (buf, sizeof (buf), "%.2f secs", instance.ports[LoopPosition]);
	loop_position_label.set_text (buf);

	snprintf (buf, sizeof (buf), "%.2f secs", instance.ports[CycleLength]);
	cycle_length_label.set_text (buf);

	snprintf (buf, sizeof (buf), "%.2f secs", instance.ports[LoopFreeMemory]);
	loop_free_memory_label.set_text (buf);

	snprintf (buf, sizeof (buf), "%.2f secs", instance.ports[LoopMemory]);
	loop_memory_label.set_text (buf);

	return true;
}

void
SoopPanel::state_button_toggle (Gtk::ToggleButton* button, ControlPort port)
{
	instance.set_port (port, button->get_active() ? 1.0 : 0.0);
}

void
SoopPanel::mode_button_toggle (Gtk::Button* button, int mode)
{
	instance.request_mode (mode);
}

void
SoopPanel::adjustment_changed (Gtk::Adjustment* adj, ControlPort port)
{
	instance.set_port (port, adj->get_value());
}

/*----------------------------------------------------------------------*/

class Soop : public Gtk::Window
{
  public:
    Soop ();
    ~Soop ();

    bool operator() () const { return _ok; }
    void new_loop();

  protected:
    gint delete_event_impl (GdkEventAny*);

  private:
    Gtk::VBox     vpacker;
    Gtk::MenuBar  menubar;
    Gtk::Notebook notebook;

    bool _ok;
    jack_client_t *jack;
    const LADSPA_Descriptor* sooper_looper_descriptor;

    int connect_to_jack ();

    int process_callback (jack_nframes_t);
    static int _process_callback (jack_nframes_t, void*);

    typedef vector<SoopInstance*> Instances;
    Instances instances;
    pthread_mutex_t instance_lock;
};

Soop::Soop ()
	: Gtk::Window::Window (Gtk::WINDOW_TOPLEVEL)
{
	using namespace Gtk::Menu_Helpers;

	_ok = false;

	if (connect_to_jack ()) {
		return;
	}

	pthread_mutex_init (&instance_lock, 0);
	

	set_title ("soop");
	set_default_size (800, 300);

	MenuList& items = menubar.items();
	items.push_back (MenuElem ("New Looper", slot (*this, &Soop::new_loop)));

	vpacker.pack_start (menubar, false, false);
	vpacker.pack_start (notebook, true, true);

	add (vpacker);

	show_all ();

	sooper_looper_descriptor = ladspa_descriptor (0);
	
	if (jack_activate (jack)) {
		cerr << "cannot activate JACK\n";
		return;
	}

	new_loop();
	
	_ok = true;
}

Soop::~Soop ()
{

	jack_client_close (jack);
}

void
Soop::new_loop ()
{
	SoopPanel* panel;
	SoopInstance* instance;
	using namespace Gtk::Notebook_Helpers;

	instance = new SoopInstance (jack, sooper_looper_descriptor);

	if (!(*instance)()) {
		cerr << "can't create a new loop!\n";
		delete instance;
		return;
	}

	panel = new SoopPanel (*instance);
	notebook.pages().push_back (TabElem (*panel, "Loop N"));

	pthread_mutex_lock (&instance_lock);
	instances.push_back (instance);
	pthread_mutex_unlock (&instance_lock);
}

int
Soop::connect_to_jack ()
{
	if ((jack = jack_client_new ("soop")) == 0) {
		return -1;
	}

	if (jack_set_process_callback (jack, _process_callback, this) != 0) {
		return -1;
	}

	return 0;
}

gint
Soop::delete_event_impl (GdkEventAny* ev)
{
	Gtk::Main::quit ();
	return TRUE;
}

int
Soop::_process_callback (jack_nframes_t nframes, void* arg)
{
	return static_cast<Soop*> (arg)->process_callback (nframes);
}

int
Soop::process_callback (jack_nframes_t nframes)
{
	pthread_mutex_lock (&instance_lock);
	for (Instances::iterator i = instances.begin(); i != instances.end(); ++i) {
		(*i)->run (nframes);
	}
	pthread_mutex_unlock (&instance_lock);
	return 0;

}

int
main (int argc, char *argv[])
{
	Gtk::Main ui (&argc, &argv);

	sl_init ();

	Soop soop;

	if (!soop()) {
		cerr << "cannot initialize soop\n";
		exit (1);
	}

	ui.run ();

	sl_fini ();
}
