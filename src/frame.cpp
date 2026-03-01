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
        builder->get_widget("buttonENTest", m_buttonTest);
        builder->get_widget("buttonENSave", m_buttonSave);
        builder->get_widget("entryENURL", m_entryURL);
        builder->get_widget("entryENToken", m_entryToken);

        m_buttonTest->signal_clicked().connect([&]() {
                std::thread([&]() {
                        test();
                        }).detach();
                });


                // sigc::mem_fun(*this, &FrmMain::test));
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

void FrmMain::test() {
    if ( ! gotifyOK() ) {
        log("Gotify settings missing\n");
    }
    log("Testing gotify\n");
    try {
        std::string title = "Ekos test message";
        std::string msg = "This is a test from ekos notify";
        push(title, msg, 5);
    } catch (std::runtime_error &e) {
        log("Failure: ");
        log(e.what(), false);
        log("\n", false);
        return;
    } catch (...) {
        log("Failure\n");
        return;
    }
    log("Success\n");
}

bool FrmMain::gotifyOK() {
    return m_entryURL->get_text() != "" && m_entryToken->get_text() != "";
}

void FrmMain::push(std::string &title, std::string &msg, int priority) {
    if ( ! gotifyOK () ) {
        throw std::runtime_error("Gotify settings missing");
    }
    nlohmann::json json;
    json["message"] = msg;
    json["title"] = title;
    json["priority"] = priority;
    json["extras"]["client::display"]["contentType"] = "text/markdown";

    char buffURL[256];
    snprintf(buffURL, sizeof(buffURL)-1, "/message?token=%s", m_entryToken->get_text().c_str());

    auto client = httplib::Client(m_entryURL->get_text());
    client.set_follow_location(true);
    httplib::Headers headers = {
            {"AContent-Type", "application/json"}
    };
    auto result = client.Post(buffURL, json.dump(), "application/json");
    if ( ! result ) {
        auto error = result.error();
        throw std::runtime_error(std::string("Error posting data to server: ") + httplib::to_string(error));
    }
    if ( result->status != httplib::StatusCode::OK_200 ) {
                char buff[256];
                snprintf(buff, sizeof(buff), "Got HTTP response %d instead of 200", result->status);
                throw std::runtime_error(buff);
    }
}

}
