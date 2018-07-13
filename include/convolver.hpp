/*
 * convolver.hpp
 *
 *  Created on: Jul 12, 2018
 *      Author: rob
 */

#ifndef INCLUDE_CONVOLVER_HPP_
#define INCLUDE_CONVOLVER_HPP_

#include <string>

class Convolver;

class ConvolverListener {
public:
	virtual void started(Convolver*) = 0;
	virtual void update(Convolver*) = 0;
	virtual void stopped(Convolver*) = 0;
	virtual void finished(Convolver*) = 0;
	virtual ~ConvolverListener() {}
};

class Convolver {
private:
	double m_progress;

public:
	/**
	 * Run the convolver on the given files. The listener will receive callbacks.
	 *
	 * @param listener A ConvolverListener to receive updates.
	 * @param bandDef The band definition file.
	 * @param spectra The spectral data file.
	 * @param output The output file.
	 * @param running A reference to a boolean that is true so long as the processor should keep running.
	 */
	void run(ConvolverListener& listener,
			const std::string& bandDef, const std::string& spectra, const std::string& output,
			bool& running);

	/**
	 * Cancel the run.
	 */
	void cancel();

	/**
	 * Return the progress as a double between 0 and 1.
	 */
	double progress() const;
};



#endif /* INCLUDE_CONVOLVER_HPP_ */
