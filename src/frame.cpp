#include "frame.h"

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
        builder->get_widget("buttonNotificationSettings", m_buttonSettings);

        m_buttonTest->signal_clicked().connect([&]() {
                std::thread([&]() {
                        test();
                        }).detach();
                });

        m_buttonSettings->signal_clicked().connect([&]() {
                auto win = std::make_unique<NotificationSettingsWindow>(
                        m_notificationDescriptions,
                        m_notificationPriorities,
                        m_notificationEnabled
                        );
                m_settingsWindow = std::move(win);
                m_settingsWindow->set_title("Notification settings");
                m_settingsWindow->set_default_geometry(500,700);
                m_settingsWindow->signal_value_chagned().connect([&]() {
                        writeSettingsConfig();
                        makeNotificationMap();
                        });
                m_settingsWindow->show();
                });



        m_buttonSave->signal_clicked().connect(sigc::mem_fun(*this, &FrmMain::saveConfig));

        signal_realize().connect([this](){
                std::thread([this]() {
                        initConfig();
                        }).detach();
                });

        m_dbus->signal_subscribe(sigc::mem_fun(*this, &FrmMain::onSignal), "org.kde.kstars",  "", "", "", "");
}

FrmMain::~FrmMain() {
}

int FrmMain::extractStatus(const Glib::VariantContainerBase &container) {
    Glib::VariantBase inside;
    container.get_child(inside, 0);
    Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(inside).get_child(inside, 0);
    return Glib::VariantBase::cast_dynamic<Glib::Variant<int>>(inside).get();
}

void FrmMain::onSignal(
  const Glib::RefPtr<Gio::DBus::Connection>& connection,
  const Glib::ustring &sender_name,
  const Glib::ustring &object_path,
  const Glib::ustring &interface_name,
  const Glib::ustring &signal_name,
  const Glib::VariantContainerBase& parameters
) {
    std::ignore = connection;

    SignalData data = {sender_name, object_path, interface_name, signal_name, parameters};
    std::thread([&, data]() {
            processSignal(data);
            }).detach();
}

void FrmMain::processSignal(const SignalData &data) {
    std::lock_guard<std::mutex> lock(m_signalMutex);
    std::cout << "Got signal" << std::endl;
    std::cout << "From: " << data.sender << std::endl;
    std::cout << "Object: " << data.object << std::endl;
    std::cout << "Interface: " << data.interface << std::endl;
    std::cout << "Signal: " << data.signal << std::endl;
    std::cout << data.parameters.get_type_string() << " " << data.parameters.print() << std::endl << std::endl;

    for ( auto cb : m_dispatchTable ) {
        if ( (this->*cb)(data) ) {
            return;
        }
    }
}

std::string FrmMain::getDeviceName(const SignalData &data) {
    Glib::VariantContainerBase args = Glib::VariantContainerBase::create_tuple({
            Glib::Variant<Glib::ustring>::create(data.interface),
            Glib::Variant<Glib::ustring>::create("name")
            });
    auto proxy = Gio::DBus::Proxy::create_sync(m_dbus, "org.kde.kstars",
            data.object, "org.freedesktop.DBus.Properties");
    auto result = proxy->call_sync("Get", args);
    auto tuple = Glib::VariantContainerBase::cast_dynamic<Glib::VariantContainerBase>(result);
    Glib::VariantBase v_layer = tuple.get_child(0);
    auto v_container = Glib::VariantContainerBase::cast_dynamic<Glib::VariantContainerBase>(v_layer);
    Glib::VariantBase s_layer = v_container.get_child();
    auto string_variant = Glib::Variant<Glib::ustring>::cast_dynamic<Glib::Variant<Glib::ustring>>(s_layer);
    std::string device = string_variant.get();
    return device;
}

bool FrmMain::onEkosStatusChanged(const SignalData &data) {
    if ( data.signal != "ekosStatusChanged" || data.object != "/KStars/Ekos" || data.interface != "org.kde.kstars.Ekos" ) {
        return false;
    }
    int status = extractStatus(data.parameters);
    std::string nf = m_ekosStatusNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onEkosNewDevice(const SignalData &data) {
    if ( data.signal != "newDevice" || data.object != "/KStars/Ekos" || data.interface != "org.kde.kstars.Ekos" ) {
        return false;
    }
    std::string nf = "ekosNewDevice";
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    Glib::VariantBase inside;
    data.parameters.get_child(inside, 0);
    std::string device = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(inside).get();
    std::string msg = "New device connected to ekos: ";
    msg += device;
    push(msg, msg, m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onEkosSettleStatusChanged(const SignalData &data) {
    if ( data.signal != "settleStatusChanged" || data.object != "/KStars/Ekos" || data.interface != "org.kde.kstars.Ekos" ) {
        return false;
    }
    int status = extractStatus(data.parameters);
    std::string nf = m_ekosSettleStatusNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onNewLog(const SignalData &data) {
    if ( data.signal != "newLog" ) {
        return false;
    }
    if ( ! m_logInterfaceNotificationMap.count(data.interface) ) {
        // Correct handler, but no notification set up
        return true;
    }
    std::string nf = m_logInterfaceNotificationMap[data.interface];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    Glib::VariantBase inside;
    data.parameters.get_child(inside, 0);
    std::string log = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(inside).get();
    push(m_notificationMap[nf].description, log, m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onAlignNewSolution(const SignalData &data) {
    if ( data.signal != "newSolution" || data.object != "/KStars/Ekos/Align" || data.interface != "org.kde.kstars.Ekos.Align" ) {
        return false;
    }
    std::string nf = "alignNewSolution";
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    Glib::VariantBase inside;
    data.parameters.get_child(inside, 0);
    auto content = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(inside);
    std::string msg = "";
    for (gsize ii=0; ii<content.get_n_children(); ii++) {
        auto thing = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(content.get_child(ii));
        Glib::VariantBase child;
        thing.get_child(child);
        auto name = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(child).get();
        auto value = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(thing.get_child(1)).get_child(0);
        msg += name + ": " + value.print() + "\n\n"; // Double newline required for message formatting
    }
    push(m_notificationMap[nf].description, msg, m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onAlignStatusChanged(const SignalData &data) {
    if ( data.signal != "newStatus" || data.object != "/KStars/Ekos/Align" || data.interface != "org.kde.kstars.Ekos.Align" ) {
        return false;
    }
    int status = extractStatus(data.parameters);
    std::string nf = m_alignStatusNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onCaptureComplete(const SignalData &data) {
    if ( data.signal != "captureComplete" || data.object != "/KStars/Ekos/Capture" || data.interface != "org.kde.kstars.Ekos.Capture" ) {
        return false;
    }
    std::string nf = "captureCaptureComplete";
    if ( ! m_notificationMap[nf].enabled ) {
        return false;
    }
    Glib::VariantBase inside;
    data.parameters.get_child(inside, 0);
    auto content = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(inside);
    std::unordered_map<std::string, std::string> stuff;
    for (gsize ii=0; ii<content.get_n_children(); ii++) {
        auto thing = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(content.get_child(ii));
        Glib::VariantBase child;
        thing.get_child(child);
        auto name = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(child).get();
        auto value = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(thing.get_child(1)).get_child(0);
        if ( name == "filename" ) {
            auto fileName = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(value).get();
            if ( fileName == "/tmp/image.fits" || fileName == "" || fileName == " " ) {
                // Preview
                return true;
            }
        }
        if ( name == "type" ) {
            stuff[name] = m_frameNameMap[Glib::VariantBase::cast_dynamic<Glib::Variant<int>>(value).get()];
            continue;
        }
        if ( name == "hfr" || name == "eccentricity" ) {
            char buff[32];
            snprintf(buff, sizeof(buff)-1, "%.2f", Glib::VariantBase::cast_dynamic<Glib::Variant<double>>(value).get());
            stuff[name] = std::string(buff);
            continue;
        }
        stuff[name] = value.print();
    }

    std::string msg = "";
    msg += "Type: " + stuff["type"] + "\n\n";
    msg += "Filter: " + stuff["filter"] + "\n\n";
    msg += "Exposure: " + stuff["exposure"] + "s\n\n";
    msg += "HFR: " + stuff["hfr"] + "\n\n";
    msg += "Median: " + stuff["median"] + "\n\n";
    msg += "Star count: " + stuff["starCount"] + "\n\n";
    msg += "Eccentricity: " + stuff["eccentricity"] + "\n\n";
    msg += "Binning: " + stuff["binx"] + "x" + stuff["biny"] + "\n\n";
    msg += "Dimensions: " + stuff["width"] + "x" + stuff["height"] + "\n\n";
    msg += "File: " + stuff["filename"] + "\n\n";
    push(m_notificationMap[nf].description, msg, m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onCaptureMeridianFlipStarted(const SignalData &data) {
    if ( data.signal != "meridianFlipStarted" || data.object != "/KStars/Ekos/Capture" || data.interface != "org.kde.kstars.Ekos.Capture" ) {
        return false;
    }
    std::string nf = "captureMeridianFlipStarted";
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description, m_notificationMap[nf].description, m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onCaptureReady(const SignalData &data) {
    if ( data.signal != "ready" || data.object != "/KStars/Ekos/Capture" || data.interface != "org.kde.kstars.Ekos.Capture" ) {
        return false;
    }
    std::string nf = "captureReady";
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description, m_notificationMap[nf].description, m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onFocusNewHFR(const SignalData &data) {
    if ( data.signal != "newHFR" || data.object != "/KStars/Ekos/Focus" || data.interface != "org.kde.kstars.Ekos.Focus" ) {
        return false;
    }
    std::string nf = "focusNewHFR";
    if ( ! m_notificationMap[nf].enabled) {
        return true;
    }

    Glib::VariantBase inside;
    data.parameters.get_child(inside, 0);
    double hfr = Glib::VariantBase::cast_dynamic<Glib::Variant<double>>(inside).get();
    data.parameters.get_child(inside, 1);
    int position = Glib::VariantBase::cast_dynamic<Glib::Variant<int>>(inside).get();
    data.parameters.get_child(inside, 2);
    bool autofocus = Glib::VariantBase::cast_dynamic<Glib::Variant<bool>>(inside).get();
    data.parameters.get_child(inside, 3);
    std::string train = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(inside).get();

    char buff[128];
    std::string msg = "";
    snprintf(buff, sizeof(buff)-1, "HFR: %.2f\n\n", hfr);
    msg += buff;
    snprintf(buff, sizeof(buff)-1, "Position: %d\n\n", position);
    msg += buff;
    msg += std::string("Autofocus: ") + (autofocus ? "enabled" : "disabled") + "\n\n";
    msg += "Train: " + train + "\n\n";
    push(m_notificationMap[nf].description, msg, m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onFocusStatusChanged(const SignalData &data) {
    if ( data.signal != "newStatus" || data.object != "/KStars/Ekos/Focus" || data.interface != "org.kde.kstars.Ekos.Focus" ) {
        return false;
    }
    Glib::VariantBase inside;
    data.parameters.get_child(inside, 0);
    Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(inside).get_child(inside, 0);
    int status= Glib::VariantBase::cast_dynamic<Glib::Variant<int>>(inside).get();

    std::string nf = m_focusStatusNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }

    data.parameters.get_child(inside, 1);
    std::string train = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(inside).get();

    std::string title = m_notificationMap[nf].description;
    std::string message = "Train: " + train;
    push(title, message, m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onGuideStatusChanged(const SignalData &data) {
    if ( data.signal != "newStatus" || data.object != "/KStars/Ekos/Guide" || data.interface != "org.kde.kstars.Ekos.Guide" ) {
        return false;
    }
    int status = extractStatus(data.parameters);
    std::string nf = m_guideStatusNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onMountNewMeridianFlipSetup(const SignalData &data) {
    if ( data.signal != "newMeridianFlipSetup" || data.object != "/KStars/Ekos/Mount" || data.interface != "org.kde.kstars.Ekos.Mount" ) {
        return false;
    }
    std::string nf = "mountNewMeridianFlipSetup";
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onMountParkStatusChanged(const SignalData &data) {
    if ( data.signal != "newParkStatus" || data.object != "/KStars/Ekos/Mount" || data.interface != "org.kde.kstars.Ekos.Mount" ) {
        return false;
    }
    int status = extractStatus(data.parameters);
    std::string nf = m_mountParkStatusNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onMountStatusChanged(const SignalData &data) {
    if ( data.signal != "newStatus" || data.object != "/KStars/Ekos/Mount" || data.interface != "org.kde.kstars.Ekos.Mount" ) {
        return false;
    }
    int status = extractStatus(data.parameters);
    std::string nf = m_mountStatusNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onMountPierSideChanged(const SignalData &data ){
    if ( data.signal != "pierSideChanged" || data.object != "/KStars/Ekos/Mount" || data.interface != "org.kde.kstars.Ekos.Mount" ) {
        return false;
    }
    int status = extractStatus(data.parameters);
    std::string nf = m_mountPierSideNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onMountReady(const SignalData &data) {
    if ( data.signal != "ready" || data.object != "/KStars/Ekos/Mount" || data.interface != "org.kde.kstars.Ekos.Mount" ) {
        return false;
    }
    std::string nf = "mountReady";
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onObservatoryStatusChanged(const SignalData &data) {
    if ( data.signal != "newStatus" || data.object != "/KStars/Ekos/Observatory" || data.interface != "org.kde.kstars.Ekos.Observatory" ) {
        return false;
    }
    int status = extractStatus(data.parameters);
    std::string nf = m_observatoryStatusNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onSchedulerStatusChanged(const SignalData &data) {
    if ( data.signal != "newStatus" || data.object != "/KStars/Ekos/Scheduler" || data.interface != "org.kde.kstars.Ekos.Scheduler" ) {
        return false;
    }
    int status = extractStatus(data.parameters);
    std::string nf = m_schedulerStatusNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onDeviceConnected(const SignalData &data) {
    if ( data.signal != "Connected" || data.object.rfind("/KStars/INDI/", 0) != 0 || data.interface.rfind("org.kde.kstars.INDI.", 0) != 0) {
        return false;
    }
    std::string nf = "deviceConnected";
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    std::string device = getDeviceName(data);
    std::string msg = "Device connected: " + device;
    push(m_notificationMap[nf].description, msg, m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onDeviceDisconnected(const SignalData &data) {
    if ( data.signal != "Disconnected" || data.object.rfind("/KStars/INDI/", 0) != 0 || data.interface.rfind("org.kde.kstars.INDI.", 0) != 0) {
        return false;
    }
    std::string nf = "deviceDisconnected";
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    std::string device = getDeviceName(data);
    std::string msg = "Device disconnected: " + device;
    push(m_notificationMap[nf].description, msg, m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onDomeParkStatusChanged(const SignalData &data) {
    if ( data.signal != "newParkStatus" || data.object.rfind("/KStars/INDI/Dome/", 0) != 0 || data.interface != "org.kde.kstars.INDI.Dome" ) {
        return false;
    }
    int status = extractStatus(data.parameters);
    std::string nf = m_domeParkStatusNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onDomeShutterStatusChanged(const SignalData &data) {
    if ( data.signal != "newShutterStatus" || data.object.rfind("/KStars/INDI/Dome/", 0) != 0 || data.interface != "org.kde.kstars.INDI.Dome" ) {
        return false;
    }
    int status = extractStatus(data.parameters);
    std::string nf = m_domeShutterStatusNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onDomeStatusChanged(const SignalData &data) {
    if ( data.signal != "newStatus" || data.object.rfind("/KStars/INDI/Dome/", 0) != 0 || data.interface != "org.kde.kstars.INDI.Dome" ) {
        return false;
    }
    int status = extractStatus(data.parameters);
    std::string nf = m_domeStatusNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onDomePositionChanged(const SignalData &data) {
    if ( data.signal != "positionChanged" || data.object.rfind("/KStars/INDI/Dome/", 0) != 0 || data.interface != "org.kde.kstars.INDI.Dome" ) {
        return false;
    }
    std::string nf = "domePositionChanged";
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    Glib::VariantBase inside;
    data.parameters.get_child(inside, 0);
    double position = Glib::VariantBase::cast_dynamic<Glib::Variant<double>>(inside).get();
    char buff[256];
    snprintf(buff, sizeof(buff)-1, "Dome position changed to %.1f", position);
    std::string msg = buff;
    push(m_notificationMap[nf].description, msg, m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onDomeReady(const SignalData &data) {
    if ( data.signal != "ready" || data.object.rfind("/KStars/INDI/Dome/", 0) != 0 || data.interface != "org.kde.kstars.INDI.Dome" ) {
        return false;
    }
    std::string nf = "domeReady";
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description, m_notificationMap[nf].description, m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onDustcapParkStatusChanged(const SignalData &data) {
    if ( data.signal != "newParkStatus" || data.object.rfind("/KStars/INDI/DustCap/", 0) != 0 || data.interface != "org.kde.kstars.INDI.DustCap" ) {
        return false;
    }
    int status = extractStatus(data.parameters);
    std::string nf = m_dustcapParkStatusNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onDustcapStatusChanged(const SignalData &data) {
    if ( data.signal != "newStatus" || data.object.rfind("/KStars/INDI/DustCap/", 0) != 0 || data.interface != "org.kde.kstars.INDI.DustCap" ) {
        return false;
    }
    int status = extractStatus(data.parameters);
    std::string nf = m_dustcapStatusNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onDustcapReady(const SignalData &data) {
    if ( data.signal != "ready" || data.object.rfind("/KStars/INDI/DustCap/", 0) != 0 || data.interface != "org.kde.kstars.INDI.DustCap" ) {
        return false;
    }
    std::string nf = "dustcapReady";
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onWeatherStatusChanged(const SignalData &data) {
    if ( data.signal != "newStatus" || data.object.rfind("/KStars/INDI/Weather/", 0) != 0 || data.interface != "org.kde.kstars.INDI.Weather" ) {
        return false;
    }
    int status = extractStatus(data.parameters);
    std::string nf = m_weatherStatusNotificationMap[status];
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
    return true;
}

bool FrmMain::onWeatherReady(const SignalData &data) {
    if ( data.signal != "ready" || data.object.rfind("/KStars/INDI/Weather/", 0) != 0 || data.interface != "org.kde.kstars.INDI.Weather" ) {
        return false;
    }
    std::string nf = "weatherReady";
    if ( ! m_notificationMap[nf].enabled ) {
        return true;
    }
    push(m_notificationMap[nf].description,
         m_notificationMap[nf].description,
         m_notificationMap[nf].priority);
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

void FrmMain::makeNotificationMap() {
    m_notificationMap.clear();
    for (size_t ii=0; ii<m_notificationIds.size(); ii++) {
        std::string key = std::string(m_notificationIds[ii]);
        m_notificationMap[key] = NotificationConfig{
            m_notificationEnabled[ii],
            m_notificationPriorities[ii],
            m_notificationDescriptions[ii]
        };
    }
}

void FrmMain::initConfig() {
        std::string url, token, more;
        const gchar *args[3];
        args[0] = g_get_user_config_dir();
        args[1] = "ekosnotify_credentials.conf";
        args[2] = NULL;
        m_credentialsFile = std::string(g_build_filenamev((gchar **) args));
        std::ifstream stream(m_credentialsFile);
        if ( stream.is_open() ) {
            std::getline(stream, url);
            std::getline(stream, token);
            std::getline(stream, more);
            if ( url == "" || token == "" || ! stream.eof()) {
                    log("Corrupted gotify configuration detected\n");
                    char buff[512];
                    snprintf(buff, sizeof(buff),
                                    "Your gotify configuration file (%s) is corrupted and will be deleted. "
                                    "You will have to re-enter your url and token.",
                                    m_credentialsFile.c_str());
                    showError("Corrupted Gotify Configuration", buff);
                    stream.close();
                    std::remove(m_credentialsFile.c_str());
                    return;
            }
            m_entryURL->set_text(url);
            m_entryToken->set_text(token);
            log("Found existing gotify configuration\n");
        } else {
            log("No exiting gotify configuration available\n");
        }

        auto defaultsJson = nlohmann::json::parse(__defaultSettingsData);
        for (const auto& change : defaultsJson) {
            m_defaultNotificationIds.push_back(Glib::ustring(change[0].template get<std::string>()));
            m_defaultNotificationDescriptions.push_back(Glib::ustring(change[1].template get<std::string>()));
            m_defaultNotificationPriorities.push_back(change[2].template get<int>());
            m_defaultNotificationEnabled.push_back(change[3].template get<bool>());
        }

        args[1] = "ekosnotify_notifications.conf";
        m_settingsFile = std::string(g_build_filenamev((gchar **) args));
        log("Reading notification settings\n");
        if ( readSettingsConfig() ) {
            bool modified = false;

            // Include possibly new notifications
            for (size_t ii=0; ii<m_defaultNotificationIds.size(); ii++) {
                if ( std::count(m_notificationIds.begin(), m_notificationIds.end(), m_defaultNotificationIds[ii]) ) {
                    continue;
                }
                log(std::string("Adding new notification " + m_defaultNotificationIds[ii] + "\n"));
                m_notificationIds.push_back(m_defaultNotificationIds[ii]);
                m_notificationDescriptions.push_back(m_defaultNotificationDescriptions[ii]);
                m_notificationPriorities.push_back(m_defaultNotificationPriorities[ii]);
                m_notificationEnabled.push_back(m_defaultNotificationEnabled[ii]);
                modified = true;
            }

            // Remove possibly obsolete notifications
            std::vector<size_t> listOK;
            for (size_t ii=0; ii<m_notificationIds.size(); ii++) {
                if ( std::count(m_defaultNotificationIds.begin(), m_defaultNotificationIds.end(), m_notificationIds[ii]) ) {
                    listOK.push_back(ii);
                } else {
                    log(std::string("Removing obsolete notification " + m_notificationIds[ii] + "\n"));
                }
            }
            if ( listOK.size() != m_notificationIds.size() ) {
                std::vector<Glib::ustring> notificationIds;
                std::vector<Glib::ustring> notificationDescriptions;
                std::vector<int> notificationPriorities;
                std::vector<bool> notificationEnabled;
                for (size_t ii=0; ii<listOK.size(); ii++) {
                    notificationIds.push_back(m_notificationIds[ii]);
                    notificationDescriptions.push_back(m_notificationDescriptions[ii]);
                    notificationPriorities.push_back(m_notificationPriorities[ii]);
                    notificationEnabled.push_back(m_notificationEnabled[ii]);
                }
                m_notificationIds = notificationIds;
                m_notificationDescriptions = notificationDescriptions;
                m_notificationPriorities = notificationPriorities;
                m_notificationEnabled = notificationEnabled;
                modified = true;
            }

            // Update description
            for (size_t ii=0; ii<m_notificationIds.size(); ii++) {
                for (size_t jj=0; jj<m_defaultNotificationIds.size(); jj++) {
                    if ( m_notificationIds[ii] == m_defaultNotificationIds[jj] &&
                            m_notificationDescriptions[ii] != m_defaultNotificationDescriptions[jj] ) {
                        log(std::string("Updating description for " + m_notificationIds[ii]) + "\n");
                        m_notificationDescriptions[ii] = m_defaultNotificationDescriptions[jj];
                        modified = true;
                    }
                }
            }

            if ( modified ) {
                writeSettingsConfig();
            }
        } else {
            log("Using default notification settings\n");
            m_notificationIds = m_defaultNotificationIds;
            m_notificationDescriptions = m_defaultNotificationDescriptions;
            m_notificationPriorities = m_defaultNotificationPriorities;
            m_notificationEnabled = m_defaultNotificationEnabled;
            writeSettingsConfig();
        }
        makeNotificationMap();
}

bool FrmMain::readSettingsConfig() {
    m_notificationIds.clear();
    m_notificationDescriptions.clear();
    m_notificationPriorities.clear();
    m_notificationEnabled.clear();
    std::ifstream stream(m_settingsFile);
    if ( ! stream.is_open() ) {
        std::string msg = "Notification settings file " + m_settingsFile + " not found\n";
        log(msg);
        return false;
    }
    try {
        nlohmann::json settings;
        stream >> settings;
        if ( ! settings.is_array() ) {
            throw std::runtime_error("No array in json file");
        }
        for ( const auto& setting : settings ) {
            if ( ! setting.is_array() ||
                    setting.size() != 4 ||
                    ! setting[0].is_string() ||
                    ! setting[1].is_string() ||
                    ! setting[2].is_number_integer() ||
                    ! setting[3].is_boolean()
                    ) {
                std::string msg = "Found malformed part in notification settings file: \"" + setting.dump() + "\"\nignoring\n";
                log(msg);
                continue;
            }
            m_notificationIds.push_back(setting[0].template get<std::string>());
            m_notificationDescriptions.push_back(setting[1].template get<std::string>());
            m_notificationPriorities.push_back(setting[2].template get<int>());
            m_notificationEnabled.push_back(setting[3].template get<bool>());
        }
    } catch (const std::exception& e) {
        std::string msg = "Malformed notification settings file " + m_settingsFile + " found, have to remove it\n";
        log(msg);
        stream.close();
        std::remove(m_settingsFile.c_str());
        m_notificationIds.clear();
        m_notificationDescriptions.clear();
        m_notificationPriorities.clear();
        m_notificationEnabled.clear();
        return false;
    }
    return true;
}

bool FrmMain::writeSettingsConfig() {
    std::lock_guard<std::mutex> lock(m_settingsWriteMutex);
    std::ofstream out(m_settingsFile);
    if ( ! out.is_open() ) {
        log(std::string("Could not write to settings file ") + m_settingsFile + "\n");
        return false;
    }
    nlohmann::json j;
    for (size_t ii=0; ii<m_notificationIds.size(); ii++) {
        j.push_back(
                {m_notificationIds[ii], m_notificationDescriptions[ii], m_notificationPriorities[ii], m_notificationEnabled[ii]}
                );
    }
    out << j.dump(4) << std::endl;
    if ( ! out ) {
        log(std::string("Could not write to settings file ") + m_settingsFile + "\n");
        return false;
    }
    out.close();
    return true;
}

void FrmMain::saveConfig() {
        if ( m_entryURL->get_text() == "" || m_entryToken->get_text() == "" ) {
                showError("Configuration Error",
                                "You need to enter the username and API key in order to save it");
                return;
        }
        log("Saving configuration\n");
        std::ofstream stream(m_credentialsFile);
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
        push(title, msg, 10);
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

    log(std::string("Notification: ") + title + "\n");

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

NotificationSettingsWindow::NotificationSettingsWindow(std::vector<Glib::ustring> &labels,
        std::vector<int> &levels,
        std::vector<bool> &enabled) {
    assert(labels.size() == levels.size() && levels.size() == enabled.size());
    set_title("Notification settings");
    set_default_size(400, 300);
    add(m_scrolledWindow);
    m_scrolledWindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    m_scrolledWindow.add(m_grid);
    m_grid.set_row_spacing(5);
    m_grid.set_column_spacing(10);

    for (size_t ii=0; ii<labels.size(); ii++) {
        Gtk::Label* label = Gtk::make_managed<Gtk::Label>(labels[ii]);
        label->set_xalign(0.0);
        label->set_hexpand(true);
        m_grid.attach(*label, 0, ii, 1, 1);
        auto adjustment = Gtk::Adjustment::create(1, 1, 10, 1);
        Gtk::SpinButton* spin = Gtk::make_managed<Gtk::SpinButton>(adjustment, 1.0, 0);
        spin->set_value(levels[ii]);
        spin->set_hexpand(true);
        spin->signal_value_changed().connect([this, spin, ii, &levels]() {
                levels[ii] = spin->get_value();
                m_signal_value_changed.emit();
                });
        m_grid.attach(*spin, 1, ii, 1, 1);
        Gtk::Switch* sw = Gtk::make_managed<Gtk::Switch>();
        sw->set_active(enabled[ii]);
        sw->set_hexpand(true);
        sw->property_state().signal_changed().connect([this, sw, ii, &enabled](){
                enabled[ii] = sw->get_active();
                m_signal_value_changed.emit();
                });
        m_grid.attach(*sw, 2, ii, 1, 1);
    }
    show_all_children();
}

bool NotificationSettingsWindow::onSwitchChange(bool state) {
                if ( state ) {
                    std::cout << "Turned on" << std::endl;
                } else {
                    std::cout << "Turned off" << std::endl;
                }
                return false;
}

}
