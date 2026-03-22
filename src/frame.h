#pragma once

#include "glibmm/variant.h"
#include <memory>
#include <iostream>
#include <queue>
#include <mutex>
#include <thread>
#include <chrono>
#include <ctime>
#include <fstream>
#include <cstdio>
#include <exception>
#include <stdexcept>

#include <gtkmm.h>
#include <glib.h>
#include <gtkmm/builder.h>
#include <glibmm/fileutils.h>
#include <exception>
#include <unordered_map>


#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "json.hpp"

#include "gui.h"
#include "settings.h"

namespace EN {

	using namespace std::chrono_literals;

        class NotificationSettingsWindow : public Gtk::ApplicationWindow {
            public:
                NotificationSettingsWindow(
                        std::vector<Glib::ustring> &labels,
                        std::vector<int> &levels,
                        std::vector<bool> &enabled
                        );
            sigc::signal<void()> signal_value_chagned() {
                return m_signal_value_changed;
            }
            private:
                Gtk::ScrolledWindow m_scrolledWindow;
                Gtk::Grid m_grid;
                bool onSwitchChange(bool state);
                sigc::signal<void()> m_signal_value_changed;
        };

	class FrmMain : public Gtk::ApplicationWindow {
		public:
			~FrmMain();
			FrmMain(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& refGlade);

		private:
			void showError(Glib::ustring title, Glib::ustring message, Glib::ustring secondaryMessage = "");
			void log(const std::string &msg, bool showTimestamp = true);
                        void initConfig();
                        void saveConfig();
                        void test();
                        bool gotifyOK();
                        void push(std::string &title, std::string &msg, int priority);

			Glib::RefPtr<Gtk::Builder> builder;
			Glib::RefPtr<Gio::DBus::Connection> m_dbus;
			Glib::RefPtr<Gio::DBus::Proxy> m_proxy;
			Glib::RefPtr<Gtk::TextBuffer> m_logBuffer;
			std::unique_ptr<Gtk::MessageDialog> m_dialog;
			Gtk::TextView *m_tvLog;
			Glib::Dispatcher m_logDispatcher;
                        Gtk::Button *m_buttonTest, *m_buttonSave, *m_buttonSettings;
                        Gtk::Entry *m_entryURL, *m_entryToken;

                        std::string m_settingsFile, m_credentialsFile;
			std::mutex m_logMutex, m_signalMutex, m_settingsWriteMutex;

                        std::unique_ptr<NotificationSettingsWindow> m_settingsWindow = nullptr;

                        std::vector<Glib::ustring> m_notificationIds;
                        std::vector<Glib::ustring> m_notificationDescriptions;
                        std::vector<int> m_notificationPriorities;
                        std::vector<bool> m_notificationEnabled;

                        std::vector<Glib::ustring> m_defaultNotificationIds;
                        std::vector<Glib::ustring> m_defaultNotificationDescriptions;
                        std::vector<int> m_defaultNotificationPriorities;
                        std::vector<bool> m_defaultNotificationEnabled;

                        struct NotificationConfig {
                            bool enabled;
                            int priority;
                            std::string description;
                        };

                        std::unordered_map<std::string, NotificationConfig> m_notificationMap;

                        void onSignal(
                                        const Glib::RefPtr<Gio::DBus::Connection>& connection,
                                        const Glib::ustring &sender_name,
                                        const Glib::ustring &object_path,
                                        const Glib::ustring &interface_name,
                                        const Glib::ustring &signal_name,
                                        const Glib::VariantContainerBase& parameters
                                );
                        bool readSettingsConfig();
                        bool writeSettingsConfig();
                        void makeNotificationMap();

                        int extractStatus(const Glib::VariantContainerBase &container);

                        std::vector<std::string> m_ekosStatusNotificationMap = {
                            "ekosStatusChangedToIdle", "ekosStatusChangedToPending",
                            "ekosStatusChangedToSuccess", "ekosStatusChangedToError"
                        };

                        std::vector<std::string> m_ekosSettleStatusNotificationMap = {
                            "ekosSettleStatusChangedToIdle", "ekosSettleStatusChangedToPending",
                            "ekosSettleStatusChangedToSuccess", "ekosSettleStatusChangedToError"
                        };

                        std::unordered_map<std::string, std::string> m_logInterfaceNotificationMap = {
                            {"org.kde.kstars.Ekos", "ekosNewLog"},
                            {"org.kde.kstars.Ekos.Align", "alignNewLog"},
                            {"org.kde.kstars.Ekos.Capture", "captureNewLog"},
                            {"org.kde.kstars.Ekos.Focus", "focusNewLog"},
                            {"org.kde.kstars.Ekos.Guide", "guideNewLog"},
                            {"org.kde.kstars.Ekos.Scheduler", "schedulerNewLog"}
                        };

                        std::vector<std::string> m_alignStatusNotificationMap = {
                            "alignStatusChangedToIdle", "alignStatusChangedToCompleted",
                            "alignStatusChangedToFailed", "alignStatusChangedToAborted",
                            "alignStatusChangedToProgress", "alignStatusChangedToSuccess",
                            "alignStatusChangedToSyncing", "alignStatusChangedToSlewing",
                            "alignStatusChangedToRotating", "alignStatusChangedToSuspended"
                        };

                        std::vector<std::string> m_frameNameMap = {
                            "Light", "Bias", "Dark", "Flat", "Video", "None"
                        };

                        std::vector<std::string> m_focusStatusNotificationMap = {
                            "focusStatusChangedToIdle", "focusStatusChangedToComplete",
                            "focusStatusChangedToFailed", "focusStatusChangedToAborted",
                            "focusStatusChangedToWaiting", "focusStatusChangedToProgress",
                            "focusStatusChangedToFraming", "focusStatusChangedToFilter"
                        };

                        std::vector<std::string> m_guideStatusNotificationMap = {
                            "guideStatusChangedToIdle", "guideStatusChangedToAborted",
                            "guideStatusChangedToConnected", "guideStatusChangedToDisconnected",
                            "guideStatusChangedToCapture", "guideStatusChangedToLooping",
                            "guideStatusChangedToDark", "guideStatusChangedToSubframe",
                            "guideStatusChangedToStarselect", "guideStatusChangedToCalibrate",
                            "guideStatusChangedToCalibrationError", "guideStatusChangedToCalibrationSuccess",
                            "guideStatusChangedToGuiding", "guideStatusChangedToSuspended",
                            "guideStatusChangedToReacquiring", "guideStatusChangedToDithering",
                            "guideStatusChangedToManualDithering", "guideStatusChangedToDitheringError",
                            "guideStatusChangedToDitheringSuccess", "guideStatusChangedToDitheringSettle"
                        };

                        std::vector<std::string> m_mountParkStatusNotificationMap = {
                            "mountParkStatusChangedToUnknown", "mountParkStatusChangedToParked",
                            "mountParkStatusChangedToParking", "mountParkStatusChangedToUnparking",
                            "mountParkStatusChangedToUnparked", "mountParkStatusChangedToError"
                        };

                        std::vector<std::string> m_mountStatusNotificationMap = {
                            "mountStatusChangedToIdle", "mountStatusChangedToMoving",
                            "mountStatusChangedToSlewing", "mountStatusChangedToTracking",
                            "mountStatusChangedToParking", "mountStatusChangedToParked",
                            "mountStatusChangedToError"
                        };

                        std::unordered_map<int,std::string> m_mountPierSideNotificationMap = {
                            {-1, "mountPierSidechangedToUnknown"},
                            {0, "mountPierSidechangedToWest"},
                            {1, "mountPierSidechangedToEast"}
                        };

                        std::vector<std::string> m_observatoryStatusNotificationMap = {
                            "observatoryStatusChangedToIdle", "observatoryStatusChangedToOK",
                            "observatoryStatusChangedToWarning", "observatoryStatusChangedToAlert"
                        };

                        std::vector<std::string> m_schedulerStatusNotificationMap = {
                            "schedulerStatusChangedToIdle", "schedulerStatusChangedToStartup",
                            "schedulerStatusChangedToRunning", "schedulerStatusChangedToPaused",
                            "schedulerStatusChangedToShutdown", "schedulerStatusChangedToAborted",
                            "schedulerStatusChangedToLoading"
                        };

                        struct SignalData {
                            const Glib::ustring sender;
                            const Glib::ustring object;
                            const Glib::ustring interface;
                            const Glib::ustring signal;
                            const Glib::VariantContainerBase parameters;
                        };
                        std::string getDeviceName(const SignalData &data);

                        // Actual processing of signals
                        void processSignal(const SignalData &data);
                        bool onEkosStatusChanged(const SignalData &data);
                        bool onEkosNewDevice(const SignalData &data);
                        bool onEkosSettleStatusChanged(const SignalData &data);
                        bool onNewLog(const SignalData &data);
                        bool onAlignNewSolution(const SignalData &data);
                        bool onAlignStatusChanged(const SignalData &data);
                        bool onCaptureComplete(const SignalData &data);
                        bool onCaptureMeridianFlipStarted(const SignalData &data);
                        bool onCaptureReady(const SignalData &data);
                        bool onFocusNewHFR(const SignalData &data);
                        bool onFocusStatusChanged(const SignalData &data);
                        bool onGuideStatusChanged(const SignalData &data);
                        bool onMountNewMeridianFlipSetup(const SignalData &data);
                        bool onMountParkStatusChanged(const SignalData &data);
                        bool onMountStatusChanged(const SignalData &data);
                        bool onMountPierSideChanged(const SignalData &data);
                        bool onMountReady(const SignalData &data);
                        bool onObservatoryStatusChanged(const SignalData &data);
                        bool onSchedulerStatusChanged(const SignalData &data);
                        bool onDeviceConnected(const SignalData &data);

                        std::vector<bool (FrmMain::*)(const SignalData &)> m_dispatchTable {
                            &FrmMain::onEkosStatusChanged,
                            &FrmMain::onEkosNewDevice,
                            &FrmMain::onEkosSettleStatusChanged,
                            &FrmMain::onNewLog,
                            &FrmMain::onAlignNewSolution,
                            &FrmMain::onAlignStatusChanged,
                            &FrmMain::onCaptureComplete,
                            &FrmMain::onCaptureMeridianFlipStarted,
                            &FrmMain::onCaptureReady,
                            &FrmMain::onFocusNewHFR,
                            &FrmMain::onFocusStatusChanged,
                            &FrmMain::onGuideStatusChanged,
                            &FrmMain::onMountNewMeridianFlipSetup,
                            &FrmMain::onMountParkStatusChanged,
                            &FrmMain::onMountStatusChanged,
                            &FrmMain::onMountPierSideChanged,
                            &FrmMain::onMountReady,
                            &FrmMain::onObservatoryStatusChanged,
                            &FrmMain::onSchedulerStatusChanged,
                            &FrmMain::onDeviceConnected
                        };
	};
}
