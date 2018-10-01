/*
 * convolve.cpp
 *
 *  Created on: Jul 10, 2018
 *      Author: rob
 */

#include <iostream>

#include <QtCore/QObject>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

#include "convolver.hpp"
#include "ui/convolve_ui.hpp"

using namespace hlrg;

int runWithGui(int argc, char **argv) {
	class ConvolveApp : public QApplication {
	public:
		ConvolveApp(int &argc, char **argv) : QApplication(argc, argv) {}
		bool notify(QObject *receiver, QEvent *e) {
			try {
				return QApplication::notify(receiver, e);
			} catch(const std::exception &ex) {
				QMessageBox err;
				err.setText("Error");
				err.setInformativeText(QString(ex.what()));
				err.exec();
				return false;
			}
		}
	};

	ConvolveApp q(argc, argv);
	Convolver conv;
	ConvolveForm form(&conv, &q);
	QDialog qform;
	form.setupUi(&qform);
	qform.show();
	return q.exec();
}

class DummyListener : public ConvolverListener {
public:
	void started(Convolver* conv) {
		std::cout << "Started\n";
	}
	void update(Convolver* conv) {
		std::cout << "Progress: " << (int) (conv->progress() * 100) << "%\n";
	}
	void stopped(Convolver* conv) {
		std::cout << "Stopped.\n";
	}
	void finished(Convolver* conv) {
		std::cout << "Finished.\n";
	}
};

void usage() {
	std::cerr << "Usage: convolve [<band definition file> <spectra file> <output file> [input scale] [threshold]]\n"
			<< "    Run without arguments to use the gui.\n";
}

#include "convolver.hpp"

int main(int argc, char** argv) {

	if(argc > 1) {
		if(argc != 4) {
			usage();
			return 1;
		} else {
			std::string bandDef = argv[1];
			std::string spectra = argv[2];
			std::string output = argv[3];
			double inputScale = 1.0;
			double threshold = 0.0001;
			double shift = 0;
			if(argc > 4)
				inputScale = std::strtod(argv[4], nullptr);
			if(argc > 5)
				threshold = std::strtod(argv[5], nullptr);

			Convolver conv;
			DummyListener listener;
			bool running = true;
			conv.run(listener, bandDef, ",", spectra, ",", 0, 0, -1, -1, output, ",", inputScale, threshold, shift, running);
		}
	} else {
		return runWithGui(argc, argv);
	}

}
