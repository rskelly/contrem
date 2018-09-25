/*
 * nano_timesync_ui.cpp
 *
 *  Created on: Sep 24, 2018
 *      Author: rob
 */

#include "reflectance_ui.hpp"

#include <QtWidgets/QDialog>
#include <QtCore/QString>
#include <QtWidgets/QFileDialog>
#include <QtCore/QDir>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QMessageBox>

#include "../../include/reflectance.hpp"
#include "reflectance_ui.hpp"

using namespace hlrg;

ReflectanceForm::ReflectanceForm(Reflectance* ts, QApplication* app) :
		m_imuUTCOffset(0),
		m_irradUTCOffset(0),
		m_ts(ts),
		m_form(nullptr),
		m_app(app),
		m_running(false) {

}

void ReflectanceForm::setupUi(QDialog* form) {
	Ui::ReflectanceForm::setupUi(form);
	m_form = form;
	progressBar->setValue(0);
	connect(btnIMUGPS, SIGNAL(clicked()), this, SLOT(btnIMUGPSClicked()));
	connect(txtIMUGPS, SIGNAL(textChanged(QString)), this, SLOT(txtIMUGPSChanged(QString)));
	connect(spnIMUUTCOffset, SIGNAL(valueChanged(double)), this, SLOT(spnIMUUTCOffsetChanged(double)));
	connect(btnFrameIndex, SIGNAL(clicked()), this, SLOT(btnFrameIndexClicked()));
	connect(txtFrameIndex, SIGNAL(textChanged(QString)), this, SLOT(txtFrameIndexChanged(QString)));
	connect(btnRadRast, SIGNAL(clicked()), this, SLOT(btnRadRastClicked()));
	connect(txtRadRast, SIGNAL(textChanged(QString)), this, SLOT(txtRadRastChanged(QString)));
	connect(btnConvIrrad, SIGNAL(clicked()), this, SLOT(btnConvIrradClicked()));
	connect(txtConvIrrad, SIGNAL(textChanged(QString)), this, SLOT(txtConvIrradChanged(QString)));
	connect(spnIrradUTCOffset, SIGNAL(valueChanged(double)), this, SLOT(spnIrradUTCOffsetChanged(double)));
	connect(btnReflOutput, SIGNAL(clicked()), this, SLOT(btnReflOutputClicked()));
	connect(txtReflOutput, SIGNAL(textChanged(QString)), this, SLOT(txtReflOutputChanged(QString)));
	connect(btnRun, SIGNAL(clicked()), this, SLOT(btnRunClicked()));
	connect(btnCancel, SIGNAL(clicked()), this, SLOT(btnCancelClicked()));
	connect(btnHelp, SIGNAL(clicked()), this, SLOT(btnHelpClicked()));
	connect(btnClose, SIGNAL(clicked()), this, SLOT(btnCloseClicked()));

	//connect(this, SIGNAL(started(Convolver*)), this, SLOT(convStarted(Convolver*)));
	//connect(this, SIGNAL(stopped(Convolver*)), this, SLOT(convStopped(Convolver*)));
	//connect(this, SIGNAL(update(Convolver*)), this, SLOT(convUpdate(Convolver*)));
	//connect(this, SIGNAL(finished(Convolver*)), this, SLOT(convFinished(Convolver*)));

	txtIMUGPS->setText(m_settings.value("lastIMUGPS", "").toString());
	spnIMUUTCOffset->setValue(m_settings.value("lastIMUUTCOffset", 0).toDouble());
	txtFrameIndex->setText(m_settings.value("lastFrameIndex", "").toString());
	txtRadRast->setText(m_settings.value("lastRadRast", "").toString());
	txtConvIrrad->setText(m_settings.value("lastIrradConv", "").toString());
	spnIrradUTCOffset->setValue(m_settings.value("lastIrradUTCOffset", 0).toDouble());
	txtReflOutput->setText(m_settings.value("lastReflOut", "").toString());
}

void ReflectanceForm::checkRun() {

}

void _process(Reflectance* ts, const std::string* imuGps, double imuUTCOffset,
		const std::string* rawRad,
		const std::string* frameIdx,
		const std::string* irradConv, double irradUTCOffset,
		const std::string* reflOut,
		bool* running) {

	ts->process(*imuGps, imuUTCOffset, *rawRad, *frameIdx, *irradConv, irradUTCOffset, *reflOut, running);
}

void ReflectanceForm::run() {
	if(!m_running) {
		m_running = true;
		m_thread = std::thread(_process, m_ts, &m_imuGps, m_imuUTCOffset, &m_rawRad, &m_frameIdx, &m_irradConv, m_irradUTCOffset, &m_reflOut, &m_running);
	}
}

void ReflectanceForm::cancel() {
	if(m_running) {
		m_running = false;
		if(m_thread.joinable())
			m_thread.join();
	}

}

void ReflectanceForm::runState() {

}

void ReflectanceForm::stopState() {

}

	//void started(Convolver*);
	//void update(Convolver*);
	//void stopped(Convolver*);
	//void finished(Convolver*);

void ReflectanceForm::txtIMUGPSChanged(QString v) {
	m_imuGps = v.toStdString();
	m_settings.setValue("lastIMUGPS", v);
	checkRun();
}

void ReflectanceForm::btnIMUGPSClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getOpenFileName(this, "IMU GPS File", lastDir);
	QFileInfo dir(filename);
	m_settings.setValue("lastDir", dir.dir().absolutePath());
	txtIMUGPS->setText(filename);
}

void ReflectanceForm::spnIMUUTCOffsetChanged(double v) {
	m_imuUTCOffset = v;
	m_settings.setValue("lastIMUUTCOffset", v);
	checkRun();
}

void ReflectanceForm::txtFrameIndexChanged(QString v) {
	m_frameIdx = v.toStdString();
	m_settings.setValue("lastFrameIndex", v);
	checkRun();
}

void ReflectanceForm::btnFrameIndexClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getOpenFileName(this, "Frame Index File", lastDir);
	QFileInfo dir(filename);
	m_settings.setValue("lastDir", dir.dir().absolutePath());
	txtFrameIndex->setText(filename);
}

void ReflectanceForm::txtRadRastChanged(QString v) {
	m_rawRad = v.toStdString();
	m_settings.setValue("lastRadRast", v);
	checkRun();
}

void ReflectanceForm::btnRadRastClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getOpenFileName(this, "Raw Radiance File", lastDir);
	QFileInfo dir(filename);
	m_settings.setValue("lastDir", dir.dir().absolutePath());
	txtRadRast->setText(filename);
}

void ReflectanceForm::txtConvIrradChanged(QString v) {
	m_irradConv = v.toStdString();
	m_settings.setValue("lastIrradConv", v);
	checkRun();
}

void ReflectanceForm::btnConvIrradClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getOpenFileName(this, "Convolved Irradiance File", lastDir);
	QFileInfo dir(filename);
	m_settings.setValue("lastDir", dir.dir().absolutePath());
	txtConvIrrad->setText(filename);
}

void ReflectanceForm::spnIrradUTCOffsetChanged(double v) {
	m_irradUTCOffset = v;
	m_settings.setValue("lastIrradUTCOffset", v);
	checkRun();
}

void ReflectanceForm::txtReflOutputChanged(QString v) {
	m_reflOut = v.toStdString();
	m_settings.setValue("lastReflOut", v);
	checkRun();
}

void ReflectanceForm::btnReflOutputClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getOpenFileName(this, "Reflectance output File", lastDir);
	QFileInfo dir(filename);
	m_settings.setValue("lastDir", dir.dir().absolutePath());
	txtReflOutput->setText(filename);
}

void ReflectanceForm::btnRunClicked() {
	run();
}

void ReflectanceForm::btnCancelClicked() {
	cancel();
}

void ReflectanceForm::btnHelpClicked() {
	QDesktopServices::openUrl(QUrl("https://github.com/rskelly/contrem/wiki/reflectance", QUrl::TolerantMode));
}

void ReflectanceForm::btnCloseClicked() {
	cancel();
	m_form->close();
	m_app->quit();
}


