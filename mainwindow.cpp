#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QClipboard>
#include <QTimer>

/**
 * @brief MainWindow::MainWindow
 * @param parent
 */
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    process = new QProcess(this);
    connect(process, SIGNAL(readyReadStandardOutput()), this, SLOT(readyRead()));
    connect(process, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
    connect(process, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished(int, QProcess::ExitStatus)));
    ui->setupUi(this);
}

/**
 * @brief MainWindow::~MainWindow
 */
MainWindow::~MainWindow()
{
    delete ui;
}

/**
 * @brief MainWindow::checkConfig
 */
void MainWindow::checkConfig() {

    QSettings settings("IJHack", "QtPass");

    usePass = (settings.value("usePass") == "true");

    useClipboard = (settings.value("useClipboard") == "true");
    useAutoclear = (settings.value("useAutoclear") == "true");
    autoclearSeconds = settings.value("autoclearSeconds").toInt();

    passStore = settings.value("passStore").toString();
    if (passStore == "") {
        passStore = QDir::homePath()+"/.password-store/";
        /** @TODO exists? */
        settings.setValue("passStore", passStore);
    }

    passExecutable = settings.value("passExecutable").toString();
    if (passExecutable == "") {
        process->start("which pass");
        process->waitForFinished();
        if (process->exitCode() == 0) {
            passExecutable = process->readAllStandardOutput().trimmed();
            settings.setValue("passExecutable", passExecutable);
            usePass = true;
            settings.setValue("usePass", "true");
        }
    }

    gitExecutable = settings.value("gitExecutable").toString();
    if (gitExecutable == "") {
        process->start("which git");
        process->waitForFinished();
        if (process->exitCode() == 0) {
            gitExecutable = process->readAllStandardOutput().trimmed();
            settings.setValue("gitExecutable", gitExecutable);
        }
    }

    gpgExecutable = settings.value("gpgExecutable").toString();
    if (gpgExecutable == "") {
        process->start("which gpg2");
        process->waitForFinished();
        if (process->exitCode() != 0) {
            process->start("which gpg");
            process->waitForFinished();
        }
        if (process->exitCode() == 0) {
            gpgExecutable = process->readAllStandardOutput().trimmed();
            settings.setValue("gpgExecutable", gpgExecutable);
        }
    }

    if (passExecutable == "" && (gitExecutable == "" || gpgExecutable == "")) {
        config();
    }

    ui->treeView->setModel(&model);
    ui->treeView->setRootIndex(model.setRootPath(passStore));
    ui->treeView->setColumnHidden(1, true);
    ui->treeView->setColumnHidden(2, true);
    ui->treeView->setColumnHidden(3, true);
    ui->treeView->header()->hide();
}

/**
 * @brief MainWindow::config
 */
void MainWindow::config() {
    d = new Dialog();
    d->setModal(true);

    d->setPassPath(passExecutable);
    d->setGitPath(gitExecutable);
    d->setGpgPath(gpgExecutable);
    d->setStorePath(passStore);
    d->usePass(usePass);
    d->useClipboard(useClipboard);
    d->useAutoclear(useAutoclear);
    d->setAutoclear(autoclearSeconds);

    if (d->exec()) {
        if (d->result() == QDialog::Accepted) {
            passExecutable = d->getPassPath();
            gitExecutable = d->getGitPath();
            gpgExecutable = d->getGpgPath();
            passStore = d->getStorePath();
            usePass = d->usePass();
            useClipboard = d->useClipboard();
            useAutoclear = d->useAutoclear();
            autoclearSeconds = d->getAutoclear();

            QSettings settings("IJHack", "QtPass");

            settings.setValue("passExecutable", passExecutable);
            settings.setValue("gitExecutable", gitExecutable);
            settings.setValue("gpgExecutable", gpgExecutable);
            settings.setValue("passStore", passStore);
            settings.setValue("usePass", usePass ? "true" : "false");
            settings.setValue("useClipboard", useClipboard ? "true" : "false");
            settings.setValue("useAutoclear", useAutoclear ? "true" : "false");
            settings.setValue("autoclearSeconds", autoclearSeconds);

            ui->treeView->setRootIndex(model.setRootPath(passStore));
        }
    }
}

/**
 * @brief MainWindow::on_updateButton_clicked
 */
void MainWindow::on_updateButton_clicked()
{
    currentAction = GIT;
    if (usePass) {
        executePass("git pull");
    } else {
        executeWrapper(gitExecutable, "pull");
    }
}

/**
 * @brief MainWindow::on_treeView_clicked
 * @param index
 */
void MainWindow::on_treeView_clicked(const QModelIndex &index)
{
    currentAction = GPG;
    if (model.fileInfo(index).isFile()){
        QString passFile = model.filePath(index);
        if (usePass) {
            passFile.replace(".gpg", "");
            passFile.replace(passStore, "");
            executePass(passFile);
        } else {
            executeWrapper(gpgExecutable , "--no-tty -dq " + passFile);
        }
    }
}

/**
 * @brief MainWindow::executePass
 * @param args
 */
void MainWindow::executePass(QString args) {
    executeWrapper(passExecutable, args);
}

/**
 * @brief MainWindow::executeWrapper
 * @param app
 * @param args
 */
void MainWindow::executeWrapper(QString app, QString args) {
    process->setWorkingDirectory(passStore);
    process->start("sh", QStringList() << "-c" << app + " " + args);
    ui->textBrowser->clear();
    ui->textBrowser->setTextColor(Qt::black);
    enableUiElements(false);
}

/**
 * @brief MainWindow::readyRead
 */
void MainWindow::readyRead() {
    QString output = ui->textBrowser->document()->toPlainText();
    QString error = process->readAllStandardError();
    if (error.size() > 0) {
        ui->textBrowser->setTextColor(Qt::red);
        output += error;
    } else {
        output += process->readAllStandardOutput();
    }
    ui->textBrowser->setText(output);
}

/**
 * @brief MainWindow::clearClipboard
 */
void MainWindow::clearClipboard()
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->clear();
    ui->statusBar->showMessage(tr("Clipboard cleared"), 3000);
}

/**
 * @brief MainWindow::processFinished
 * @param exitCode
 * @param exitStatus
 */
void MainWindow::processFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (exitStatus != QProcess::NormalExit || exitCode > 0) {
         ui->textBrowser->setTextColor(Qt::red);
    }
    readyRead();
    enableUiElements(true);
    if (currentAction == GPG && useClipboard) {
        //Copy first line to clipboard
        QClipboard *clip = QApplication::clipboard();
        QStringList tokens =  ui->textBrowser->document()->toPlainText().split("\n",QString::SkipEmptyParts);
        clip->setText(tokens[0]);
        ui->statusBar->showMessage(tr("Password copied to clipboard"), 3000);
        if (useAutoclear) {
              QTimer::singleShot(1000*autoclearSeconds, this, SLOT(clearClipboard()));
        }
    }
}

/**
 * @brief MainWindow::enableUiElements
 * @param state
 */
void MainWindow::enableUiElements(bool state) {
    ui->updateButton->setEnabled(state);
    ui->treeView->setEnabled(state);
}

/**
 * @brief MainWindow::processError
 * @param error
 */
void MainWindow::processError(QProcess::ProcessError error)
{
    QString errorString;
    switch (error) {
        case QProcess::FailedToStart:
            errorString = tr("QProcess::FailedToStart");
            break;
        case QProcess::Crashed:
            errorString = tr("QProcess::Crashed");
            break;
        case QProcess::Timedout:
            errorString = tr("QProcess::Timedout");
            break;
        case QProcess::ReadError:
            errorString = tr("QProcess::ReadError");
            break;
        case QProcess::WriteError:
            errorString = tr("QProcess::WriteError");
            break;
        case QProcess::UnknownError:
            errorString = tr("QProcess::UnknownError");
            break;
    }
    ui->textBrowser->setTextColor(Qt::red);
    ui->textBrowser->setText(errorString);
}

/**
 * @brief MainWindow::setPassExecutable
 * @param path
 */
void MainWindow::setPassExecutable(QString path) {
    passExecutable = path;
}

/**
 * @brief MainWindow::setGitExecutable
 * @param path
 */
void MainWindow::setGitExecutable(QString path) {
    gitExecutable = path;
}

/**
 * @brief MainWindow::setGpgExecutable
 * @param path
 */
void MainWindow::setGpgExecutable(QString path) {
    gpgExecutable = path;
}

/**
 * @brief MainWindow::on_configButton_clicked
 */
void MainWindow::on_configButton_clicked()
{
    config();
}
