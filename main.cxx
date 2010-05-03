// configuration utility for bu0836 joystick controllers
//
// Copyright (C) 2010  Melchior FRANZ  <melchior.franz@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
#include <iomanip>
#include <sstream>

#include "bu0836.hxx"
#include "logging.hxx"
#include "options.h"

#define EMAIL "<melchior.franz@gmail.com>"

using namespace std;
using namespace logging;



namespace {

void help(void)
{
	//      |---------1---------2---------3---------4---------5---------6---------7---------8
	cout << "Usage: bu0836 [OPTION]..." << endl;
	cout << endl;
	cout << "  -h, --help               show this help screen and exit" << endl;
	cout << "      --version            show version number and exit" << endl;
	cout << "  -v, --verbose            increase verbosity level (can be used three times)" << endl;
	cout << "  -l, --list               list BU0836 devices" << endl;
	cout << endl;
	cout << "Device options:" << endl;
	cout << "  -d, --device=STRING      select device by bus id or (ending of) serial number" << endl;
	cout << "  -s, --status             show current device configuration" << endl;
	cout << "  -m, --monitor            monitor device output (terminate with Ctrl-c)" << endl;
	cout << "  -r, --reset              reset device configuration to \"factory default\"" << endl;
	cout << "                           (equivalent of -a0-7 -i0 -z0 -b0-31 -e0 -p6)" << endl;
	cout << "  -y, --sync               write current changes to the controller's EEPROM" << endl;
	cout << "  -O, --save=FILE          save EEPROM image buffer to file <s>" << endl;
	cout << "  -I, --load=FILE          load EEPROM image from file <s> and flash EEPROM" << endl;
	cout << "  -X, --dump               display EEPROM image buffer" << endl;
	cout << endl;
	cout << "Axis options:" << endl;
	cout << "  -a, --axes=LIST          select axes (overrides prior axis selection)" << endl;
	cout << "  -i, --invert=STRING      set inverted mode (\"on\" and \"off\", or 1 and 0)" << endl;
	cout << "  -z, --zoom=STRING        set zoom mode/factor (\"on\" (198), \"off\" (0))" << endl;
	cout << endl;
	cout << "Encoder options:" << endl;
	cout << "  -b, --buttons=LIST       select buttons (overrides prior button selection)" << endl;
	cout << "  -e, --encoder=STRING     set encoder mode for selected buttons:" << endl;
	cout << "                           \"off\" or 0 for normal button function," << endl;
	cout << "                           \"1:1\" or 1 for quarter wave encoder," << endl;
	cout << "                           \"1:2\" or 2 for half wave encoder," << endl;
	cout << "                           \"1:4\" or 3 for full wave encoder" << endl;
	cout << "  -p, --pulse-width=STRING set pulse width for all encoders (number from 1-11" << endl;
	cout << "                           or number from 8-88 followed by \"ms\")" << endl;
	cout << endl << endl;
	cout << "  LIST is a number or number range, or a list thereof separated by commas," << endl;
	cout << "       e.g. \"0\" or \"1,3,5\" or \"0-5\" or \"0,2,6-10,30\"" << endl;
	cout << endl;
	cout << "Report bugs to " EMAIL << endl;
	//      |---------1---------2---------3---------4---------5---------6---------7---------8
}



void version(void)
{
#ifdef TAG
	cout << "bu0836 "STRINGIZE(TAG)" ("STRINGIZE(SHA)")";
#if MOD
	cout << "++";
#endif
	cout << endl;
#else
	cout << "??" << endl;
#endif
	cout << "Copyright (C) Melchior FRANZ " EMAIL << endl;
	cout << "License GPLv2+: GNU GPL version 2 or later <http://gnu.org/licenses/gpl-2.0.html>" << endl;
	cout << "This is free software; see the source for copying conditions.  There is NO" << endl;
	cout << "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE." << endl;
}



uint32_t numlist_to_bitmap(const char *list, unsigned int max = 31)
{
	const unsigned int undef = ~0u;
	unsigned int low = undef, high = undef;
	uint32_t bitmap = 0u;
	bool range = false;
	istringstream s(list);

	while (1) {
		int next = s.get();
		if (s.eof() || next == ',') {
			if (range)
				throw string("incomplete range in number list");

			if (low != undef) {
				if (high == undef)
					high = low;
				for (unsigned int i = low; i <= high; i++)
					bitmap |= 1 << i;
			}

			if (s.eof())
				break;

			low = high = undef;

		} else if (isdigit(next)) {
			s.putback(next);
			unsigned int i;
			s >> i;
			if (i > max)
				throw string("number in axis/button list out of range");

			if (!range && low == undef)
				low = i;
			else if (range && high == undef)
				high = i;
			else
				throw string("unexpected number in list");
			range = false;

		} else if (next == '-') {
			if (low == undef || range)
				throw string("unexpected range in number list");
			range = true;

		} else if (!isspace(next)) {
			throw string("malformed number list");
		}
	}
	return bitmap;
}



void list_devices(bu0836::manager& dev)
{
	for (size_t i = 0; i < dev.size(); i++) {
		const char *marker = &dev[i] == dev.selected() ? " <<" : "";
		cout << magenta << dev[i].bus_address() << reset
				<< "\t" << dev[i].manufacturer()
				<< ", " << dev[i].product()
				<< ", " << magenta << dev[i].serial() << reset
				<< ", v" << dev[i].release()
				<< green << marker << reset
				<< endl;
	}
}



void print_status(bu0836::controller *c)
{
	cout << bg_cyan << bold << white << ' ' << setw(62) << left << c->jsid() << right << reset << endl << endl;

	if (c->capabilities() & bu0836::CONFIG) {
		cout << bold << black << "_____________________________ Axes ____________________________"
				<< reset << endl << endl;
		cout << "       ";
		for (int i = 0; i < 8; i++)
			if (c->active_axes() & (1 << i))
				cout << bold << "     #" << i << reset;
			else
				cout << bold << black << "      " << i << reset;
		cout << endl;

		cout << "inverted:    ";
		for (int i = 0; i < 8; i++)
			if (c->get_invert(i))
				cout << red << "I      ";
			else
				cout << green << "-      ";
		cout << reset << endl;

		cout << "zoom:  ";
		for (int i = 0; i < 8; i++) {
			int zoom = c->get_zoom(i);
			cout << (zoom ? red : green) << setw(7) << zoom;
		}
		cout << reset << endl;

		if (c->capabilities() & bu0836::ENCODER)
			cout << endl << endl;
	}

	if (c->capabilities() & bu0836::ENCODER) {
		const char *s[4] = { " -   -  ", "\\_1:1_/ ", "\\_1:2_/ ", "\\_1:4_/ " };
		cout << bold << black << "_______________________ Buttons/Encoders ______________________"
				<< reset << endl << endl;
		cout << "#00 #01 #02 #03 #04 #05 #06 #07 #08 #09 #10 #11 #12 #13 #14 #15" << endl;
		for (int i = 0; i < 16; i += 2) {
			int m = c->get_encoder_mode(i);
			cout << (m ? red : green) << s[m];
		}
		cout << reset << endl << endl;

		cout << "#16 #17 #18 #19 #20 #21 #22 #23 #24 #25 #26 #27 #28 #29 #30 #31" << endl;
		for (int i = 16; i < 32; i += 2) {
			int m = c->get_encoder_mode(i);
			cout << (m ? red : green) << s[m];
		}
		cout << reset << endl << endl;

		cout << "pulse width: ";
		int pulse = c->get_pulse_width();
		cout << (pulse == 6 ? green : red) << pulse * 8 << " ms" << reset << endl;
	}
}



void commit_changes(bu0836::manager& dev)
{
	for (size_t i = 0; i < dev.size(); i++) {
		if (!dev[i].is_dirty())
			continue;

		cerr << endl;
		print_status(&dev[i]);
		cout << endl;
		int key;
		do {
			cerr << cyan << "Write configuration to controller? [y/N] " << reset;
			key = cin.get();
			cin.clear();
			if (key == '\n')
				key = 'n';
			else
				cin.ignore(80, cin.widen('\n'));
		} while (!cin.fail() && key != 'n' && key != 'N' && key != 'y' && key != 'Y');

		if (key == 'y' || key == 'Y')
			dev[i].sync();
	}
}

} // namespace



int main(int argc, const char *argv[]) try
{
	enum {
		HELP_OPTION, VERSION_OPTION, VERBOSE_OPTION,
		LIST_OPTION, DEVICE_OPTION, STATUS_OPTION, MONITOR_OPTION,
		RESET_OPTION, SYNC_OPTION, SAVE_OPTION, LOAD_OPTION, DUMP_OPTION,
		AXES_OPTION, INVERT_OPTION, ZOOM_OPTION,
		BUTTONS_OPTION, ENCODER_OPTION, PULSEWIDTH_OPTION,
	};

	const struct command_line_option options[] = {
		{ "--help",        "-h", 0, "\0" },
		{ "--version",        0, 0, "\0" },
		{ "--verbose",     "-v", 0, "\0" },
		{ "--list",        "-l", 0, "\0" },
		{ "--device",      "-d", 1, "\0" },
		//
		{ "--status",      "-s", 0, "d" },
		{ "--monitor",     "-m", 0, "d" },
		{ "--reset",       "-r", 0, "d" },
		{ "--sync",        "-y", 0, "d" },
		{ "--save",        "-O", 1, "d" },
		{ "--load",        "-I", 1, "d" },
		{ "--dump",        "-X", 0, "d" },
		//
		{ "--axes",        "-a", 1, "\0" },
		{ "--invert",      "-i", 1, "a" },
		{ "--zoom",        "-z", 1, "a" },
		//
		{ "--buttons",     "-b", 1, "\0" },
		{ "--encoder",     "-e", 1, "b" },
		{ "--pulse-width", "-p", 1, "e" },
		OPTIONS_LAST
	};

	// option extensions:
	// "d" ... requires device
	// "a" ... requires axis support & selection
	// "b" ... requires encoder support & button selection
	// "e" ... requires encoder support

	int option;
	struct option_parser_context ctx;

	// first pass options
	set_log_level(WARN);
	init_options_context(&ctx, argc, argv, options);
	while ((option = get_option(&ctx)) != OPTIONS_DONE) {
		if (option == HELP_OPTION) {
			help();
			return EXIT_SUCCESS;

		} else if (option == VERSION_OPTION) {
			version();
			return EXIT_SUCCESS;

		} else if (option == VERBOSE_OPTION) {
			set_log_level(get_log_level() - 1);
		}
	}

	bu0836::manager dev;
	uint32_t selected_axes = 0;
	uint32_t selected_buttons = 0;

	// second pass options
	init_options_context(&ctx, argc, argv, options);
	while ((option = get_option(&ctx)) != OPTIONS_DONE) {

		// check for option requirements
		if (option >= 0) {
			char req = options[option].ext[0];
			if (req) {
				if (dev.empty())
					throw string("no BU0836 device found");
				if (!dev.selected())
					throw string("you need to select a device before you can use the ")
							+ options[option].long_opt + " option, for\n       example with -d"
							+ dev[0].bus_address() + " or -d" + dev[0].serial()
							+ ". Use the --list option for available devices.";
				if (dev.selected()->claim())
					throw string("cannot access device '") + dev[0].serial() + '\'';
			}

			if (req == 'a') {
				if (!(dev.selected()->capabilities() & bu0836::CONFIG))
					throw string("device '") + dev.selected()->serial()
							+ "' doesn't support axis configuration";
				if (!selected_axes)
					throw string("no axes selected for ") + options[option].long_opt + " option";
			}

			if (req == 'b' || req == 'e')
				if (!(dev.selected()->capabilities() & bu0836::ENCODER))
					throw string("device '") + dev.selected()->serial()
							+ "' doesn't support button/encoder configuration";

			if (req == 'b' && !selected_buttons)
					throw string("no buttons selected for ") + options[option].long_opt + " option";
		}

		switch (option) {
		case LIST_OPTION:
			list_devices(dev);
			break;

		case DEVICE_OPTION: {
			int num = dev.select(ctx.argument);
			if (num == 1)
				log(INFO) << "selecting device '" << dev.selected()->serial() << '\'' << endl;
			else if (num)
				throw string("ambiguous device specifier");
			else
				throw string("no matching device found");
			break;
		}

		case STATUS_OPTION:
			print_status(dev.selected());
			break;

		case MONITOR_OPTION:
			dev.selected()->show_input_reports();
			break;

		case RESET_OPTION:
			log(INFO) << "resetting configuration to \"factory default\"" << endl;
			if (dev.selected()->capabilities() & bu0836::CONFIG) {
				for (int i = 0; i < 8; i++) {
					dev.selected()->set_invert(i, false);
					dev.selected()->set_zoom(i, 0);
				}
			}
			if (dev.selected()->capabilities() & bu0836::ENCODER) {
				for (int i = 0; i < 32; i += 2)
					dev.selected()->set_encoder_mode(i, 0);
				dev.selected()->set_pulse_width(6);
			}
			break;

		case SYNC_OPTION:
			log(INFO) << "write changes to EEPROM" << endl;
			dev.selected()->sync();
			break;

		case SAVE_OPTION:
			log(INFO) << "saving image to file '" << ctx.argument << '\'' << endl;
			if (!dev.selected()->get_eeprom() && !dev.selected()->save_image_file(ctx.argument))
				log(INFO) << "saved" << endl;
			break;

		case LOAD_OPTION:
			log(INFO) << "loading image from file '" << ctx.argument << '\'' << endl;
			if (!dev.selected()->load_image_file(ctx.argument) && !dev.selected()->set_eeprom(0x00, 0xff))
				log(INFO) << "loaded" << endl;
			break;

		case DUMP_OPTION:
			cout << dev.selected()->jsid() << endl << magenta << "-- " << hex << setfill('0');
			for (int i = 0; i < 16; i++)
				cout << setw(2) << i << ' ';
			cout << reset << endl;
			for (int i = 0; i < 16; i++)
				cout << magenta << setw(2) << i * 16 << ' ' << reset
						<< bytes(dev.selected()->eeprom() + i * 16, 16) << endl;
			cout << dec << endl;
			break;

		case AXES_OPTION:
			selected_axes = numlist_to_bitmap(ctx.argument, 7);
			log(INFO) << "selecting axes 0x" << hex << selected_axes << dec << endl;
			break;

		case INVERT_OPTION: {
			log(INFO) << "setting axes to inverted=" << ctx.argument << endl;
			string arg = ctx.argument;
			bool invert;
			if (arg == "0" || arg == "off")
				invert = false;
			else if (arg == "1" || arg == "on")
				invert = true;
			else
				throw string("invalid argument to --invert: use \"on\"/1 or \"off\"/0");
			for (uint32_t i = 0; i < 8; i++)
				if (selected_axes & (1 << i))
					dev.selected()->set_invert(i, invert);
			break;
		}

		case ZOOM_OPTION: {
			string arg = ctx.argument;
			int zoom = -1;
			if (arg == "off") {
				zoom = 0;
			} else if (arg == "on") {
				zoom = 198;
			} else {
				istringstream x(ctx.argument);
				x >> zoom;
				if (x.fail() || !x.eof() || zoom < 0 || zoom > 255)
					throw string("invalid argument to --zoom: use \"on\"/198, \"off\"/0, "
							"or number in range 0-255");
			}
			log(INFO) << "setting axes to zoom=" << zoom << endl;
			for (uint32_t i = 0; i < 8; i++)
				if (selected_axes & (1 << i))
					dev.selected()->set_zoom(i, zoom);
			break;
		}

		case BUTTONS_OPTION:
			selected_buttons = numlist_to_bitmap(ctx.argument, 31);
			log(INFO) << "selecting buttons 0x" << hex << selected_buttons << dec << endl;
			break;

		case ENCODER_OPTION: {
			string arg = ctx.argument;
			int enc;
			if (arg == "off" || arg == "0")
				enc = 0;
			else if (arg == "1:1" || arg == "1")
				enc = 1;
			else if (arg == "1:2" || arg == "2")
				enc = 2;
			else if (arg == "1:4" || arg == "3")
				enc = 3;
			else
				throw string("invalid argument to --encoder: "
						"use \"off\"/0, \"1:1\"/1, \"1:2\"/2, or \"1:4\"/3");
			log(INFO) << "configuring buttons for encoder mode " << enc << endl;
			for (uint32_t i = 0; i < 31; i++)
				if (selected_buttons & (1 << i))
					dev.selected()->set_encoder_mode(i, enc);
			break;
		}

		case PULSEWIDTH_OPTION: {
			istringstream x(ctx.argument);
			unsigned int p;
			x >> p;
			if (!x.eof()) {
				string ms;
				x >> ms;
				if (ms != "ms")
					throw string("invalid argument to --pulse-width: must be integer in the range "
							"1-11, or in the range 8-88 and followed by \"ms\", e.g. 48ms");
				if (p < 8)
					p = 8;
				else if (p > 88)
					p = 88;
				p = (p + 4) / 8;
			} else if (p < 1 || p > 11) {
				throw string("requested pulse width out of range (1-11)");
			}

			log(INFO) << "pulse width = " << p << "  (" << p * 8 << " ms)" << endl;
			dev.selected()->set_pulse_width(p);
			break;
		}

		// ignored options
		case HELP_OPTION:
		case VERSION_OPTION:
		case VERBOSE_OPTION:

		// signals and errors
		case OPTIONS_TERMINATOR:
			break;

		case OPTIONS_ARGUMENT:
			throw string("don't know what to do with an argument '") + ctx.option + '\'';

		case OPTIONS_EXCESS_ARGUMENT:
			throw string("illegal option assignment '") + ctx.argument + '\'';

		case OPTIONS_UNKNOWN_OPTION:
			throw string("unknown option '") + ctx.option + '\'';

		case OPTIONS_MISSING_ARGUMENT:
			throw string("missing argument for option '") + ctx.option + '\'';

		default:
			log(ALERT) << "this can't happen: " << option << '/' << ctx.option << endl;
			return EXIT_FAILURE;
		}
	}

	commit_changes(dev);
	return EXIT_SUCCESS;

} catch (const string &msg) {
	log(ALERT) << "Error: " << msg << endl;
	return EXIT_FAILURE;
}
