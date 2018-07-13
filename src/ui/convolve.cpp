/*
 * convolve.cpp
 *
 *  Created on: Jul 12, 2018
 *      Author: rob
 */

#include <QtWidgets/QDialog>
#include <QtCore/QString>
#include <QtWidgets/QFileDialog>
#include <QtCore/QDir>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QMessageBox>

#include "convolve.hpp"

ConvolveForm::ConvolveForm(Convolver* convolver, QApplication* app) :
	m_convolver(convolver),
	m_form(nullptr),
	m_app(app),
	m_running(false) {
}

void ConvolveForm::setupUi(QDialog* form) {
	Ui::ConvolveForm::setupUi(form);
	m_form = form;
	progressBar->setValue(0);
	connect(txtBandDef, SIGNAL(textChanged(QString)), this, SLOT(txtBandDefChanged(QString)));
	connect(txtSpectra, SIGNAL(textChanged(QString)), this, SLOT(txtSpectraChanged(QString)));
	connect(txtOutput, SIGNAL(textChanged(QString)), this, SLOT(txtOutputChanged(QString)));
	connect(btnBandDef, SIGNAL(clicked()), this, SLOT(btnBandDefClicked()));
	connect(btnSpectra, SIGNAL(clicked()), this, SLOT(btnSpectraClicked()));
	connect(btnOutput, SIGNAL(clicked()), this, SLOT(btnOutputClicked()));
	connect(btnRun, SIGNAL(clicked()), this, SLOT(btnRunClicked()));
	connect(btnCancel, SIGNAL(clicked()), this, SLOT(btnCancelClicked()));
	connect(btnHelp, SIGNAL(clicked()), this, SLOT(btnHelpClicked()));
	connect(btnClose, SIGNAL(clicked()), this, SLOT(btnCloseClicked()));

	connect(this, SIGNAL(started(Convolver*)), this, SLOT(convStarted(Convolver*)));
	connect(this, SIGNAL(stopped(Convolver*)), this, SLOT(convStopped(Convolver*)));
	connect(this, SIGNAL(update(Convolver*)), this, SLOT(convUpdate(Convolver*)));
	connect(this, SIGNAL(finished(Convolver*)), this, SLOT(convFinished(Convolver*)));

	txtBandDef->setText(m_settings.value("lastBandDef", "").toString());
	txtSpectra->setText(m_settings.value("lastSpectra", "").toString());
	txtOutput->setText(m_settings.value("lastOutput", "").toString());
}

void ConvolveForm::checkRun() {
	bool a = !m_bandDefFile.empty() && QFile(m_bandDefFile.c_str()).exists();
	bool b = !m_spectraFile.empty() && QFile(m_spectraFile.c_str()).exists();
	QFileInfo dir(m_outputFile.c_str());
	bool c = !m_outputFile.empty() && dir.dir().exists();
	btnRun->setEnabled(a && b && c);
}

void ConvolveForm::txtBandDefChanged(QString filename) {
	m_bandDefFile = filename.toStdString();
	checkRun();
}

void ConvolveForm::txtSpectraChanged(QString filename) {
	m_spectraFile = filename.toStdString();
	checkRun();
}

void ConvolveForm::txtOutputChanged(QString filename) {
	m_outputFile = filename.toStdString();
	checkRun();
}

void ConvolveForm::btnBandDefClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getOpenFileName(this, "Band Definition File", lastDir);
	QFileInfo dir(filename);
	m_settings.setValue("lastDir", dir.dir().absolutePath());
	m_settings.setValue("lastBandDef", filename);
	txtBandDef->setText(filename);
}

void ConvolveForm::btnSpectraClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getOpenFileName(this, "Spectra File", lastDir);
	QFileInfo dir(filename);
	m_settings.setValue("lastDir", dir.dir().absolutePath());
	m_settings.setValue("lastSpectra", filename);
	txtSpectra->setText(filename);
}

void ConvolveForm::btnOutputClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getSaveFileName(this, "Output File", lastDir);
	QFileInfo dir(filename);
	m_settings.setValue("lastDir", dir.dir().absolutePath());
	m_settings.setValue("lastOutput", filename);
	txtOutput->setText(filename);
}

void _run(ConvolveForm* form, Convolver* conv, const std::string* bandDef, const std::string* spectra, const std::string* output, bool* running) {
	conv->run(*form, *bandDef, *spectra, *output, *running);
}

void ConvolveForm::runState() {
	btnRun->setEnabled(false);
	btnCancel->setEnabled(true);
	txtBandDef->setEnabled(false);
	btnBandDef->setEnabled(false);
	txtSpectra->setEnabled(false);
	btnSpectra->setEnabled(false);
	txtOutput->setEnabled(false);
	btnOutput->setEnabled(false);
}

void ConvolveForm::stopState() {
	btnRun->setEnabled(true);
	btnCancel->setEnabled(false);
	txtBandDef->setEnabled(true);
	btnBandDef->setEnabled(true);
	txtSpectra->setEnabled(true);
	btnSpectra->setEnabled(true);
	txtOutput->setEnabled(true);
	btnOutput->setEnabled(true);
}

void ConvolveForm::run() {
	runState();
	if(!m_running) {
		m_running = true;
		m_thread = std::thread(_run, this, m_convolver, &m_bandDefFile, &m_spectraFile, &m_outputFile, &m_running);
	}
	if(!m_thread.joinable())
		stopState();
}

void ConvolveForm::cancel() {
	if(m_running) {
		m_running = false;
		if(m_thread.joinable())
			m_thread.join();
	}
	stopState();
}

void ConvolveForm::btnRunClicked() {
	run();
}

void ConvolveForm::btnCancelClicked() {
	cancel();
}

void ConvolveForm::btnHelpClicked() {
	QDesktopServices::openUrl(QUrl("https://github.com/rskelly/contrem/wiki", QUrl::TolerantMode));
}

void ConvolveForm::btnCloseClicked() {
	cancel();
	m_form->close();
	m_app->quit();
}

void ConvolveForm::convStarted(Convolver*) {
	progressBar->setValue(0);
}

void ConvolveForm::convUpdate(Convolver* conv) {
	progressBar->setValue(conv->progress() * 100);
}

void ConvolveForm::convStopped(Convolver*) {
	progressBar->setValue(0);
	stopState();
	checkRun();
}

void ConvolveForm::convFinished(Convolver*) {
	QMessageBox::information(this, "Finished", "Processing is finished.");
}
