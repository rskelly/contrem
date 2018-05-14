/*
 * writer.cpp
 *
 *  Created on: May 9, 2018
 *      Author: rob
 */

#include <gdal_priv.h>
#include <errno.h>
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <cstring>

#include "writer.hpp"

int makedir(const std::string& filename) {
	std::string path = filename.substr(0, filename.find_last_of('/'));
	if(mkdir(path.c_str(), 0755) == 0)
		return true;
	switch(errno) {
	case EEXIST: return 0;
	default: return errno;
	}
}

GDALWriter::GDALWriter(const std::string& filename, const std::string& driver, int cols, int rows, int bands,
		const std::string& fieldName, const std::vector<std::string>& bandNames) :
	m_ds(nullptr),
	m_bands(0), m_cols(0), m_rows(0) {

	int err;
	if((err = makedir(filename)))
		throw std::runtime_error("Could not create directory for " + filename + "; " + std::to_string(err));

	GDALAllRegister();
	GDALDriverManager* gm = GetGDALDriverManager();
	GDALDriver* drv = gm->GetDriverByName(driver.c_str());
	if(!drv)
		throw std::runtime_error("Driver not found: " + driver);
	m_ds = drv->Create(filename.c_str(), cols, rows, bands, GDT_Float32, nullptr);
	m_bands = m_ds->GetRasterCount();
	m_cols = m_ds->GetRasterXSize();
	m_rows = m_ds->GetRasterYSize();

	if(!bandNames.empty()) {
		for(int i = 1; i <= std::min((int) bandNames.size(), m_bands); ++i)
			m_ds->GetRasterBand(i)->SetMetadataItem(fieldName.c_str(), bandNames[i - 1].c_str());
	}
}

bool GDALWriter::write(std::vector<double>& buf, int col, int row, int cols, int rows, int bufSize) {
	if(col < 0 || col >= m_cols || col + cols > m_cols
			|| row < 0 || row >= m_rows || row + rows > m_rows)
		return false;
	double* data = buf.data();
	for(int i = 1; i <= m_bands; ++i) {
		GDALRasterBand* band = m_ds->GetRasterBand(i);
		if(band->RasterIO(GF_Write, col, row, cols, rows,
				(void*) (data + (i - 1) * bufSize * bufSize),
				bufSize, bufSize, GDT_Float64, 0, 0, 0))
			return false;
	}
	return true;
}



#include "stats.hpp"

bool __isnonzero(const double& v) {
	return v > 0;
}

bool GDALWriter::writeStats(const std::string& filename, const std::vector<std::string>& names) {

	if(!names.empty() && (int) names.size() != m_bands)
		throw std::invalid_argument("Band names must be the same size as the number of bands, or empty.");

	Stats stats;

	std::vector<double> buf(m_cols * m_rows);
	std::vector<double> results(23);
	std::vector<std::string> statNames = stats.getStatNames();

	std::ofstream out(filename, std::ios::out);
	out << std::setprecision(12) << "name";
	for(const std::string& name : statNames)
		out << "," << name;
	out << "\n";

	m_ds->FlushCache();

	for(int i = 1; i <= m_bands; ++i) {

		// Read an entire band into the buffer.
		GDALRasterBand* band = m_ds->GetRasterBand(i);
		if(band->RasterIO(GF_Read, 0, 0, m_cols, m_rows, (void*) buf.data(), m_cols, m_rows, GDT_Float64, 0, 0, 0))
			return false;

		// Filter the values list to eliminate zeroes.
		std::vector<double> values;
		std::copy_if(buf.begin(), buf.end(), std::back_inserter(values), __isnonzero);

		if(!values.empty())
			stats.computeStats(values, results);

		out << names[i - 1];
		for(double v : results)
			out << "," << v;
		out << "\n";
	}
	return true;
}

GDALWriter::~GDALWriter() {
	GDALClose(m_ds);
}

