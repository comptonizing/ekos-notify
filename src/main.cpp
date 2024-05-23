#include "frame.h"

int main(int argc, char *argv[]) {
	Gtk::Main kit(argc, argv);
	EN::FrmMain *frm = nullptr;
	try {
	Glib::RefPtr<Gtk::Builder> builder =
		Gtk::Builder::create_from_string(__guiData);
		builder->get_widget_derived("main", frm);
	} catch ( const Glib::Error &e ) {
		std::cerr << std::string("Could not run the main program: ") + e.what() << std::endl;
		exit(EXIT_FAILURE);
	} catch ( const std::exception &e) {
		std::cerr << std::string("Could not run the main program: ") + e.what() << std::endl;
		exit(EXIT_FAILURE);
	} catch ( ... ) {
		std::cerr << std::string("Could not run the main program: Unknown error") << std::endl;
		exit(EXIT_FAILURE);
	}
	kit.run(*frm);
	delete frm;
}
