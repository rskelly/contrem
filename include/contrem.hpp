/*
 * processor.hpp
 *
 *  Created on: May 9, 2018
 *      Author: rob
 */

#ifndef _CONTREM_HPP_
#define _CONTREM_HPP_

#include <list>
#include <vector>

#include "util.hpp"
#include "reader.hpp"
#include "plotter.hpp"

using namespace geo::util;
using namespace hlrg::reader;
using namespace hlrg::plot;

namespace hlrg {
namespace contrem {

/**
 * Forward declaration.
 */
class Contrem;

/**
 * An interface that provides implementors with the ability to receive
 * notifications from the Contrem.
 */
class ContremListener {
public:

	/**
	 * Called when the convolution has started.
	 *
	 * \param A Contrem.
	 */
	virtual void started(Contrem*) = 0;

	/**
	 * Called when the convolution status has updated.
	 *
	 * \param A Contrem.
	 */
	virtual void update(Contrem*) = 0;

	/**
	 * Called when the convolution has stopped.
	 *
	 * \param A Contrem.
	 */
	virtual void stopped(Contrem*) = 0;

	/**
	 * Called when the convolution has finished.
	 *
	 * \param A Contrem.
	 */
	virtual void finished(Contrem*) = 0;

	virtual ~ContremListener() {}
};


/**
 * Performs the continuum removal process.
 */
class Contrem {
private:
	ContremListener* m_listener;
	int m_step;
	int m_steps;

public:
	std::string output;						///<! The output file.
	FileType outputType;					///<! The output file type.
	std::string extension;					///<! The output extension.
	std::string roi;						///<! The mask/ROI; Raster format.
	std::string spectra;					///<! The input spectra; raster or CSV.
	FileType spectraType;					///<! The input spectra file type.
	std::string samplePoints;				///<! The sample points file for plotting.
	std::string samplePointsLayer;			///<! The sample points layer for plotting.
	std::string samplePointsIDField;		///<! A field to identify the sample point. Optional.
	double minWl;							///<! The lower bound of the wavelength range to process.
	double maxWl;							///<! The upper bound of the wavelength range to process.
	int wlMinCol;							///<! The first column in the dataset which is a wavelength.
	int wlMaxCol;							///<! The last column in the dataset which is a wavelength.
	int wlHeaderRows;						///<! The number of rows used as a header.
	bool wlTranspose;						///<! True, if the dataset should be transposed.
	int wlIDCol;							///<! A column used as an identifier (a string).
	bool plotOrig;							///<! Plot the original spectrum and hull.
	bool plotNorm;							///<! Plot the normalized continuum removed spectrum and regression line.
	NormMethod normMethod;					///<! The normalization method.
	int threads;							///<! The number of threads to use.
	bool running;							///<! True if the process is running. Setting this to false causes shutdown.

	GDALReader* grdr;						///<! A pointer to the reader if it was a raster reader;

	/**
	 * Process the continuum removal job.
	 *
	 * \param listener An implementation of ContremListener that can receive status events.
	 * \param config A configuration object.
	 */
	void run(ContremListener* listener);

	/**
	 * Return a reference to the plotter. TODO: This is a hack.
	 */
	Plotter& plotter();

	/**
	 * Initialize the number of steps to completion and the first step position.
	 */
	void initSteps(int step, int steps);

	/**
	 * Advance to the next progress step.
	 */
	void nextStep();

	/**
	 * Returns the current processing progress as a double from 0 to 1.
	 *
	 * \return The current processing progress as a double from 0 to 1.
	 */
	double progress() const;

};

} // contrem
} // hlrg

#endif /* _CONTREM_HPP_ */
