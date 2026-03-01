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
        builder->get_widget("buttonENHelp", m_buttonHelp);
        builder->get_widget("buttonENSave", m_buttonSave);
        builder->get_widget("entryENURL", m_entryURL);
        builder->get_widget("entryENToken", m_entryToken);

        m_buttonHelp->signal_clicked().connect(sigc::mem_fun(*this, &FrmMain::help));
        m_buttonSave->signal_clicked().connect(sigc::mem_fun(*this, &FrmMain::saveConfig));

        initConfig();

	// m_httpClient = std::make_unique<httplib::Client>("https://app.lightbucket.co:443");
}

FrmMain::~FrmMain() {
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

void FrmMain::initConfig() {
        std::string url, token, more;
        const gchar *args[3];
        args[0] = g_get_user_config_dir();
        args[1] = "ekosnotify.conf";
        args[2] = NULL;
        m_configFile = std::string(g_build_filenamev((gchar **) args));
        std::ifstream stream(m_configFile);
        if ( ! stream.is_open() ) {
                log("No exiting configuration available\n");
                return;
        }
        std::getline(stream, url);
        std::getline(stream, token);
        std::getline(stream, more);
        if ( url == "" || token == "" || ! stream.eof()) {
                log("Corrupted configuration detected\n");
                char buff[512];
                snprintf(buff, sizeof(buff),
                                "Your configuration file (%s) is corrupted and will be deleted. "
                                "You will have to re-enter your url and token.",
                                m_configFile.c_str());
                showError("Corrupted Configuration", buff);
                stream.close();
                std::remove(m_configFile.c_str());
                return;
        }
        m_entryURL->set_text(url);
        m_entryToken->set_text(token);
        log("Found existing configuration\n");
}

void FrmMain::saveConfig() {
        if ( m_entryURL->get_text() == "" || m_entryToken->get_text() == "" ) {
                showError("Configuration Error",
                                "You need to enter the username and API key in order to save it");
                return;
        }
        log("Saving configuration\n");
        std::ofstream stream(m_configFile);
        stream << m_entryURL->get_text() << std::endl;
        stream << m_entryToken->get_text() << std::endl;
        stream.close();
}

void FrmMain::help() {
        Glib::ustring msg = "You need to give a full ULR for your gotify "
            "instance and the token of the application used to send the push "
            "notifications for"
            ;
        m_dialog.reset(new Gtk::MessageDialog(*this, msg, false,
                                Gtk::MessageType::MESSAGE_INFO, Gtk::ButtonsType::BUTTONS_CLOSE,
                                true));
        m_dialog->set_title("Gotify Help");
        m_dialog->set_modal(true);
        m_dialog->signal_response().connect([this](int response) {
                std::ignore = response;
                        m_dialog->hide();
                        });
        m_dialog->show();
}

}
