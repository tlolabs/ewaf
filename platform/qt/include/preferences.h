// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EWAF_PREFERENCES_H
#define EWAF_PREFERENCES_H

#include <QMap>
#include <QString>
#include <memory>

namespace EWAF {

class IPreferences {
public:
    virtual ~IPreferences() = default;
    virtual QString read(const QString &name, const QString &fallback) = 0;
    virtual void write(const QString &name, const QString &value) = 0;
};

class DesktopPreferences : public IPreferences {
public:
    DesktopPreferences();
    QString read(const QString &name, const QString &fallback) override;
    void write(const QString &name, const QString &value) override;

    static QString decodeLegacy(const QString &value);

private:
    void importLinux();
    void save();
    static QString readDconf(const QString &key);

    QMap<QString, QString> m_values;
    QString m_file;
    QString m_registryKey;
    bool m_isWindows = false;
};

class MemoryPreferences : public IPreferences {
public:
    QString read(const QString &name, const QString &fallback) override {
        return m_values.value(name, fallback);
    }
    void write(const QString &name, const QString &value) override {
        m_values.insert(name, value);
    }

private:
    QMap<QString, QString> m_values;
};

class Preferences {
public:
    static IPreferences &store();
    static void setStore(std::shared_ptr<IPreferences> store);
    static QString read(const QString &name, const QString &fallback);
    static void write(const QString &name, const QString &value);

private:
    static std::shared_ptr<IPreferences> s_store;
};

} // namespace EWAF

#endif // EWAF_PREFERENCES_H
