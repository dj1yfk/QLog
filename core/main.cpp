#include <QApplication>
#include <QtSql/QtSql>
#include <QMessageBox>
#include <QProgressDialog>
#include <QResource>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include "Migration.h"
#include "ui/DbDialog.h"
#include "ui/MainWindow.h"
#include "Cty.h"
#include "Rig.h"

static void loadStylesheet(QApplication* app) {
    QFile style(":/res/stylesheet.css");
    style.open(QFile::ReadOnly | QIODevice::Text);
    app->setStyleSheet(style.readAll());
    style.close();
}

static void setupTranslator(QApplication* app) {
    QTranslator* qtTranslator = new QTranslator(app);
    qtTranslator->load("qt_" + QLocale::system().name(),
    QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    app->installTranslator(qtTranslator);

    QTranslator* translator = new QTranslator(app);
    translator->load(":/i18n/qlog_" + QLocale::system().name().left(2));
    app->installTranslator(translator);
}

static void createDataDirectory() {
    QDir dataDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation));

    if (!dataDir.exists()) {
        dataDir.mkpath(dataDir.path());
    }
}

static bool openDatabase() {
    QSettings settings;

    QString hostname = settings.value("db/hostname").toString();
    QString dbname = settings.value("db/dbname").toString();

    if (hostname.isEmpty() || dbname.isEmpty()) return false;

    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName(hostname);
    db.setPort(settings.value("db/port").toInt());
    db.setDatabaseName(dbname);
    db.setUserName(settings.value("db/username").toString());
    db.setPassword(settings.value("db/password").toString());

    if (!db.open()) {
        qCritical() << db.lastError();
        return false;
    }
    else {
        return true;
    }
}

static bool migrateDatabase() {
    Migration m;
    return m.run();
}

static bool updateDxcc() {
    QProgressDialog progress("Updating DXCC entities...", nullptr, 0, 346);
    progress.show();

    Cty cty;

    QObject::connect(&cty, &Cty::progress, &progress, &QProgressDialog::setValue);
    QObject::connect(&cty, &Cty::finished, &progress, &QProgressDialog::done);

    cty.update();
    return progress.exec();
}

static void startRigThread() {
    QThread* rigThread = new QThread;
    Rig* rig = Rig::instance();
    rig->moveToThread(rigThread);
    QObject::connect(rigThread, SIGNAL(started()), rig, SLOT(start()));
    rigThread->start();
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setApplicationVersion(VERSION);
    app.setOrganizationName("DL2IC");
    app.setApplicationName("QLog");

    loadStylesheet(&app);
    setupTranslator(&app);
    createDataDirectory();

    while (!openDatabase()) {
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                              QMessageBox::tr("Could not connect to database."));

        DbDialog dbDialog;
        if (!dbDialog.exec()) {
            return 0;
        }
    }

    if (!migrateDatabase()) {
        QMessageBox::critical(nullptr, QMessageBox::tr("QLog Error"),
                              QMessageBox::tr("Database migration failed."));
        return 1;
    }

    if (!updateDxcc()) {
        QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                              QMessageBox::tr("DXCC update failed."));
    }

    startRigThread();

    MainWindow w;
    QIcon icon(":/res/qlog.png");
    w.setWindowIcon(icon);
    w.show();

    return app.exec();
}
