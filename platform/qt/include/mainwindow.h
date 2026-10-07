// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EWAF_MAINWINDOW_H
#define EWAF_MAINWINDOW_H

#include "workspace.h"

#include <QMainWindow>
#include <QPointer>
#include <QPoint>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTimer;

namespace EWAF {

class MainWindow : public QMainWindow, public IWorkspaceDialogs {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    Workspace *model() { return m_workspace.get(); }

    // IWorkspaceDialogs implementation
    QString chooseFolder() override;
    bool confirm(const QString &title, const QString &message, const QString &accept = QStringLiteral("Continue")) override;
    void message(const QString &title, const QString &message) override;

    void copyName();
    void showSettings();

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onStateChanged();
    void onNewWindow();
    void onFind();
    void onReveal();
    void onHelp();
    void onAbout();
    void onUpdates();

private:
    void setupUi();
    void setupMenus();
    bool showCustomDialog(const QString &title, QWidget *content, const QString &acceptText);
    void pickDate(QLineEdit *targetEdit);

    std::unique_ptr<Workspace> m_workspace;
    QTimer *m_updateTimer = nullptr;
    bool m_firstShow = true;

    // UI Widgets
    QLineEdit *m_startDateEdit = nullptr;
    QLineEdit *m_endDateEdit = nullptr;
    QComboBox *m_weekdayCombo = nullptr;
    QLabel *m_destinationLabel = nullptr;
    QPushButton *m_chooseDestBtn = nullptr;
    QPushButton *m_openFolderBtn = nullptr;

    QLabel *m_folderCountLabel = nullptr;
    QLineEdit *m_searchBox = nullptr;
    QListWidget *m_previewList = nullptr;

    QPushButton *m_createBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_statusLabel = nullptr;

    QPoint m_dragStartPos;
};

} // namespace EWAF

#endif // EWAF_MAINWINDOW_H
