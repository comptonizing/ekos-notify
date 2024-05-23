#pragma once

#include <iostream>
#include <queue>
#include <mutex>
#include <thread>
#include <chrono>
#include <ctime>
#include <fstream>
#include <cstdio>
#include <exception>

#include <gtkmm.h>
#include <glib.h>
#include <gtkmm/builder.h>
#include <glibmm/fileutils.h>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "json.hpp"

#include "gui.h"

namespace EN {

	using namespace std::chrono_literals;

	class FrmMain : public Gtk::ApplicationWindow {
		public:
			~FrmMain();
			FrmMain(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& refGlade);
		private:
			void showError(Glib::ustring title, Glib::ustring message, Glib::ustring secondaryMessage = "");
			void log(const std::string &msg, bool showTimestamp = true);
			bool quit(_GdkEventAny* event);

			Glib::RefPtr<Gtk::Builder> builder;
			Glib::RefPtr<Gio::DBus::Connection> m_dbus;
			Glib::RefPtr<Gio::DBus::Proxy> m_proxy;
			Glib::RefPtr<Gtk::TextBuffer> m_logBuffer;
			std::unique_ptr<Gtk::MessageDialog> m_dialog;
			Gtk::TextView *m_tvLog;
			Glib::Dispatcher m_logDispatcher;

			std::mutex m_logMutex;

			std::unique_ptr<httplib::Client> m_httpClient = nullptr;
	};
}
