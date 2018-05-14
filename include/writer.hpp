/*
 * writer.hpp
 *
 *  Created on: May 9, 2018
 *      Author: rob
 */

#ifndef WRITER_HPP_
#define WRITER_HPP_

#include <vector>
#include <string>

#include <gdal_priv.h>

class Writer {
public:
	virtual bool write(std::vector<double>& buf, int col, int row, int cols, int rows, int bufSize) = 0;
	virtual bool writeStats(const std::string& filename, const std::vector<std::string>& names = {}) = 0;
	virtual ~Writer() {}
};

class GDALWriter {
private:
	GDALDataset* m_ds;
	int m_bands;
	int m_cols;
	int m_rows;

public:

	enum DataType {
		Byte,
		Int32,
		Float32
	};

	GDALWriter(const std::string& filename, const std::string& driver, int cols, int rows, int bands,
			const std::string& fieldName = "", const std::vector<std::string>& bandNames = {}, DataType type = DataType::Float32);
	bool write(std::vector<double>& buf, int col, int row, int cols, int rows, int bufSize);
	bool writeStats(const std::string& filename, const std::vector<std::string>& names = {});
	~GDALWriter();
};


#endif /* WRITER_HPP_ */
