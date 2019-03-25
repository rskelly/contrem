/*
 * nano_timesync_ui.cpp
 *
 *  Created on: Sep 24, 2018
 *      Author: rob
 */

#include <fstream>

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
	connect(btnFrameIndexFile, SIGNAL(clicked()), this, SLOT(btnFrameIndexFileClicked()));
	connect(btnFrameIndexDir, SIGNAL(clicked()), this, SLOT(btnFrameIndexDirClicked()));
	connect(txtFrameIndex, SIGNAL(textChanged(QString)), this, SLOT(txtFrameIndexChanged(QString)));
	connect(btnRadRastFile, SIGNAL(clicked()), this, SLOT(btnRadRastFileClicked()));
	connect(btnRadRastDir, SIGNAL(clicked()), this, SLOT(btnRadRastDirClicked()));
	connect(txtRadRast, SIGNAL(textChanged(QString)), this, SLOT(txtRadRastChanged(QString)));
	connect(btnConvIrradFile, SIGNAL(clicked()), this, SLOT(btnConvIrradFileClicked()));
	connect(btnConvIrradDir, SIGNAL(clicked()), this, SLOT(btnConvIrradDirClicked()));
	connect(txtConvIrrad, SIGNAL(textChanged(QString)), this, SLOT(txtConvIrradChanged(QString)));
	connect(spnIrradUTCOffset, SIGNAL(valueChanged(double)), this, SLOT(spnIrradUTCOffsetChanged(double)));
	connect(btnReflOutputFile, SIGNAL(clicked()), this, SLOT(btnReflOutputFileClicked()));
	connect(btnReflOutputDir, SIGNAL(clicked()), this, SLOT(btnReflOutputDirClicked()));
	connect(txtReflOutput, SIGNAL(textChanged(QString)), this, SLOT(txtReflOutputChanged(QString)));
	connect(btnRun, SIGNAL(clicked()), this, SLOT(btnRunClicked()));
	connect(btnCancel, SIGNAL(clicked()), this, SLOT(btnCancelClicked()));
	connect(btnHelp, SIGNAL(clicked()), this, SLOT(btnHelpClicked()));
	connect(btnClose, SIGNAL(clicked()), this, SLOT(btnCloseClicked()));

	connect(this, SIGNAL(started(Reflectance*)), this, SLOT(reflStarted(Reflectance*)));
	connect(this, SIGNAL(stopped(Reflectance*)), this, SLOT(reflStopped(Reflectance*)));
	connect(this, SIGNAL(update(Reflectance*)), this, SLOT(reflUpdate(Reflectance*)));
	connect(this, SIGNAL(finished(Reflectance*)), this, SLOT(reflFinished(Reflectance*)));
	connect(this, SIGNAL(exception(Reflectance*, const std::exception&)), this, SLOT(reflException(Reflectance*, const std::exception&)));

	txtIMUGPS->setText(m_settings.value("lastIMUGPS", "").toString());
	spnIMUUTCOffset->setValue(m_settings.value("lastIMUUTCOffset", 0).toDouble());
	txtFrameIndex->setText(m_settings.value("lastFrameIndex", "").toString());
	txtRadRast->setText(m_settings.value("lastRadRast", "").toString());
	txtConvIrrad->setText(m_settings.value("lastIrradConv", "").toString());
	spnIrradUTCOffset->setValue(m_settings.value("lastIrradUTCOffset", 0).toDouble());
	txtReflOutput->setText(m_settings.value("lastReflOut", "").toString());

	checkRun();
}

bool _fexists(const std::string& filename) {
	if(filename.empty())
		return false;
	std::ifstream f(filename);
	return f.good();
}

void _popup(const std::string& title, const std::string& message) {

}

void ReflectanceForm::checkRun() {
	bool a = _fexists(m_imuGps);
	bool b = _fexists(m_frameIdx);
	bool c = _fexists(m_rawRad);
	bool d = _fexists(m_irradConv);
	btnRun->setEnabled(a && b && c && d);
}

void _process(ReflectanceListener* listener,
		const std::string* imuGps, double imuUTCOffset,
		const std::string* rawRad,
		const std::string* frameIdx,
		const std::string* irradConv, double irradUTCOffset,
		const std::string* reflOut,
		bool* running) {

	Reflectance refl;
	try {
		refl.run(*listener, *imuGps, imuUTCOffset, *rawRad, *frameIdx, *irradConv, irradUTCOffset, *reflOut, *running);
	} catch(const std::exception& ex) {
		listener->exception(&refl, ex);
	}
}

void ReflectanceForm::run() {
	if(!m_running) {
		m_running = true;
		runState();
		if(m_thread.joinable())
			m_thread.join();
		m_thread = std::thread(_process, static_cast<ReflectanceListener*>(this),
				&m_imuGps, m_imuUTCOffset,
				&m_rawRad,
				&m_frameIdx,
				&m_irradConv, m_irradUTCOffset,
				&m_reflOut, &m_running);
	}
}

bool _failure = false;

void ReflectanceForm::cancel() {
	if(m_running) {
		m_running = false;
		if(_failure) {
			m_thread.detach();
			_failure = false;
		} else {
			if(m_thread.joinable())
				m_thread.join();
		}
	}
}

void _enable(std::vector<QWidget*>& widgets, bool enable) {
	for(QWidget* w : widgets)
		w->setEnabled(enable);
}

void ReflectanceForm::runState() {
	btnRun->setEnabled(false);
	btnCancel->setEnabled(true);
	std::vector<QWidget*> widgets = {btnIMUGPS, txtIMUGPS, spnIMUUTCOffset, btnFrameIndex,
			txtFrameIndex, btnRadRast, txtRadRast, btnConvIrrad,
			txtConvIrrad, spnIrradUTCOffset, btnReflOutput, txtReflOutput};
	_enable(widgets, false);

}

void ReflectanceForm::stopState() {
	btnRun->setEnabled(true);
	btnCancel->setEnabled(false);
	std::vector<QWidget*> widgets = {btnIMUGPS, txtIMUGPS, spnIMUUTCOffset, btnFrameIndex,
			txtFrameIndex, btnRadRast, txtRadRast, btnConvIrrad,
			txtConvIrrad, spnIrradUTCOffset, btnReflOutput, txtReflOutput};
	_enable(widgets, true);
}

void ReflectanceForm::reflStarted(Reflectance* refl) {
	runState();
	progressBar->setValue(0);
}

void ReflectanceForm::reflUpdate(Reflectance* refl) {
	runState();
	int p = (int) (refl->progress() * 100.0);
	progressBar->setValue(p);
}

void ReflectanceForm::reflStopped(Reflectance* refl) {
	m_running = false;
	if(m_thread.joinable())
		m_thread.detach();
	stopState();
}

void ReflectanceForm::reflFinished(Reflectance* refl) {
	m_running = false;
	if(m_thread.joinable())
		m_thread.detach();
	stopState();
}

void ReflectanceForm::reflException(Reflectance* refl, const std::exception& ex) {
	progressBar->setValue(0);
	QMessageBox::critical(this, "Error", ex.what());
	_failure = true;
	cancel();
}

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

void ReflectanceForm::btnFrameIndexFileClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getOpenFileName(this, "Frame Index File", lastDir);
	QFileInfo dir(filename);
	m_settings.setValue("lastDir", dir.dir().absolutePath());
	txtFrameIndex->setText(filename);
}

void ReflectanceForm::btnFrameIndexDirClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getExistingDirectory(this, "Frame Index Folder", lastDir);
	QFileInfo dir(filename);
	m_settings.setValue("lastDir", dir.dir().absolutePath());
	txtFrameIndex->setText(filename);
}

void ReflectanceForm::txtRadRastChanged(QString v) {
	m_rawRad = v.toStdString();
	m_settings.setValue("lastRadRast", v);
	checkRun();
}

void ReflectanceForm::btnRadRastFileClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getOpenFileName(this, "Raw Radiance File", lastDir);
	QFileInfo dir(filename);
	m_settings.setValue("lastDir", dir.dir().absolutePath());
	txtRadRast->setText(filename);
}

void ReflectanceForm::btnRadRastDirClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getExistingDirectory(this, "Raw Radiance Folder", lastDir);
	QFileInfo dir(filename);
	m_settings.setValue("lastDir", dir.dir().absolutePath());
	txtRadRast->setText(filename);
}

void ReflectanceForm::txtConvIrradChanged(QString v) {
	m_irradConv = v.toStdString();
	m_settings.setValue("lastIrradConv", v);
	checkRun();
}

void ReflectanceForm::btnConvIrradFileClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getOpenFileName(this, "Convolved Irradiance File", lastDir);
	QFileInfo dir(filename);
	m_settings.setValue("lastDir", dir.dir().absolutePath());
	txtConvIrrad->setText(filename);
}

void ReflectanceForm::btnConvIrradDirClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getExistingDirectory(this, "Convolved Irradiance Folder", lastDir);
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

void ReflectanceForm::btnReflOutputFileClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getOpenFileName(this, "Reflectance Output File", lastDir);
	QFileInfo dir(filename);
	m_settings.setValue("lastDir", dir.dir().absolutePath());
	txtReflOutput->setText(filename);
}

void ReflectanceForm::btnReflOutputDirClicked() {
	QString lastDir = m_settings.value("lastDir", "").toString();
	QString filename = QFileDialog::getExistingDirectory(this, "Reflectance Output Folder", lastDir);
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

ReflectanceForm::~ReflectanceForm() {
	if(m_thread.joinable())
		m_thread.detach();
}


