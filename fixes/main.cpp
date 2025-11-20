#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QAbstractListModel>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QProcess>
#include <QRegularExpression>
#include <QQuickWindow>
#include <LayerShellQt/window.h>
#include <QDebug>
#include <QFileSystemWatcher>
#include <QTimer>


// =============================
//        AppEntry struct
// =============================
struct AppEntry {
    QString name;
    QString icon;
    QString exec;
    bool empty() const { return name.isEmpty() && exec.isEmpty(); }
};

// =============================
//        AppModel class
// =============================
class AppModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles { NameRole = Qt::UserRole + 1, IconRole, ExecRole };

    explicit AppModel(QObject* parent = nullptr)
    : QAbstractListModel(parent)
    {
        loadApps();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return m_apps.size();
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_apps.size())
            return {};
        const auto &a = m_apps[index.row()];
        switch (role) {
            case NameRole: return a.name;
            case IconRole: return a.icon;
            case ExecRole: return a.exec;
        }
        return {};
    }

    QHash<int, QByteArray> roleNames() const override {
        return {
            { NameRole, "name" },
            { IconRole, "icon" },
            { ExecRole, "exec" }
        };
    }

    // --- Add a .desktop file
    Q_INVOKABLE void addDesktopFile(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Cannot open .desktop file:" << path;
            return;
        }

        QString name, icon, exec;
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith("Name=") && name.isEmpty())
                name = line.mid(5);
            else if (line.startsWith("Icon=") && icon.isEmpty())
                icon = resolveIcon(line.mid(5));
            else if (line.startsWith("Exec=") && exec.isEmpty())
                exec = line.mid(5);
        }
        file.close();

        if (name.isEmpty() && exec.isEmpty()) {
            qWarning() << "Invalid .desktop file:" << path;
            return;
        }

        beginInsertRows(QModelIndex(), m_apps.size(), m_apps.size());
        m_apps.append({ name, icon, exec });
        endInsertRows();
        emit countChanged(); // notify QML
        saveApps();
    }

    // --- Remove app by index
    Q_INVOKABLE void removeApp(int index) {
        qDebug() << "[removeApp] Requested to remove index:" << index;

        if (index < 0 || index >= m_apps.size()) {
            qWarning() << "[removeApp] Invalid index:" << index
            << "Valid range is 0 to" << m_apps.size() - 1;
            return;
        }

        qDebug() << "[removeApp] Removing app at index:" << index
        << "Name:" << m_apps.at(index).name;

        beginRemoveRows(QModelIndex(), index, index);
        m_apps.removeAt(index);
        endRemoveRows();

        qDebug() << "[removeApp] Removal complete. New count:" << m_apps.size();

        emit countChanged();
        saveApps();

        qDebug() << "[removeApp] saveApps() called and countChanged() emitted.";
    }


    // --- Move app (for rearranging)
    Q_INVOKABLE void moveApp(int from, int to) {
        if (from < 0 || to < 0 || from >= m_apps.size() || to >= m_apps.size() || from == to)
            return;
        beginMoveRows(QModelIndex(), from, from, QModelIndex(), (to > from) ? to + 1 : to);
        m_apps.move(from, to);
        endMoveRows();
        emit countChanged(); // optional, keeps QML consistent
        saveApps();
    }

    // --- Launch app
    Q_INVOKABLE void launchApp(int index) {
        if (index < 0 || index >= m_apps.size()) return;
        const auto &a = m_apps[index];
        if (a.exec.isEmpty()) return;

        QString exec = a.exec;
        exec.remove(QRegularExpression("%[fFuUdDnNick]"));
        QStringList parts = exec.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) return;

        QString program = parts.takeFirst();
        if (!QProcess::startDetached(program, parts))
            qWarning() << "Failed to launch app:" << exec;
    }
    Q_INVOKABLE void toggleWin10Menu() {
        QString processName = "Win10Menu";
        QString exec = QStandardPaths::findExecutable(processName);

        if (exec.isEmpty())
            exec = "/usr/bin/Win10Menu";

        // 1️⃣ Check if it's running
        QProcess checkProc;
        checkProc.start("pgrep", {"-x", processName});
        checkProc.waitForFinished();

        QString output = QString::fromUtf8(checkProc.readAllStandardOutput()).trimmed();

        if (!output.isEmpty()) {
            // 2️⃣ Found running instance — kill it
            qInfo() << processName << "is running. Killing it...";
            QProcess::execute("pkill", {"-x", processName});
        } else {
            // 3️⃣ Not running — start it
            qInfo() << "Launching" << processName;
            if (!QProcess::startDetached(exec)) {
                qWarning() << "Failed to launch" << processName;
            }
        }
    }

    Q_INVOKABLE void togglenmqt() {
        QString processName = "nmqt";
        QString exec = QStandardPaths::findExecutable(processName);

        if (exec.isEmpty())
            exec = "/usr/bin/nmqt";

        // 1️⃣ Check if it's running
        QProcess checkProc;
        checkProc.start("pgrep", {"-x", processName});
        checkProc.waitForFinished();

        QString output = QString::fromUtf8(checkProc.readAllStandardOutput()).trimmed();

        if (!output.isEmpty()) {
            // 2️⃣ Found running instance — kill it
            qInfo() << processName << "is running. Killing it...";
            QProcess::execute("pkill", {"-x", processName});
        } else {
            // 3️⃣ Not running — start it
            qInfo() << "Launching" << processName;
            if (!QProcess::startDetached(exec)) {
                qWarning() << "Failed to launch" << processName;
            }
        }
    }

    Q_INVOKABLE void toggleblueman() {
        QString processName = "blueman-manager";
        QString exec = QStandardPaths::findExecutable(processName);

        if (exec.isEmpty())
            exec = "/usr/bin/blueman-manager";

        // 1️⃣ Check if it's running
        QProcess checkProc;
        checkProc.start("pgrep", {"-x", processName});
        checkProc.waitForFinished();

        QString output = QString::fromUtf8(checkProc.readAllStandardOutput()).trimmed();

        if (!output.isEmpty()) {
            // 2️⃣ Found running instance — kill it
            qInfo() << processName << "is running. Killing it...";
            QProcess::execute("pkill", {"-x", processName});
        } else {
            // 3️⃣ Not running — start it
            qInfo() << "Launching" << processName;
            if (!QProcess::startDetached(exec)) {
                qWarning() << "Failed to launch" << processName;
            }
        }
    }



    // --- Resolve icon from system or file path
    Q_INVOKABLE QString resolveIcon(const QString &name) const {
        if (QFile::exists(name))
            return "file://" + name;

        const QStringList iconDirs = {
            "/usr/share/icons/hicolor/256x256/apps/",
            "/usr/share/icons/hicolor/128x128/apps/",
            "/usr/share/icons/hicolor/64x64/apps/",
            "/usr/share/icons/hicolor/48x48/apps/",
            "/usr/share/icons/hicolor/scalable/apps/",
            "/usr/share/pixmaps/",
            QDir::homePath() + "/.local/share/icons/hicolor/256x256/apps/"
        };

        for (const QString& dir : iconDirs) {
            if (QFile::exists(dir + name + ".png"))
                return "file://" + dir + name + ".png";
            if (QFile::exists(dir + name + ".svg"))
                return "file://" + dir + name + ".svg";
        }

        return "qrc:/icons/placeholder.svg";
    }

signals:
    void countChanged();

private:
    QVector<AppEntry> m_apps;

    void loadApps() {
        QString cfgPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + "/bottompanel/apps.json";
        QFile f(cfgPath);
        if (!f.exists() || !f.open(QIODevice::ReadOnly))
            return;

        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (!doc.isArray()) return;

        QJsonArray arr = doc.array();
        beginResetModel();
        m_apps.clear();
        for (auto v : arr) {
            QJsonObject o = v.toObject();
            AppEntry a;
            a.name = o["name"].toString();
            a.icon = o["icon"].toString();
            a.exec = o["exec"].toString();
            m_apps.append(a);
        }
        endResetModel();
        emit countChanged(); // notify QML of initial load
    }

    void saveApps() {
        QJsonArray arr;
        for (const auto &a : m_apps) {
            if (a.empty()) continue;
            QJsonObject o;
            o["name"] = a.name;
            o["icon"] = a.icon;
            o["exec"] = a.exec;
            arr.append(o);
        }

        QJsonDocument doc(arr);
        QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + "/bottompanel/";
            QDir().mkpath(cfgDir);
            QFile f(cfgDir + "apps.json");
            if (f.open(QIODevice::WriteOnly)) {
                f.write(doc.toJson());
                f.close();
            }
    }
};

#include <QFileSystemWatcher>
#include <QTimer>
#include <QSettings>
#include <QProcess>

class RunningWindowModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        AppIdRole,
        FocusedRole,
        IconRole
    };

    struct WindowEntry {
        QString id;       // <-- now using ID (group name), not title
        QString title;
        QString app_id;
        bool focused;
        QString icon;
    };

    RunningWindowModel(QObject* parent = nullptr)
    : QAbstractListModel(parent)
    {
        refresh();
        QTimer::singleShot(300, this, &RunningWindowModel::refresh);

        auto timer = new QTimer(this);
        timer->setInterval(2000);
        connect(timer, &QTimer::timeout, this, &RunningWindowModel::refresh);
        timer->start();
    }

    Q_INVOKABLE void refresh()
    {
        const QString iniPath =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + "/hexlauncher/windows.ini";
            loadFromIni(iniPath);
    }

    Q_INVOKABLE void activate(int index)
    {
        if (index < 0 || index >= windows.size())
            return;

        QString wid = windows[index].id;  // <-- use unique ID

        QString program = QStandardPaths::findExecutable("list-windows");
        if (program.isEmpty()) {
            qWarning() << "list-windows not found!";
            return;
        }

        QProcess::startDetached(program, { "--activate", wid });
        QTimer::singleShot(120, this, &RunningWindowModel::refresh);
    }

    Q_INVOKABLE void close(int index)
    {
        if (index < 0 || index >= windows.size())
            return;

        QString wid = windows[index].id;

        QString program = QStandardPaths::findExecutable("list-windows");
        if (program.isEmpty()) {
            qWarning() << "list-windows not found!";
            return;
        }

        if (QProcess::startDetached(program, { "--close", wid })) {
            QTimer::singleShot(120, this, &RunningWindowModel::refresh);
        }
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        Q_UNUSED(parent);
        return windows.size();
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() >= windows.size())
            return {};

        const auto& w = windows[index.row()];

        switch (role) {
            case IdRole:     return w.id;
            case TitleRole:  return w.title;
            case AppIdRole:  return w.app_id;
            case FocusedRole:return w.focused;
            case IconRole:   return w.icon;
        }
        return {};
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {
            { IdRole, "wid" },
            { TitleRole, "title" },
            { AppIdRole, "app_id" },
            { FocusedRole, "focused" },
            { IconRole, "icon" }
        };
    }

private:
    QList<WindowEntry> windows;

    void loadFromIni(const QString& path)
    {
        if (!QFile::exists(path))
            return;

        QList<WindowEntry> newList;

        QSettings ini(path, QSettings::IniFormat);

        for (const QString& group : ini.childGroups()) {
            ini.beginGroup(group);

            WindowEntry w;
            w.id      = group;  // <-- IMPORTANT
            w.title   = ini.value("Title").toString();
            w.app_id  = ini.value("AppID").toString();
            w.focused = ini.value("Focused").toBool();

            QString iconName = readDesktopIcon(w.app_id);
            w.icon = resolveIcon(iconName);

            newList.append(w);
            ini.endGroup();
        }

        // Fast update if same count
        if (newList.size() == windows.size()) {
            windows = newList;
            if (!windows.isEmpty())
                emit dataChanged(index(0), index(windows.size()-1));
            return;
        }

        // Full reset only when needed
        beginResetModel();
        windows = newList;
        endResetModel();
    }

    // --- Safe desktop file icon reader ---
    QString readDesktopIcon(const QString& appId) const
    {
        QStringList names = {
            appId + ".desktop",
            appId.toLower() + ".desktop"
        };

        QStringList dirs = {
            QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation),
            "/usr/share/applications",
            "/usr/local/share/applications"
        };

        for (const QString& dir : dirs) {
            for (const QString& file : names) {
                QString path = QDir(dir).filePath(file);
                if (!QFile::exists(path))
                    continue;

                QFile f(path);
                if (!f.open(QFile::ReadOnly))
                    continue;

                QTextStream ts(&f);
                while (!ts.atEnd()) {
                    QString line = ts.readLine().trimmed();
                    if (line.startsWith("Icon="))
                        return line.mid(5);
                }
            }
        }
        return "";
    }

    // --- Theme icon resolver ---
    QString resolveIcon(const QString& name) const
    {
        if (name.isEmpty())
            return ":/icons/default.png";

        // Direct file path?
        if (QFile::exists(name))
            return name;

        // Try themed icon
        QIcon ico = QIcon::fromTheme(name);
        if (!ico.isNull()) {
            QPixmap pm = ico.pixmap(64, 64);
            if (!pm.isNull()) {
                QString out = QStandardPaths::writableLocation(
                    QStandardPaths::CacheLocation)
                + "/" + name + "_64.png";

                QDir().mkpath(QFileInfo(out).path());
                pm.save(out);
                return out;
            }
        }

        return ":/icons/default.png";
    }
};



class OsdControl : public QObject {
    Q_OBJECT
public:
    explicit OsdControl(QObject* parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE void volUp()   { QProcess::startDetached("osd-client", {"--volup"}); }
    Q_INVOKABLE void volDown() { QProcess::startDetached("osd-client", {"--voldown"}); }
    Q_INVOKABLE void volMute() { QProcess::startDetached("osd-client", {"--mute"}); }

    Q_INVOKABLE void dispUp()   { QProcess::startDetached("osd-client", {"--dispup"}); }
    Q_INVOKABLE void dispDown() { QProcess::startDetached("osd-client", {"--dispdown"}); }
};

// =============================
//        main()
// =============================
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    AppModel model;
    engine.rootContext()->setContextProperty("appModel", &model);

    RunningWindowModel windowModel;
    engine.rootContext()->setContextProperty("windowModel", &windowModel);

    OsdControl osdControl;
    engine.rootContext()->setContextProperty("osdController", &osdControl);


    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    QQuickWindow* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window)
        return -1;

    auto layerWindow = LayerShellQt::Window::get(window);
    layerWindow->setLayer(LayerShellQt::Window::LayerTop);
    layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
    layerWindow->setAnchors({
        LayerShellQt::Window::AnchorBottom,
        LayerShellQt::Window::AnchorLeft,
        LayerShellQt::Window::AnchorRight
    });
    layerWindow->setExclusiveZone(window->height());
    layerWindow->setMargins({0, 0, 0, 0});

    window->setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    window->show();

    return app.exec();
}

#include "main.moc"
