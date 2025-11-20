#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QObject>
#include <QList>
#include <QString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QAbstractListModel>
#include <QMimeData>
#include <QDrag>
#include <QPixmap>
#include <QQuickItem>
#include <QQuickWindow>
#include <LayerShellQt/window.h>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <algorithm>

// ----------------------------
// AppInfo struct
// ----------------------------
struct AppInfo {
    QString name;
    QString command;
    QString icon;
    QString desktopFilePath;
};

// ----------------------------
// AppModel class
// ----------------------------
class AppModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        CommandRole,
        IconRole,
        LetterRole,
        HeaderVisibleRole,
        DesktopFileRole
    };

    AppModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    void setApps(const QList<AppInfo>& apps) {
        beginResetModel();
        m_apps = apps;
        endResetModel();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return m_apps.count();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if(!index.isValid() || index.row() < 0 || index.row() >= m_apps.count())
            return QVariant();

        const AppInfo &app = m_apps[index.row()];
        switch(role) {
            case NameRole: return app.name;
            case CommandRole: return app.command;
            case IconRole: return app.icon;
            case LetterRole: return app.name.left(1).toUpper();
            case HeaderVisibleRole:
                if(index.row() == 0) return true;
                return app.name.left(1).toUpper() != m_apps[index.row()-1].name.left(1).toUpper();
            case DesktopFileRole: return app.desktopFilePath;
            default: return QVariant();
        }
    }

    QHash<int, QByteArray> roleNames() const override {
        return {
            {NameRole, "name"},
            {CommandRole, "command"},
            {IconRole, "icon"},
            {LetterRole, "letter"},
            {HeaderVisibleRole, "headerVisible"},
            {DesktopFileRole, "desktopFilePath"}
        };
    }

private:
    QList<AppInfo> m_apps;
};

// ----------------------------
// AppLauncher class
// ----------------------------
class AppLauncher : public QObject
{
    Q_OBJECT
public:
    explicit AppLauncher(QObject* parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE void startSystemDrag(const QString &desktopFilePath, QQuickItem *iconItem)
    {
        if (!iconItem || !iconItem->window())
            return;

        QFileInfo fi(desktopFilePath);
        if (!fi.exists()) {
            qWarning() << "Desktop file not found:" << desktopFilePath;
            return;
        }

        QMimeData *mimeData = new QMimeData;
        mimeData->setUrls({ QUrl::fromLocalFile(desktopFilePath) });
        QByteArray specialData("copy\n" + QUrl::fromLocalFile(desktopFilePath).toEncoded() + "\n");
        mimeData->setData("x-special/gnome-copied-files", specialData);

        QDrag *drag = new QDrag(iconItem->window());
        drag->setMimeData(mimeData);

        QQuickWindow *win = iconItem->window();
        if (win) {
            QImage rendered = win->grabWindow();
            if (!rendered.isNull()) {
                QPoint itemPos = iconItem->mapToScene(QPointF(0, 0)).toPoint();
                QRect srcRect(itemPos, QSize(iconItem->width(), iconItem->height()));
                QPixmap dragPixmap = QPixmap::fromImage(rendered.copy(srcRect));
                drag->setPixmap(dragPixmap);
                drag->setHotSpot(QPoint(dragPixmap.width()/2, dragPixmap.height()/2));
            }
        }

        drag->exec(Qt::CopyAction);
    }

    Q_INVOKABLE void launchApp(const QString &command) {
        if (command.isEmpty()) {
            qWarning() << "⚠️ launchApp: Empty command.";
            return;
        }

        QString cmd = command.trimmed();

        // Remove .desktop placeholders (like %U, %u, %f, etc.)
        cmd.replace(QRegularExpression("%[uUfFdDnNvVmM]"), "");

        // Expand environment variables ($VAR or ${VAR})
        QRegularExpression envVarPattern(R"(\$(\w+)|\$\{([^}]+)\})");
    QRegularExpressionMatchIterator it = envVarPattern.globalMatch(cmd);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString varName = match.captured(1);
        if (varName.isEmpty())
            varName = match.captured(2);
        QString value = QString::fromUtf8(qgetenv(varName.toUtf8()));
        if (!value.isEmpty())
            cmd.replace(match.captured(0), value);
    }

    // Split command into executable and arguments
    QStringList parts = QProcess::splitCommand(cmd);
    if (parts.isEmpty()) {
        qWarning() << "⚠️ launchApp: Could not parse command:" << command;
        return;
    }

    QString program = parts.takeFirst();

    // Resolve program path (search PATH if needed)
    QString programPath = QFile::exists(program)
        ? program
        : QStandardPaths::findExecutable(program);

    if (programPath.isEmpty()) {
        qWarning() << "❌ launchApp: Executable not found:" << program;
        return;
    }

    // Debug info
    qDebug() << "🚀 Launching:" << programPath << "args:" << parts;

    // Launch detached process safely
    bool ok = QProcess::startDetached(programPath, parts);
    if (!ok)
        qWarning() << "❌ launchApp: Failed to start" << programPath;
    else
        qDebug() << "✅ launchApp: Started successfully.";
}


Q_INVOKABLE QVariantList listApplications() {
    QVariantList appList;
    QStringList dirs = {
        "/usr/share/applications",
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation)
    };

    for (const QString &dirPath : dirs) {
        QDir dir(dirPath);
        QStringList files = dir.entryList(QStringList() << "*.desktop", QDir::Files);

        for (const QString &file : files) {
            QString path = dir.absoluteFilePath(file);
            QFile f(path);

            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                qWarning() << "⚠️ Could not open:" << path;
                continue;
            }

            QString name, exec, iconName;
            bool noDisplay = false;
            bool inMainSection = false;

            while (!f.atEnd()) {
                QString line = f.readLine().trimmed();

                if (line.startsWith('[')) {
                    // Only read inside [Desktop Entry]
                    if (line == "[Desktop Entry]") {
                        inMainSection = true;
                        continue;
                    } else if (line.startsWith("[Desktop Action")) {
                        // Stop reading when we reach any action section
                        break;
                    } else {
                        inMainSection = false;
                        continue;
                    }
                }

                if (!inMainSection)
                    continue;

                if (line.startsWith("Name=")) {
                    name = line.mid(5).trimmed();
                } else if (line.startsWith("Exec=")) {
                    exec = line.mid(5).trimmed();
                    // Remove field codes like %U, %u, %F, etc. — optional placeholders
                    exec.replace(QRegularExpression("%[UuFfDdNnVvMm]"), "");
                } else if (line.startsWith("Icon=")) {
                    iconName = line.mid(5).trimmed();
                } else if (line.startsWith("NoDisplay=") &&
                    line.mid(10).trimmed().toLower() == "true") {
                    noDisplay = true;
                    }
            }

            f.close();

            if (name.isEmpty() || exec.isEmpty() || noDisplay) {
                qDebug() << "⚠️ Skipping:" << path
                << " name:" << name
                << " exec:" << exec
                << " noDisplay:" << noDisplay;
                continue;
            }

            QVariantMap app;
            app["name"] = name;
            app["command"] = exec;
            app["icon"] = resolveIcon(iconName);
            app["desktopFilePath"] = path;
            appList.append(app);

            qDebug() << "✅ Added:" << name << "→" << exec;
        }
    }

    std::sort(appList.begin(), appList.end(), [](const QVariant &a, const QVariant &b) {
        QString nameA = a.toMap()["name"].toString();
        QString nameB = b.toMap()["name"].toString();
        return nameA.toLower() < nameB.toLower();
    });

    qDebug() << "✅ Total apps loaded:" << appList.size();
    return appList;
}


    Q_INVOKABLE QString resolveIcon(const QString& name) const {
        if (name.isEmpty())
            return "qrc:/placeholder.svg";

        if (QFile::exists(name))
            return "file://" + name;

        QString home = QDir::homePath();
        const QStringList iconDirs = {
            "/usr/share/icons/hicolor/256x256/apps/",
            "/usr/share/icons/hicolor/128x128/apps/",
            "/usr/share/icons/hicolor/64x64/apps/",
            "/usr/share/icons/hicolor/48x48/apps/",
            "/usr/share/icons/hicolor/scalable/apps/",
            "/usr/share/pixmaps/",
            home + "/.local/share/icons/hicolor/256x256/apps/"
        };

        for (const QString& dir : iconDirs) {
            QString pngPath = dir + name + ".png";
            QString svgPath = dir + name + ".svg";
            if (QFile::exists(pngPath)) return "file://" + pngPath;
                if (QFile::exists(svgPath)) return "file://" + svgPath;
        }

        return "qrc:/placeholder.svg";
    }
};

// ----------------------------
// Tile Model (JSON persistent)
// ----------------------------
struct Tile {
    QString name;
    QString icon;
    QString desktopFile;
    QString command;
    double x, y;
    QString size;
};

class TileModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { NameRole = Qt::UserRole + 1, IconRole, DesktopFileRole, CommandRole, XRole, YRole, SizeRole };

    TileModel(QObject *parent = nullptr) : QAbstractListModel(parent) { load(); }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return m_tiles.count();
    }

    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid() || index.row() >= m_tiles.count())
            return QVariant();
        const Tile &t = m_tiles[index.row()];
        switch (role) {
            case NameRole: return t.name;
            case IconRole: return t.icon;
            case DesktopFileRole: return t.desktopFile;
            case CommandRole: return t.command;
            case XRole: return t.x;
            case YRole: return t.y;
            case SizeRole: return t.size;
            default: return QVariant();
        }
    }

    QHash<int, QByteArray> roleNames() const override {
        return {{NameRole, "name"}, {IconRole, "icon"}, {DesktopFileRole, "desktopFile"}, {CommandRole, "command"},
        {XRole, "x"}, {YRole, "y"}, {SizeRole, "size"}};
    }

    Q_INVOKABLE void addTileFromDesktopFile(const QString &filePath, double dropX, double dropY) {
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

        QString name, icon, command;
        bool inMainSection = false;

        while (!f.atEnd()) {
            QString line = f.readLine().trimmed();

            if (line.startsWith('[')) {
                // Only process the main [Desktop Entry] section
                if (line == "[Desktop Entry]") {
                    inMainSection = true;
                    continue;
                } else if (line.startsWith("[Desktop Action")) {
                    // Stop reading once subactions begin
                    break;
                } else {
                    inMainSection = false;
                    continue;
                }
            }

            if (!inMainSection)
                continue;

            if (line.startsWith("Name="))
                name = line.mid(5).trimmed();
            else if (line.startsWith("Icon="))
                icon = line.mid(5).trimmed();
            else if (line.startsWith("Exec=")) {
                command = line.mid(5).trimmed();
                // Remove placeholders like %U, %F, %u etc.
                command.replace(QRegularExpression("%[UuFfDdNnVvMm]"), "");
            }
        }

        f.close();

        if (name.isEmpty())
            name = QFileInfo(filePath).baseName();

        beginInsertRows(QModelIndex(), m_tiles.count(), m_tiles.count());
        m_tiles.append({name, icon, filePath, command, dropX, dropY, "medium"});
        endInsertRows();

        save();

        qDebug() << "✅ Added tile:" << name << "→" << command;
    }



    Q_INVOKABLE void updateTilePosition(int index, double x, double y) {
        if (index < 0 || index >= m_tiles.count()) return;
        m_tiles[index].x = x;
        m_tiles[index].y = y;
        emit dataChanged(this->index(index), this->index(index), {XRole, YRole});
        save();
    }

    Q_INVOKABLE void resizeTile(int index, const QString &size) {
        if (index < 0 || index >= m_tiles.count()) return;
        m_tiles[index].size = size;
        emit dataChanged(this->index(index), this->index(index), {SizeRole});
        save();
    }

    Q_INVOKABLE void removeTile(int index) {
        if (index < 0 || index >= m_tiles.count()) return;
        beginRemoveRows(QModelIndex(), index, index);
        m_tiles.removeAt(index);
        endRemoveRows();
        save();
    }

    void load() {
        QFile file(jsonPath());
        if (!file.exists()) return;
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();
            QJsonArray arr = doc.array();
            beginResetModel();
            m_tiles.clear();
            for (auto v : arr) {
                QJsonObject o = v.toObject();
                m_tiles.append({
                    o["name"].toString(),
                               o["icon"].toString(),
                               o["desktopFile"].toString(),
                               o["command"].toString(),
                               o["x"].toDouble(),
                               o["y"].toDouble(),
                               o["size"].toString("medium")
                });
            }
            endResetModel();
        }
    }

    void save() const {
        QJsonArray arr;
        for (const auto &t : m_tiles) {
            QJsonObject o;
            o["name"] = t.name;
            o["icon"] = t.icon;
            o["desktopFile"] = t.desktopFile;
            o["command"] = t.command;
            o["x"] = t.x;
            o["y"] = t.y;
            o["size"] = t.size;
            arr.append(o);
        }
        QFile file(jsonPath());
        QDir().mkpath(QFileInfo(file).absolutePath());
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
            file.close();
        }
    }

private:
    QList<Tile> m_tiles;
    QString jsonPath() const {
        return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + "/launcher_tiles.json";
    }
};

// ----------------------------
// main()
// ----------------------------
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    AppLauncher launcher;
    AppModel appModel;
    TileModel tileModel;

    QVariantList apps = launcher.listApplications();
    QList<AppInfo> appInfos;
    for (const QVariant &v : apps) {
        QVariantMap map = v.toMap();
        appInfos.append({ map["name"].toString(),
            map["command"].toString(),
                        map["icon"].toString(),
                        map["desktopFilePath"].toString() });
    }
    appModel.setApps(appInfos);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("AppLauncher", &launcher);
    engine.rootContext()->setContextProperty("appModel", &appModel);
    engine.rootContext()->setContextProperty("tileModel", &tileModel);

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    QQuickWindow* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window)
        return -1;

    auto layerWindow = LayerShellQt::Window::get(window);

    // Set the window layer (top layer)
    layerWindow->setLayer(LayerShellQt::Window::LayerTop);

    // Keyboard interactivity as needed
    layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);

    // Anchors (optional, you can anchor where you want)
    layerWindow->setAnchors({
        LayerShellQt::Window::AnchorBottom,   // example anchor
        LayerShellQt::Window::AnchorLeft
        // You can add Right/Bottom if needed
    });

    // Non-exclusive zone (-1 is full exclusive, 0 is non-exclusive; using 0 or not setting works)
    layerWindow->setExclusiveZone(0);

    // Optional margins
    layerWindow->setMargins({0, 0, 0, 0});

    // Set Qt window flags
    window->setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // Set fixed size to 800x600
    window->setWidth(1920);
    window->setHeight(1080);
    window->show();


    return app.exec();
}

#include "main.moc"
