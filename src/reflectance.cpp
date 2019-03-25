/*
 * nano_apx_timesync.cpp
 *
 *  Created on: Sep 17, 2018
 *      Author: rob
 */

#include "../include/reflectance.hpp"

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>

#include <QtCore/QObject>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

#include <unistd.h>
#include <dirent.h>

#include "reader.hpp"
#include "raster.hpp"
#include "ui/reflectance_ui.hpp"


namespace hlrg {

class DummyListener : public ReflectanceListener {
private:
	int lastP;
public:
	void started(Reflectance* r) {
		lastP = -1;
		std::cout << "Running ";
	}

	void update(Reflectance* r) {
		int p = (int) (r->progress() * 100);
		if(p != lastP) {
			if(p % 25 == 0)
				std::cout << " " << p << "% ";
			if(p % 10 == 0)
				std::cout << ".";
			lastP = p;
		}
	}

	void stopped(Reflectance* r) {
		std::cout << " Stopped.\n";
	}

	void finished(Reflectance* r) {
		std::cout << " Done.\n";
	}

	void exception(Reflectance* r, const std::exception& ex) {
		std::cerr << ex.what() << "\n";
	}

	~DummyListener() {}
};

/**
 * Average the values in the given vector.
 */
double _avg(std::vector<uint16_t>& buf) {
	uint32_t sum = 0;
	for(uint16_t v : buf)
		sum += v;
	return (double) sum / buf.size();
}

/**
 * Average the values in the given vector.
 */
double _avg(std::vector<double>& buf) {
	double sum = 0;
	for(double v : buf)
		sum += v;
	return (double) sum / buf.size();
}

std::string _ts2str(long ts) {
	ts /= 1000;
	char buf[32];
	struct tm* dt = localtime(&ts);
	strftime(buf, 32, "%Y/%m/%d %H:%M:%S", dt);
	std::stringstream ss;
	ss << buf << "." << ts % 1000;
	return ss.str();
}

/**
 * Return a vector of the files associated with the given path and pattern. If the
 * path is itself a file, return a vector with it as the single element.
 */
std::vector<std::string> _getFiles(const std::string& path, const std::string& pat) {
	std::vector<std::string> result;
	struct stat s;
	if(stat(path.c_str(), &s)) {
		throw std::runtime_error("Failed to read file/directory: " + path);
	} else {
		if(S_ISREG(s.st_mode)) {			// Is a file
			result.push_back(path);
		} else if(S_ISDIR(s.st_mode)) {		// Is a dir.
			DIR* d = opendir(path.c_str());
			struct dirent* de;
			while((de = readdir(d)) != nullptr) {
				if(de->d_type == DT_REG) {
					std::string fname(de->d_name);
					if(fname.find(pat) != std::string::npos)
						result.push_back(fname);
				}
			}
			closedir(d);
		}
	}
	return result;
}

void Reflectance::run(ReflectanceListener& listener,
		const std::string& imuGps, double imuUTCOffset,
		const std::string& rawRad,
		const std::string& rawPat,
		const std::string& frameIdx,
		const std::string& framePat,
		const std::string& irradConv, const std::string& irradPat, double irradUTCOffset,
		const std::string& reflOut, const std::string& reflPat,
		bool& running) {

	// Proposed algorithm: Since the flame is the largest dataset, we iterate over the rows, using the frame index
	// from the nano to locate the row offset for the corresponding time. Probably should use a b-tree for searching. -- done

	// The frames won't be found in consecutive order because the flame's integration time is lower than the nano's
	// frame period. Get two consecutive frames and attibute the first time to the first half of the intermediate
	// frames and the second time to the others.

	listener.started(this);

	m_step = 0;
	m_numSteps = 4;
	listener.update(this);

	// Set up data readers and writers.
	FrameIndexReader fi(frameIdx);

	if(!running) {
		listener.stopped(this);
		return;
	}

	IMUGPSReader ir(imuGps, imuUTCOffset * 3600000);

	if(!running) {
		listener.stopped(this);
		return;
	}

	FlameReader fr(irradConv, irradUTCOffset * 3600000);

	if(!running) {
		listener.stopped(this);
		return;
	}

	++m_step;
	listener.update(this);

	std::vector<std::string> rawRadFiles = _getFiles(rawRad, rawPat);

	// Set up rasters.
	Raster raster(rawRad);
	Raster output(reflOut, raster.cols(), raster.rows(), raster.bands(), 0, Float32, &raster);

	// Add rows to the number of steps.
	m_numSteps += raster.rows();
	++m_step;
	listener.update(this);

	if(!running) {
		listener.stopped(this);
		return;
	}

	++m_step;
	listener.update(this);

	// Initialize variables.
	FlameRow frow0, frow1;
	long gpsTime, actualGpsTime0 = 0, actualGpsTime1 = 0;
	int frame0 = 0, frame1 = 0;
	std::vector<float> buffer;
	std::vector<float> refl(raster.cols() * raster.bands());

	// Get the first frame index.
	int firstIdx;
	fi.getNearestFrame(0, actualGpsTime0, firstIdx);

	if(!running) {
		listener.stopped(this);
		return;
	}

	++m_step;
	listener.update(this);

	// Set up a writer to log the output for checking.
	//std::ofstream out("./tmp2.out", std::ios::out);

	// Get the first row from the flame data.
	if(running && fr.next(frow0)) {

		// Get the nearest frame and times for the flame's time.
		ir.getGPSTime(frow0.utcTime, gpsTime);
		fi.getNearestFrame(gpsTime, actualGpsTime0, frame0);

		if(!running) {
			listener.stopped(this);
			return;
		}

		// Get the next flame row.
		while(fr.next(frow1)) {

			if(!running) {
				listener.stopped(this);
				return;
			}

			// Get the nearest frame and times for the flame's time.
			ir.getGPSTime(frow1.utcTime, gpsTime);
			fi.getNearestFrame(gpsTime, actualGpsTime1, frame1);

			// If the frame has advanced...
			if(running && frame1 > frame0) {

				// Compute the number of frames that blong to the first time, vs. the second time.
				int half = frame0 + (frame1 - frame0) / 2;

				// Iterate over the rows between the times.
				for(int row = frame0; running && row < frame1; ++row) {

					// Get the flame row for this raster row.
					FlameRow& frow = row < half ? frow0 : frow1;

					// Read the pixels.
					raster.get(buffer, row - firstIdx);

					// Print a selection of bands from the middle of the row.
					//out << actualGpsTime1 << "," << buffer[bufCol] << "," << frow.bands[flameCol] << ", " << _ts2str(actualGpsTime1) << "\n";

					// For every band and every cell int he buffer, compute the reflectance using the irradiance values.
					for(int b = 0; running && b < raster.bands(); ++b) {
						for(int c = 0; running && c < raster.cols(); ++c) {
							size_t i = b * raster.cols() + c;
							refl[i] = buffer[i] / frow.bands[b];
						}
					}

					// Write to the new raster
					output.write(refl, row - firstIdx);

					++m_step;
					listener.update(this);
				}
			}

			// Save the frame/time/etc., before advancing.
			frame0 = frame1;
			actualGpsTime0 = actualGpsTime1;
			frow0 = frow1;
		}
	}

	if(!running) {
		listener.stopped(this);
		return;
	}

	++m_step;
	listener.update(this);

	listener.finished(this);
}

double Reflectance::progress() const {
	return std::max(std::min((double) m_step / m_numSteps, 1.0), 0.0);
}

int runWithGui(int argc, char **argv) {
	class ReflectanceApp : public QApplication {
	public:
		ReflectanceApp(int& argc, char** argv) : QApplication(argc, argv) {}
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

	ReflectanceApp q(argc, argv);
	Reflectance nt;
	ReflectanceForm form(&nt, &q);
	QDialog qform;
	form.setupUi(&qform);
	qform.show();
	return q.exec();
}

} // hlrg


void usage() {
	std::cout << "Usage: reflectance [<options>]\n"
			<< " -i 	The IMUGPS file.\n"
			<< " -io 	Time offset to convert IMUGPS time to UTC. (Default 0).\n"
			<< " -r 	The raw radiance file (raster).\n"
			<< " -f 	The frame index file.\n"
			<< " -c		Convolved irradiance file.\n"
			<< " -co	Time offset to convert convolved time to UTC. (Default 0).\n"
			<< " -o 	Reflectance output file.\n";
}

int main(int argc, char** argv) {

	if(argc <= 1) {

		hlrg::runWithGui(argc, argv);

	} else {

		double imuUTCOffset = 0;
		double irradUTCOffset = 0;
		std::string imuGps;
		std::string rawRad;
		std::string frameIdx;
		std::string irradConv;
		std::string reflOut;

		for(int i = 1; i < argc; ++i) {
			std::string arg(argv[i]);
			if(arg == "-i") {
				imuGps = argv[++i];
			} else if(arg == "-io") {
				imuUTCOffset = atof(argv[++i]);
			} else if(arg == "-r") {
				rawRad = argv[++i];
			} else if(arg == "-f") {
				frameIdx = argv[++i];
			} else if(arg == "-c") {
				irradConv = argv[++i];
			} else if(arg == "-o") {
				reflOut = argv[++i];
			} else if(arg == "-co") {
				irradUTCOffset = atof(argv[++i]);
			}
		}

		if(imuGps.empty()) {
			std::cerr << "IMUGPS file is required.\n";
			usage();
			return 1;
		}
		if(rawRad.empty()) {
			std::cerr << "Radiance file is required.\n";
			usage();
			return 1;
		}
		if(frameIdx.empty()) {
			std::cerr << "Frame index is required.\n";
			usage();
			return 1;
		}
		if(irradConv.empty()) {
			std::cerr << "Convolved irradiance is required.\n";
			usage();
			return 1;
		}
		if(reflOut.empty()) {
			std::cerr << "Reflectance output file is required.\n";
			usage();
			return 1;
		}

		bool running = true;
		hlrg::Reflectance refl;
		hlrg::DummyListener listener;
		refl.run(listener, imuGps, imuUTCOffset, rawRad, frameIdx, irradConv, irradUTCOffset, reflOut, running);

	}

}
