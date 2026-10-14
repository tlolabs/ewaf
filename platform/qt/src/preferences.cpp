// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#include "preferences.h"
#include "identity.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QDebug>
#include <vector>

#if defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <QLibrary>
#endif

namespace EWAF {

std::shared_ptr<IPreferences> Preferences::s_store = std::make_shared<DesktopPreferences>();

IPreferences &Preferences::store() {
    return *s_store;
}

void Preferences::setStore(std::shared_ptr<IPreferences> store) {
    s_store = std::move(store);
}

QString Preferences::read(const QString &name, const QString &fallback) {
    return store().read(name, fallback);
}

bool Preferences::write(const QString &name, const QString &value) {
    return store().write(name, value);
}

DesktopPreferences::DesktopPreferences() {
#if defined(Q_OS_WIN)
    m_isWindows = true;
    QString session = qEnvironmentVariable("EWAF_TEST_SESSION");
    m_registryKey = QStringLiteral("Software\\tlolabs\\EWAF");
    if (!session.isEmpty()) {
        m_registryKey += QStringLiteral("\\TestSessions\\") + session;
    }
#else
    m_isWindows = false;
    QString session = qEnvironmentVariable("EWAF_TEST_SESSION");
    QString root = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (root.isEmpty()) {
#if defined(Q_OS_MACOS)
        root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
        root = QDir::homePath() + QStringLiteral("/.config");
#endif
    }

    QString subDir = Identity::applicationId();
    if (!session.isEmpty()) {
        subDir += QStringLiteral("/") + session;
    }
    m_file = root + QStringLiteral("/") + subDir + QStringLiteral("/preferences.json");

    if (QFile::exists(m_file)) {
        QFile file(m_file);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
            if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject obj = doc.object();
                for (auto it = obj.begin(); it != obj.end(); ++it) {
                    m_values.insert(it.key(), it.value().toString());
                }
            } else {
                qWarning() << "Preferences file is corrupt or invalid JSON:" << m_file
                           << "Error:" << parseError.errorString() << ". Falling back to defaults.";
            }
        }
    } else {
#if defined(Q_OS_LINUX)
        if (session.isEmpty()) {
            importLinux();
        }
#endif
    }
#endif
}

QString DesktopPreferences::decodeLegacy(const QString &value) {
    QString trimmed = value.trimmed();
    if (trimmed == QStringLiteral("true")) return QStringLiteral("1");
    if (trimmed == QStringLiteral("false")) return QStringLiteral("0");
    if (trimmed.startsWith(QStringLiteral("int64 "))) return trimmed.mid(6);
    if ((trimmed.startsWith('\'') && trimmed.endsWith('\'')) ||
        (trimmed.startsWith('"') && trimmed.endsWith('"'))) {
        return trimmed.mid(1, trimmed.length() - 2);
    }
    return trimmed;
}

#if defined(Q_OS_LINUX)
typedef void *(*dconf_client_new_fn)();
typedef void *(*dconf_client_read_fn)(void *, const char *);
typedef char *(*g_variant_print_fn)(void *, int);
typedef void (*g_variant_unref_fn)(void *);
typedef void (*g_free_fn)(void *);
typedef void (*g_object_unref_fn)(void *);

QString DesktopPreferences::readDconf(const QString &key) {
    QLibrary dconfLib("dconf", 1);
    QLibrary glibLib("glib-2.0", 0);
    QLibrary gobjectLib("gobject-2.0", 0);

    if (dconfLib.load() && glibLib.load() && gobjectLib.load()) {
        auto dconf_new = (dconf_client_new_fn)dconfLib.resolve("dconf_client_new");
        auto dconf_read = (dconf_client_read_fn)dconfLib.resolve("dconf_client_read");
        auto var_print = (g_variant_print_fn)glibLib.resolve("g_variant_print");
        auto var_unref = (g_variant_unref_fn)glibLib.resolve("g_variant_unref");
        auto glib_free = (g_free_fn)glibLib.resolve("g_free");
        auto gobj_unref = (g_object_unref_fn)gobjectLib.resolve("g_object_unref");

        if (dconf_new && dconf_read && var_print && var_unref && glib_free && gobj_unref) {
            void *client = dconf_new();
            if (client) {
                QByteArray fullPath = (QStringLiteral("/com/tlolabs/ewaf/") + key).toUtf8();
                void *val = dconf_read(client, fullPath.constData());
                if (val) {
                    char *printed = var_print(val, 0);
                    QString result = QString::fromUtf8(printed);
                    glib_free(printed);
                    var_unref(val);
                    gobj_unref(client);
                    return result;
                }
                gobj_unref(client);
            }
        }
    }

    // Fallback to dconf command-line utility if library resolution is unavailable
    QProcess process;
    process.start(QStringLiteral("dconf"), {QStringLiteral("read"), QStringLiteral("/com/tlolabs/ewaf/") + key});
    if (process.waitForFinished(1000) && process.exitCode() == 0) {
        return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    }
    return {};
}

void DesktopPreferences::importLinux() {
    static const struct {
        const char *oldName;
        const char *newName;
    } mappings[] = {
        {"default-weekday", "defaultWeekday"},
        {"range-start", "rangeStart"},
        {"range-end", "rangeEnd"},
        {"width", "width"},
        {"height", "height"},
        {"automatic-updates", "automaticUpdates"},
        {"update-last-success", "updateLastSuccess"},
        {"update-last-attempt", "updateLastAttempt"}
    };

    bool hasAny = false;
    for (const auto &m : mappings) {
        QString raw = readDconf(QString::fromLatin1(m.oldName));
        if (!raw.isEmpty()) {
            m_values.insert(QString::fromLatin1(m.newName), decodeLegacy(raw));
            hasAny = true;
        }
    }
    if (hasAny) {
        save();
    }
}
#else
QString DesktopPreferences::readDconf(const QString &) { return {}; }
void DesktopPreferences::importLinux() {}
#endif

QString DesktopPreferences::decodeRegistryString(const unsigned char *data, unsigned long bytes, unsigned long type, const QString &fallback) {
    // 1 == REG_SZ, 2 == REG_EXPAND_SZ
    if (type != 1 && type != 2) {
        return fallback;
    }
    if (bytes == 0) return QString();
    if (!data || bytes % 2 != 0 || bytes > 65536) return fallback;
    // Registry strings are UTF-16LE even when this decoder is tested on a host
    // with 32-bit wchar_t. Byte decoding also accepts unaligned registry buffers.
    QString decoded;
    decoded.reserve(static_cast<qsizetype>(bytes / 2));
    for (unsigned long i = 0; i < bytes; i += 2) {
        const char16_t unit = static_cast<char16_t>(data[i] | (static_cast<unsigned int>(data[i + 1]) << 8));
        if (unit == 0) break;
        decoded.append(QChar(unit));
    }
    return decoded;
}

QString DesktopPreferences::read(const QString &name, const QString &fallback) {
#if defined(Q_OS_WIN)
    HKEY hKey;
    std::wstring subKey = m_registryKey.toStdWString();
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        std::wstring valName = name.toStdWString();
        WCHAR buffer[1024];
        DWORD bufferSize = sizeof(buffer);
        DWORD type = 0;
        LSTATUS status = RegQueryValueExW(hKey, valName.c_str(), nullptr, &type,
                                          reinterpret_cast<LPBYTE>(buffer), &bufferSize);
        if (status == ERROR_MORE_DATA && bufferSize <= 65536) {
            std::vector<BYTE> dynamicBuffer(bufferSize);
            status = RegQueryValueExW(hKey, valName.c_str(), nullptr, &type,
                                      dynamicBuffer.data(), &bufferSize);
            RegCloseKey(hKey);
            if (status == ERROR_SUCCESS) {
                return decodeRegistryString(dynamicBuffer.data(), bufferSize, type, fallback);
            }
            return fallback;
        }
        RegCloseKey(hKey);
        if (status == ERROR_SUCCESS) {
            return decodeRegistryString(reinterpret_cast<const unsigned char *>(buffer), bufferSize, type, fallback);
        }
    }
    return fallback;
#else
    return m_values.value(name, fallback);
#endif
}

bool DesktopPreferences::write(const QString &name, const QString &value) {
#if defined(Q_OS_WIN)
    HKEY hKey;
    std::wstring subKey = m_registryKey.toStdWString();
    LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, subKey.c_str(), 0, nullptr,
                                    REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    if (status != ERROR_SUCCESS) {
        qWarning() << "RegCreateKeyExW failed for" << m_registryKey << "status:" << status;
        return false;
    }
    std::wstring valName = name.toStdWString();
    std::wstring valData = value.toStdWString();
    status = RegSetValueExW(hKey, valName.c_str(), 0, REG_SZ,
                            reinterpret_cast<const BYTE *>(valData.c_str()),
                            static_cast<DWORD>((valData.length() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    if (status != ERROR_SUCCESS) {
        qWarning() << "RegSetValueExW failed for" << name << "status:" << status;
        return false;
    }
    return true;
#else
    const auto previous = m_values;
    m_values.insert(name, value);
    if (save()) return true;
    m_values = previous;
    return false;
#endif
}

bool DesktopPreferences::save() {
#if !defined(Q_OS_WIN)
    if (m_file.isEmpty()) return false;

    QFileInfo info(m_file);
    if (!QDir().mkpath(info.absolutePath())) {
        qWarning() << "Failed to create directory for preferences:" << info.absolutePath();
        return false;
    }

    QJsonObject root;
    for (auto it = m_values.begin(); it != m_values.end(); ++it) {
        root.insert(it.key(), it.value());
    }

    QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    QSaveFile saveFile(m_file);
    if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Failed to open atomic preferences file:" << m_file << "-" << saveFile.errorString();
        return false;
    }
    if (saveFile.write(data) != data.size()) {
        qWarning() << "Failed to write complete preferences data:" << m_file << "-" << saveFile.errorString();
        saveFile.cancelWriting();
        return false;
    }
    if (!saveFile.commit()) {
        qWarning() << "Failed to commit atomic preferences file:" << m_file << "-" << saveFile.errorString();
        return false;
    }
    return true;
#else
    return true;
#endif
}

} // namespace EWAF
