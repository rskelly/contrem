/*
 * contrem_util.hpp
 *
 *  Created on: Jun 4, 2019
 *      Author: rob
 */

#ifndef INCLUDE_CONTREM_UTIL_HPP_
#define INCLUDE_CONTREM_UTIL_HPP_

#include <array>

namespace hlrg {

enum FileType {
	GTiff,
	ENVI,
	ROI,
	SHP,
	CSV,
	SQLITE,
	Unknown
};

constexpr std::array<FileType, 3> INPUT_TYPES = {GTiff, ENVI, CSV};				///<! Allowed input types for spectral data.
constexpr std::array<FileType, 3> OUTPUT_TYPES = {GTiff, ENVI, CSV};			///<! Allowed output types for results.
constexpr std::array<FileType, 5> ROI_TYPES = {GTiff, ENVI, SHP, SQLITE, ROI};	///<! Allowed mask/ROI file types.

FileType getFileType(const std::string& filename);

/**
 * Return a map containing pairs where the int is the 1-based band index,
 * and the float is the wavelength. Attempts to load from raster metadata
 * or table header. If these fail, will attempt to load from first column
 * of presumably transposed table.
 */
std::map<int, double> loadWavelengths(const std::string& filename);

std::string fileTypeAsString(FileType type);


} // hlrg

#endif /* INCLUDE_CONTREM_UTIL_HPP_ */
