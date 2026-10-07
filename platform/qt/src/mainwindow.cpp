// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"
#include "app.h"
#include "identity.h"
#include "preferences.h"
#include "updatecoordinator.h"

#include <QApplication>
#include <QCalendarWidget>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDrag>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QMessageBox>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QCheckBox>
#include <iostream>

namespace EWAF {

static int clampDimension(const QString &key, int fallback, int minVal, int maxVal) {
    bool ok = false;
    int val = Preferences::read(key, QString::number(fallback)).toInt(&ok);
    if (!ok) return fallback;
    return std::max(minVal, std::min(maxVal, val));
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_workspace(std::make_unique<Workspace>(this, &Preferences::store(), this)),
      m_updateTimer(new QTimer(this)) {
    setWindowTitle(Identity::title());
    int w = clampDimension(QStringLiteral("width"), 850, 640, 3000);
    int h = clampDimension(QStringLiteral("height"), 650, 480, 2000);
    resize(w, h);
    setMinimumSize(640, 480);

    setupUi();
    setupMenus();

    connect(m_workspace.get(), &Workspace::stateChanged, this, &MainWindow::onStateChanged);
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        UpdateCoordinator::check(this, false);
    });

    onStateChanged();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupMenus() {
    QMenuBar *menuBar = this->menuBar();

    // File Menu
    QMenu *fileMenu = menuBar->addMenu(tr("&File"));

    QAction *newWindowAct = fileMenu->addAction(tr("&New Window"), this, &MainWindow::onNewWindow);
    newWindowAct->setShortcut(QKeySequence::New);

    QAction *chooseDestAct = fileMenu->addAction(tr("Choose &Destination\u2026"), this, [this]() {
        m_workspace->choose();
    });
    chooseDestAct->setShortcut(QKeySequence::Open);

    m_revealAct = fileMenu->addAction(tr("&Open Destination in File Manager"), this, &MainWindow::onReveal);
    m_revealAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    m_revealAct->setEnabled(false);

    QAction *createAct = fileMenu->addAction(tr("&Create Folders"), this, [this]() {
        m_workspace->create();
    });
    createAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));

    QAction *cancelAct = fileMenu->addAction(tr("Cancel Folder Creation"), this, [this]() {
        m_workspace->cancel();
    });
    cancelAct->setShortcut(QKeySequence(Qt::Key_Escape));

    fileMenu->addSeparator();

    QAction *settingsAct = fileMenu->addAction(tr("&Settings\u2026"), this, &MainWindow::showSettings);
    settingsAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));

    QAction *closeAct = fileMenu->addAction(tr("&Close"), this, &MainWindow::close);
    closeAct->setShortcut(QKeySequence::Close);

    // Edit Menu
    QMenu *editMenu = menuBar->addMenu(tr("&Edit"));
    QAction *findAct = editMenu->addAction(tr("&Find"), this, &MainWindow::onFind);
    findAct->setShortcut(QKeySequence::Find);

    QAction *copyAct = editMenu->addAction(tr("Copy Folder Name"), this, &MainWindow::copyName);
    copyAct->setShortcut(QKeySequence::Copy);

    // Help Menu
    QMenu *helpMenu = menuBar->addMenu(tr("&Help"));
    helpMenu->addAction(tr("EWAF Help"), this, &MainWindow::onHelp);
    helpMenu->addAction(tr("Check for Updates\u2026"), this, &MainWindow::onUpdates);
    helpMenu->addAction(tr("About EWAF"), this, &MainWindow::onAbout);
}

void MainWindow::setupUi() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(20, 16, 20, 20);
    mainLayout->setSpacing(16);

    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(24);

    // Left Column
    QWidget *leftColWidget = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftColWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);
    leftColWidget->setFixedWidth(280);

    QLabel *heading = new QLabel(tr("Every Week a Folder"));
    QFont headFont = heading->font();
    headFont.setPointSize(16);
    headFont.setBold(true);
    heading->setFont(headFont);
    leftLayout->addWidget(heading);

    // Start Date
    QLabel *startLabel = new QLabel(tr("Start date (MM-DD-YYYY)"));
    leftLayout->addWidget(startLabel);

    QHBoxLayout *startBox = new QHBoxLayout();
    m_startDateEdit = new QLineEdit();
    m_startDateEdit->setObjectName(QStringLiteral("StartDate"));
    m_startDateEdit->setAccessibleName(QStringLiteral("Start date"));
    m_startDateEdit->setToolTip(tr("Start date for the folder creation range (MM-DD-YYYY)"));
    m_startDateEdit->setAccessibleDescription(tr("Enter the starting date in MM-DD-YYYY format."));
    m_startDateEdit->setText(m_workspace->start());
    connect(m_startDateEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_workspace->setStart(text);
    });
    startBox->addWidget(m_startDateEdit);

    QToolButton *startCalBtn = new QToolButton();
    startCalBtn->setText(QStringLiteral("\U0001F4C5"));
    startCalBtn->setToolTip(tr("Choose start date from calendar"));
    startCalBtn->setAccessibleName(tr("Choose start date"));
    startCalBtn->setAccessibleDescription(tr("Opens a calendar dialog to select the start date."));
    connect(startCalBtn, &QToolButton::clicked, this, [this]() {
        pickDate(m_startDateEdit);
    });
    startBox->addWidget(startCalBtn);
    leftLayout->addLayout(startBox);

    // End Date
    QLabel *endLabel = new QLabel(tr("End date (MM-DD-YYYY)"));
    leftLayout->addWidget(endLabel);

    QHBoxLayout *endBox = new QHBoxLayout();
    m_endDateEdit = new QLineEdit();
    m_endDateEdit->setObjectName(QStringLiteral("EndDate"));
    m_endDateEdit->setAccessibleName(QStringLiteral("End date"));
    m_endDateEdit->setToolTip(tr("End date for the folder creation range (MM-DD-YYYY)"));
    m_endDateEdit->setAccessibleDescription(tr("Enter the ending date in MM-DD-YYYY format."));
    m_endDateEdit->setText(m_workspace->end());
    connect(m_endDateEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_workspace->setEnd(text);
    });
    endBox->addWidget(m_endDateEdit);

    QToolButton *endCalBtn = new QToolButton();
    endCalBtn->setText(QStringLiteral("\U0001F4C5"));
    endCalBtn->setToolTip(tr("Choose end date from calendar"));
    endCalBtn->setAccessibleName(tr("Choose end date"));
    endCalBtn->setAccessibleDescription(tr("Opens a calendar dialog to select the end date."));
    connect(endCalBtn, &QToolButton::clicked, this, [this]() {
        pickDate(m_endDateEdit);
    });
    endBox->addWidget(endCalBtn);
    leftLayout->addLayout(endBox);

    // Weekday
    QLabel *weekdayLabel = new QLabel(tr("Weekday"));
    leftLayout->addWidget(weekdayLabel);

    m_weekdayCombo = new QComboBox();
    m_weekdayCombo->setObjectName(QStringLiteral("WeekdayChoice"));
    m_weekdayCombo->setAccessibleName(QStringLiteral("Weekday"));
    m_weekdayCombo->setToolTip(tr("Weekday on which folders should be generated"));
    m_weekdayCombo->setAccessibleDescription(tr("Select the day of the week for folder dates."));
    m_weekdayCombo->addItems(m_workspace->dayNames());
    m_weekdayCombo->setCurrentIndex(m_workspace->weekday());
    connect(m_weekdayCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        m_workspace->setWeekday(idx);
    });
    leftLayout->addWidget(m_weekdayCombo);

    QLabel *infoLabel = new QLabel(tr("Both dates are included. Exact entry supports years 0001\u20139999."));
    infoLabel->setWordWrap(true);
    leftLayout->addWidget(infoLabel);

    m_destinationLabel = new QLabel(m_workspace->destinationDisplay());
    m_destinationLabel->setWordWrap(true);
    m_destinationLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    leftLayout->addWidget(m_destinationLabel);

    m_chooseDestBtn = new QPushButton(tr("Choose Destination\u2026"));
    m_chooseDestBtn->setObjectName(QStringLiteral("ChooseDestination"));
    m_chooseDestBtn->setAccessibleName(QStringLiteral("Choose Destination\u2026"));
    m_chooseDestBtn->setToolTip(tr("Choose destination folder for folder creation"));
    m_chooseDestBtn->setAccessibleDescription(tr("Opens a directory picker to select where folders will be created."));
    connect(m_chooseDestBtn, &QPushButton::clicked, this, [this]() {
        m_workspace->choose();
    });
    leftLayout->addWidget(m_chooseDestBtn);

    m_openFolderBtn = new QPushButton(tr("Open Folder"));
    m_openFolderBtn->setObjectName(QStringLiteral("OpenFolder"));
    m_openFolderBtn->setAccessibleName(QStringLiteral("Open Folder"));
    m_openFolderBtn->setToolTip(tr("Open destination folder in file manager"));
    m_openFolderBtn->setAccessibleDescription(tr("Opens the currently selected destination folder in your file manager."));
    connect(m_openFolderBtn, &QPushButton::clicked, this, &MainWindow::onReveal);
    leftLayout->addWidget(m_openFolderBtn);

    leftLayout->addStretch();
    contentLayout->addWidget(leftColWidget);

    // Right Column
    QWidget *rightColWidget = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightColWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    m_folderCountLabel = new QLabel(m_workspace->countText());
    m_folderCountLabel->setObjectName(QStringLiteral("FolderCount"));
    m_folderCountLabel->setAccessibleName(m_workspace->countText());
    QFont countFont = m_folderCountLabel->font();
    countFont.setBold(true);
    m_folderCountLabel->setFont(countFont);
    rightLayout->addWidget(m_folderCountLabel);

    m_searchBox = new QLineEdit();
    m_searchBox->setObjectName(QStringLiteral("SearchBox"));
    m_searchBox->setAccessibleName(QStringLiteral("Find a folder date"));
    m_searchBox->setPlaceholderText(tr("Find a folder date"));
    m_searchBox->setToolTip(tr("Filter folder names in the preview list"));
    m_searchBox->setAccessibleDescription(tr("Type to search or filter folder names in the preview list."));
    connect(m_searchBox, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_workspace->setSearch(text);
    });
    rightLayout->addWidget(m_searchBox);

    m_previewList = new QListWidget();
    m_previewList->setObjectName(QStringLiteral("PreviewList"));
    m_previewList->setAccessibleName(QStringLiteral("Folder preview"));
    m_previewList->setToolTip(tr("List of folders to be created"));
    m_previewList->setAccessibleDescription(tr("Preview list of folder names matching the specified range and weekday filter."));
    m_previewList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_previewList->setDragDropMode(QAbstractItemView::DragOnly);
    connect(m_previewList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);
        menu.addAction(tr("Copy Folder Name"), this, &MainWindow::copyName);
        menu.exec(m_previewList->mapToGlobal(pos));
    });
    connect(m_previewList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *curr, QListWidgetItem *) {
        if (curr) m_workspace->setSelectedName(curr->text());
    });
    rightLayout->addWidget(m_previewList);

    QLabel *previewFooter = new QLabel(tr("Preview shows up to 200 matches in date order. Existing folders will be kept."));
    previewFooter->setWordWrap(true);
    rightLayout->addWidget(previewFooter);

    contentLayout->addWidget(rightColWidget, 1);
    mainLayout->addLayout(contentLayout, 1);

    // Bottom Area
    QVBoxLayout *bottomLayout = new QVBoxLayout();
    bottomLayout->setSpacing(8);

    QHBoxLayout *actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(12);

    m_createBtn = new QPushButton(tr("Create Folders"));
    m_createBtn->setObjectName(QStringLiteral("CreateFolders"));
    m_createBtn->setAccessibleName(QStringLiteral("Create Folders"));
    m_createBtn->setToolTip(tr("Create all previewed folders at the destination"));
    m_createBtn->setAccessibleDescription(tr("Starts creating the folders in the selected destination directory."));
    m_createBtn->setMinimumHeight(34);
    connect(m_createBtn, &QPushButton::clicked, this, [this]() {
        m_workspace->create();
    });
    actionLayout->addWidget(m_createBtn);

    m_cancelBtn = new QPushButton(tr("Cancel"));
    m_cancelBtn->setObjectName(QStringLiteral("CancelButton"));
    m_cancelBtn->setAccessibleName(QStringLiteral("Cancel"));
    m_cancelBtn->setToolTip(tr("Cancel folder creation"));
    m_cancelBtn->setAccessibleDescription(tr("Cancels the folder creation operation."));
    m_cancelBtn->setMinimumHeight(34);
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        m_workspace->cancel();
    });
    actionLayout->addWidget(m_cancelBtn);
    actionLayout->addStretch();
    bottomLayout->addLayout(actionLayout);

    m_progressBar = new QProgressBar();
    m_progressBar->setObjectName(QStringLiteral("ProgressBar"));
    m_progressBar->setAccessibleName(QStringLiteral("Folder creation progress"));
    m_progressBar->setAccessibleDescription(tr("Progress of folder creation operation."));
    m_progressBar->setRange(0, m_workspace->progressMaximum());
    m_progressBar->setValue(m_workspace->processed());
    bottomLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel(m_workspace->status());
    m_statusLabel->setObjectName(QStringLiteral("OperationStatus"));
    m_statusLabel->setAccessibleName(m_workspace->status());
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bottomLayout->addWidget(m_statusLabel);

    mainLayout->addLayout(bottomLayout);

    // Explicit Tab Order
    setTabOrder(m_startDateEdit, startCalBtn);
    setTabOrder(startCalBtn, m_endDateEdit);
    setTabOrder(m_endDateEdit, endCalBtn);
    setTabOrder(endCalBtn, m_weekdayCombo);
    setTabOrder(m_weekdayCombo, m_chooseDestBtn);
    setTabOrder(m_chooseDestBtn, m_openFolderBtn);
    setTabOrder(m_openFolderBtn, m_searchBox);
    setTabOrder(m_searchBox, m_previewList);
    setTabOrder(m_previewList, m_createBtn);
    setTabOrder(m_createBtn, m_cancelBtn);
}

void MainWindow::onStateChanged() {
    bool editable = m_workspace->isEditable();

    m_startDateEdit->setEnabled(editable);
    m_endDateEdit->setEnabled(editable);
    m_weekdayCombo->setEnabled(editable);
    m_chooseDestBtn->setEnabled(editable);
    m_openFolderBtn->setEnabled(m_workspace->hasDestination());
    m_revealAct->setEnabled(m_workspace->hasDestination());
    m_searchBox->setEnabled(editable);

    m_createBtn->setEnabled(m_workspace->canCreate());
    m_cancelBtn->setEnabled(m_workspace->canCancel());

    m_destinationLabel->setText(m_workspace->destinationDisplay());
    m_statusLabel->setText(m_workspace->status());
    m_statusLabel->setAccessibleName(m_workspace->status());

    QString countStr = m_workspace->countText();
    m_folderCountLabel->setText(countStr);
    m_folderCountLabel->setAccessibleName(countStr);

    m_progressBar->setMaximum(m_workspace->progressMaximum());
    m_progressBar->setValue(m_workspace->processed());
    if (m_workspace->isWorking()) {
        m_progressBar->setAccessibleDescription(
            QStringLiteral("%1 of %2 folders created").arg(m_workspace->processed()).arg(m_workspace->progressMaximum()));
    } else {
        m_progressBar->setAccessibleDescription(tr("Folder creation progress"));
    }

    // Update preview list
    const QStringList &preview = m_workspace->preview();
    m_previewList->clear();
    for (const QString &item : preview) {
        m_previewList->addItem(item);
    }
}

void MainWindow::pickDate(QLineEdit *targetEdit) {
    QWidget *prevFocus = targetEdit;
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Choose Date"));
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QCalendarWidget *calendar = new QCalendarWidget(&dialog);
    QDate current = QDate::fromString(targetEdit->text(), QStringLiteral("MM-dd-yyyy"));
    if (current.isValid()) {
        calendar->setSelectedDate(current);
    }
    layout->addWidget(calendar);

    QHBoxLayout *btnBox = new QHBoxLayout();
    QPushButton *okBtn = new QPushButton(tr("OK"), &dialog);
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"), &dialog);
    btnBox->addStretch();
    btnBox->addWidget(okBtn);
    btnBox->addWidget(cancelBtn);
    layout->addLayout(btnBox);

    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        targetEdit->setText(calendar->selectedDate().toString(QStringLiteral("MM-dd-yyyy")));
    }
    if (prevFocus) {
        prevFocus->setFocus();
    }
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    if (m_firstShow) {
        m_firstShow = false;
        m_startDateEdit->setFocus();
        m_workspace->refresh();

        if (App::isSmoke()) {
            m_workspace->setStart(QStringLiteral("09-03-2026"));
            m_workspace->setEnd(QStringLiteral("09-17-2026"));
            m_workspace->setWeekday(3); // Days[3] is Thursday=5
            connect(m_workspace.get(), &Workspace::refreshCompleted, this, [this]() {
                if (m_workspace->total() != 3) {
                    std::cerr << "Packaged UI validation failed: total was " << m_workspace->total() << std::endl;
                    exit(1);
                }
                std::cout << "Qt packaged window, bindings and core passed." << std::endl;
                close();
            });
        } else if (qEnvironmentVariable("EWAF_TEST_SESSION").isEmpty()) {
            m_updateTimer->start(3600000); // 1 hour
            UpdateCoordinator::check(this, false);
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    m_updateTimer->stop();
    m_workspace->close();

    try {
        Preferences::write(QStringLiteral("width"), QString::number(width()));
        Preferences::write(QStringLiteral("height"), QString::number(height()));
    } catch (...) {}

    App::unregisterWindow(this);
    event->accept();
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        m_workspace->cancel();
        event->accept();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

QString MainWindow::chooseFolder() {
    return QFileDialog::getExistingDirectory(this, tr("Choose Destination"), QString(),
                                             QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
}

bool MainWindow::confirm(const QString &title, const QString &message, const QString &accept) {
    return showCustomDialog(title, new QLabel(message), accept);
}

void MainWindow::message(const QString &title, const QString &message) {
    showCustomDialog(title, new QLabel(message), QString());
}

bool MainWindow::showCustomDialog(const QString &title, QWidget *content, const QString &acceptText) {
    QWidget *prevFocus = focusWidget();
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setFixedWidth(460);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    if (auto *label = qobject_cast<QLabel *>(content)) {
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }
    layout->addWidget(content);

    QHBoxLayout *btnBox = new QHBoxLayout();
    btnBox->addStretch();

    QPushButton *cancelBtn = new QPushButton(acceptText.isEmpty() ? tr("OK") : tr("Cancel"), &dialog);
    cancelBtn->setObjectName(acceptText.isEmpty() ? QStringLiteral("OkButton") : QStringLiteral("Cancel"));
    cancelBtn->setAccessibleName(acceptText.isEmpty() ? QStringLiteral("OK") : QStringLiteral("Cancel"));
    btnBox->addWidget(cancelBtn);

    QPushButton *actionBtn = nullptr;
    if (!acceptText.isEmpty()) {
        actionBtn = new QPushButton(acceptText, &dialog);
        actionBtn->setObjectName(acceptText);
        actionBtn->setAccessibleName(acceptText);
        btnBox->addWidget(actionBtn);
    }
    layout->addLayout(btnBox);

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    if (actionBtn) {
        connect(actionBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    }

    cancelBtn->setFocus();
    bool accepted = (dialog.exec() == QDialog::Accepted);
    if (prevFocus) {
        prevFocus->setFocus();
    }
    return accepted;
}

void MainWindow::onNewWindow() {
    App::newWindow()->show();
}

void MainWindow::onFind() {
    m_searchBox->setFocus();
    m_searchBox->selectAll();
}

void MainWindow::copyName() {
    QString name = m_workspace->selectedName();
    if (name.isEmpty() && m_previewList->currentItem()) {
        name = m_previewList->currentItem()->text();
    }
    if (!name.isEmpty()) {
        QApplication::clipboard()->setText(name);
    }
}

void MainWindow::onReveal() {
    QString dest = m_workspace->destination();
    if (!dest.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(dest));
    }
}

void MainWindow::onHelp() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/tlolabs/ewaf#use")));
}

void MainWindow::onAbout() {
    if (!m_workspace->isWorking()) {
        try {
            QJsonObject info = Core::call({{QStringLiteral("op"), QStringLiteral("info")}});
            QString ver = info.value(QStringLiteral("version")).toString();
            message(Identity::title(), QStringLiteral("Every Week a Folder\nVersion ") + ver);
        } catch (...) {
            message(Identity::title(), QStringLiteral("Every Week a Folder"));
        }
    }
}

void MainWindow::onUpdates() {
    UpdateCoordinator::check(this, true);
}

void MainWindow::showSettings() {
    if (!m_workspace->isEditable()) return;

    m_workspace->runWithDialog([&]() {
        QWidget *prevFocus = focusWidget();

        QDialog dialog(this);
        dialog.setWindowTitle(tr("Settings"));
        dialog.setFixedWidth(420);

        QVBoxLayout *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(12);

        layout->addWidget(new QLabel(tr("Appearance")));

        QComboBox *appearanceCombo = new QComboBox(&dialog);
        appearanceCombo->setObjectName(QStringLiteral("AppearanceCombo"));
        appearanceCombo->setAccessibleName(QStringLiteral("Appearance"));
        appearanceCombo->setToolTip(tr("Choose application appearance"));
        appearanceCombo->setAccessibleDescription(tr("Select System, Light, or Dark appearance."));
        appearanceCombo->addItem(tr("System"), QStringLiteral("system"));
        appearanceCombo->addItem(tr("Light"), QStringLiteral("light"));
        appearanceCombo->addItem(tr("Dark"), QStringLiteral("dark"));

        QString currentApp = Preferences::read(QStringLiteral("appearance"), QStringLiteral("system")).toLower();
        int appIdx = 0;
        if (currentApp == QStringLiteral("light")) {
            appIdx = 1;
        } else if (currentApp == QStringLiteral("dark")) {
            appIdx = 2;
        }
        appearanceCombo->setCurrentIndex(appIdx);
        layout->addWidget(appearanceCombo);

        layout->addWidget(new QLabel(tr("Default weekday for new windows")));

        QComboBox *combo = new QComboBox(&dialog);
        combo->setObjectName(QStringLiteral("DefaultWeekdayCombo"));
        combo->setAccessibleName(QStringLiteral("Default weekday"));
        combo->setToolTip(tr("Choose default weekday for new windows"));
        combo->setAccessibleDescription(tr("Select the default day of the week to use when creating new windows."));
        combo->addItems(m_workspace->dayNames());

        bool ok = false;
        quint32 saved = Preferences::read(QStringLiteral("defaultWeekday"), QStringLiteral("5")).toUInt(&ok);
        if (!ok) saved = 5;
        int idx = 3;
        for (size_t i = 0; i < Workspace::Days.size(); ++i) {
            if (Workspace::Days[i] == saved) {
                idx = static_cast<int>(i);
                break;
            }
        }
        combo->setCurrentIndex(idx);
        layout->addWidget(combo);

        QCheckBox *autoCheck = new QCheckBox(tr("Automatically check for updates"), &dialog);
        autoCheck->setObjectName(QStringLiteral("AutomaticUpdatesCheck"));
        autoCheck->setAccessibleName(QStringLiteral("Automatically check for updates"));
        autoCheck->setToolTip(tr("Automatically check for updates on startup"));
        autoCheck->setAccessibleDescription(tr("When enabled, the application will periodically check for updates."));
        if (Identity::isInternal()) {
            autoCheck->setChecked(false);
            autoCheck->setEnabled(false);
            layout->addWidget(autoCheck);
            QLabel *note = new QLabel(tr("Internal reference builds do not use production updates."));
            note->setWordWrap(true);
            layout->addWidget(note);
        } else {
            autoCheck->setChecked(Preferences::read(QStringLiteral("automaticUpdates"), QStringLiteral("1")) == QStringLiteral("1"));
            layout->addWidget(autoCheck);
        }

        QHBoxLayout *btnBox = new QHBoxLayout();
        btnBox->addStretch();
        QPushButton *cancelBtn = new QPushButton(tr("Cancel"), &dialog);
        QPushButton *saveBtn = new QPushButton(tr("Save"), &dialog);
        btnBox->addWidget(cancelBtn);
        btnBox->addWidget(saveBtn);
        layout->addLayout(btnBox);

        connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(saveBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

        cancelBtn->setFocus();

        if (dialog.exec() == QDialog::Accepted) {
            QString appPref = appearanceCombo->currentData().toString();
            Preferences::write(QStringLiteral("appearance"), appPref);
            App::applyAppearance(appPref);

            quint32 day = Workspace::Days[combo->currentIndex()];
            Preferences::write(QStringLiteral("defaultWeekday"), QString::number(day));
            Preferences::write(QStringLiteral("automaticUpdates"), autoCheck->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
        }

        if (prevFocus) {
            prevFocus->setFocus();
        }
    });
}

} // namespace EWAF
