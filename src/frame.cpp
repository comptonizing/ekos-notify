#include "frame.h"
#include <exception>
#include <memory>
#include <opencv2/imgcodecs.hpp>
#include <stdexcept>

namespace EN {

FrmMain::FrmMain(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& refGlade) :
		Gtk::ApplicationWindow(cobject), builder(refGlade) {
	builder->get_widget("textLog", m_tvLog);
	m_dbus = Gio::DBus::Connection::get_sync(Gio::DBus::BUS_TYPE_SESSION);
	if ( ! m_dbus ) {
		showError("DBUS Error", "Error connecting to DBUS!");
		exit(EXIT_FAILURE);
	}
	m_logBuffer = Gtk::TextBuffer::create();
	m_tvLog->set_buffer(m_logBuffer);
	signal_delete_event().connect(sigc::mem_fun(*this, &FrmMain::quit));
	// m_httpClient = std::make_unique<httplib::Client>("https://app.lightbucket.co:443");
}

FrmMain::~FrmMain() {
}

bool FrmMain::quit(_GdkEventAny* event) {
	return true;
}

void FrmMain::showError(Glib::ustring title, Glib::ustring message, Glib::ustring secondaryMessage) {
	m_dialog.reset(new Gtk::MessageDialog(*this, message, false,
				Gtk::MessageType::MESSAGE_ERROR, Gtk::ButtonsType::BUTTONS_OK, true));
	m_dialog->set_title(title);
	if ( secondaryMessage != Glib::ustring("") ) {
		m_dialog->set_secondary_text(secondaryMessage);
	}
	m_dialog->set_modal(true);
	m_dialog->signal_response().connect(sigc::hide(sigc::mem_fun(*m_dialog, &Gtk::Widget::hide)));
	m_dialog->show();
}

void FrmMain::log(const std::string &msg, bool showTimestamp) {
	static sigc::connection conn;
	m_logMutex.lock();
	std::string str = "";
	if ( showTimestamp ) {
		char timeStamp[32];
		time_t rawTime;
		struct tm *timeInfo;
		time(&rawTime);
		timeInfo = localtime(&rawTime);
		strftime(timeStamp, sizeof(timeStamp), "%H:%M:%S: ", timeInfo);
		str = std::string(timeStamp);
	}
	str = str + msg;
	conn.disconnect();
	conn = m_logDispatcher.connect([this, str]() mutable {
		m_logBuffer->insert(m_logBuffer->end(), str);
		while (gtk_events_pending()) {
			gtk_main_iteration_do(false);
		}
		auto it = m_logBuffer->get_iter_at_line(m_logBuffer->get_line_count());
		m_tvLog->scroll_to(it);
		m_logMutex.unlock();
	});
	m_logDispatcher();
	return;
}


}
