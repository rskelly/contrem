/*
 * contrem_util.cpp
 *
 *  Created on: Jun 4, 2019
 *      Author: rob
 */

#include <algorithm>

#include <gdal_priv.h>

#include "reader.hpp"

#include "contrem.hpp"
#include "contrem_util.hpp"

using namespace hlrg;

FileType hlrg::getFileType(const std::string& filename) {
	std::string ext;
	{
		size_t p = filename.find('.');
		if(p < std::string::npos) {
			std::string ext0 = filename.substr(p);
			std::transform(ext0.begin(), ext0.end(), std::back_inserter(ext), ::tolower);
		}
	}
	if(ext == ".csv") {
		return CSV;
	} else if(ext == ".roi") {
		return ROI;
	} else {
		GDALAllRegister();
		GDALDataset* ds = static_cast<GDALDataset*>(GDALOpenEx(filename.c_str(), GDAL_OF_READONLY, 0, 0, 0));
		if(ds) {
			std::string drv(ds->GetDriverName());
			FileType type = Unknown;
			if(drv == "GTiff") {
				type = GTiff;
			} else if(drv == "ENVI") {
				type = ENVI;
			} else if(drv == "ESRI Shapefile") {
				type = SHP;
			} else if(drv == "SQLite") {
				type = SQLITE;
			}
			GDALClose(ds);
			return type;
		}
	}
	return Unknown;
}

/**
 * Return a map containing pairs where the int is the 1-based band index,
 * and the float is the wavelength. Attempts to load from raster metadata
 * or table header. If these fail, will attempt to load from first column
 * of presumably transposed table.
 */
std::map<int, double> hlrg::loadWavelengths(const Contrem& contrem) {
	std::map<int, double> map;
	switch(getFileType(contrem.spectra)) {
	case GTiff:
	case ENVI:
		{
			GDALReader rdr(contrem.spectra);
			for(const auto& it : rdr.getBandMap())
				map[it.second] = (double) it.first / WL_SCALE;
		}
		break;
	case CSV:
		{
			CSVReader rdr(contrem.spectra, contrem.wlTranspose, contrem.wlHeaderRows, contrem.wlMinCol, contrem.wlMaxCol);
			for(const auto& it : rdr.getBandMap())
				map[it.second] = (double) it.first / WL_SCALE;
		}
		break;
	case SHP:
	case ROI:
	default:
		throw std::runtime_error("Invalid file type: " + contrem.spectra);
	}
	return map;
}

std::string hlrg::fileTypeAsString(FileType type) {
	switch(type) {
	case GTiff: return "GTiff";
	case ENVI: return "ENVI";
	case ROI: return "ENVI ROI";
	case SHP: return "Shapefile";
	case CSV: return "CSV";
	default: return "";
	}
}
