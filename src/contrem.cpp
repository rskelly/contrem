//============================================================================
// Name        : contrem.cpp
// Author      : Rob Skelly
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <unistd.h>

#include "reader.hpp"
#include "writer.hpp"
#include "processor.hpp"

void usage() {
	std::cerr << "Usage: contrem [options]\n"
			<< " -d A GDAL-readable data file containing spectral samples; can contain any number of bands >= 2.\n"
			<< " -r An ENVI ROI text file.\n"
			<< " -b A CSV file containing a mapping from wavelength to (1-based) band index.\n"
			<< " -w An integer giving the (0-based) column index in -b which contains wavelengths.\n"
			<< " -i An integer giving the (0-based) column index in -b which contains the band indices.\n"
			<< " -z If given, indicates the presence of a header in the band map that must be skipped.\n"
			<< " -o An output file template. This is a filename with no extension that will be modified as\n"
			<< "    appropriate. Parent directories will be created.\n"
			<< " -l The minimum wavelength to consider.\n"
			<< " -h The maximum wavelength to consider.\n"
			<< " -s The size of the buffer. Default is 256. Larger buffers are possible, but one must\n"
			<< "    consider that multiple buffers may be in memory at once.\n"
			<< " -t The number of threads to use. Default 2.\n"
			<< " -p By default, sample statistics are used. This flag forces the use of\n"
			<< "    population statistics.\n"
			<< " -v The driver to use for output rasters. Defaults to GTiff, but any GDAL-writable\n"
			<< "    format will do.\n";
}

int main(int argc, char** argv) {

	int bufSize = 256;
	double minWl = 0;
	double maxWl = 0;
	std::string datafile;
	std::string roifile;
	std::string bandfile;
	int wlCol = -1;
	int bandCol = -1;
	bool bandHeader = false;
	std::string outfile;
	int threads = 1;
	bool sample = true;
	std::string outDriver = "GTiff";

	try {
		int c;
		while((c = getopt(argc, argv, "d:r:b:o:l:h:s:w:i:t:p")) != -1) {
			switch(c) {
			case 'd': datafile = optarg; break;
			case 'r': roifile = optarg; break;
			case 'b': bandfile = optarg; break;
			case 'o': outfile = optarg; break;
			case 'l': minWl = atof(optarg); break;
			case 'h': maxWl = atof(optarg); break;
			case 's': bufSize = atoi(optarg); break;
			case 'w': wlCol = atoi(optarg); break;
			case 'i': bandCol = atoi(optarg); break;
			case 't': threads = atoi(optarg); break;
			case 'p': sample = false; break;
			case 'z': bandHeader = true; break;
			case 'v': outDriver = optarg; break;
			default: break;
			}
		}

		if(bufSize <= 0)
			throw std::invalid_argument("Buffer size must be larger than zero.");
		if(datafile.empty() && roifile.empty())
			throw std::invalid_argument("Data  or ROI file not given.");
		if(outfile.empty())
			throw std::invalid_argument("Output file template not given.");
		if(threads < 1)
			throw std::invalid_argument("At least one thread is required.");
		if(!bandfile.empty() && (bandCol < 0 || wlCol < 0))
			throw std::invalid_argument("If the band file is given, wavelength and band columns must also be given.");

		Reader* reader;
		if(!roifile.empty()) {
			reader = new ROIReader(roifile);
		} else if(!datafile.empty()) {
			reader = new GDALReader(datafile);
		} else {
			throw std::invalid_argument("No input file (-r or -d) given.");
		}

		if(!bandfile.empty()) {
			if(wlCol == -1 || bandCol == -1 || wlCol == bandCol)
				throw std::invalid_argument("If a band file is given, the indices for "
						"wavelength and band must be >=0 and different from each other.");
			BandMapReader br(bandfile, wlCol, bandCol, bandHeader);
			reader->setBandMap(br.bandMap());
		}

		if(minWl > 0 && maxWl > 0)
			reader->setBandRange(minWl, maxWl);

		reader->setBufSize(bufSize);

		Processor processor;

		processor.process(reader, outfile, outDriver, bufSize, threads, sample);

	} catch(const std::exception& ex) {
		std::cerr << ex.what() << "\n";
		usage();
		return 1;
	}

	return 0;
}
