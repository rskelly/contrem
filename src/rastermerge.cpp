/*
 * rastermerge.cpp
 *
 *  Created on: Dec 19, 2019
 *      Author: rob
 */

#include <string>
#include <vector>
#include <iostream>
#include <list>
#include <thread>
#include <mutex>

#include <gdal_priv.h>

#include "grid.hpp"
#include "util.hpp"

using namespace geo::grid;
using namespace geo::util;

bool loadRasters(const std::string& outfile, const std::vector<std::string>& files,
		std::vector<int> bands, int count, int resample, Band<float>& data, bool mapped) {

	GridProps props;
	Bounds<double> bounds;
	std::vector<std::unique_ptr<Band<float>>> rasters;
	// Create the list of bands to load.
	for(size_t i = 0; i < (size_t) count; ++i) {
		rasters.emplace_back(new Band<float>(files[i], bands[i] - 1, false, mapped));
		if(i == 0)
			props = rasters.front()->props();
		bounds.extend(props.bounds());
	}

	// If resampling is set, resize the output raster.
	if(resample > 1) {
		props.setResolution(props.resX() / resample, props.resY() / resample);
		props.setSize(props.cols(), props.rows());
	}
	props.setBounds(bounds);
	props.setWritable(true);
	props.setBands(1);

	// Initialize the output raster.
	data.init(outfile, props, true);
	data.fill(props.nodata());

	// Copy the constituent bands into the output raster.
	for(std::unique_ptr<Band<float>>& band : rasters) {
		const GridProps& p = band->props();
		// If resampling is set, copy pixels one by one.
		for(int row = 0; row < p.rows(); row += resample) {
			for(int col = 0; col < p.cols(); col += resample) {
				float v = band->get(col, row);
				if(v != p.nodata()) { // Leave the output raster's nodata intact if this pixel has nodata.
					int c = props.toCol(p.toX(col));
					int r = props.toRow(p.toY(row));
					data.set(c, r, band->get(col, row));
				}
			}
		}
		/*} else {
			// Copy the whole raster.
			int c = props.toCol(p.toX(0));
			int r = props.toRow(p.toY(0));
			band->writeTo(data, p.cols(), p.rows(), 0, 0, c, r);
		}*/
	}
	return true;
}

// Smooth the raster.
bool smooth(std::vector<bool>& filled, Band<float>& src, Band<float>& dst, int col, int row, int cols, int rows) {
	if(filled[row * cols + col])
		return false;
	float t = 0, w = 0;
	int n = 0;
	double v;
	for(int r = 0; r < rows; ++r) {
		for(int c = 0; c < cols; ++c) {
			if(!std::isnan((v = src.get(c, r)))) {
				float w0 = c == col && r == row ? 1.0 : 1.0 / (std::pow(c - col, 2) + std::pow(r - row, 2));
				t += v * w0;
				w += w0;
				++n;
			}
		}
	}
	if(n && w > 0) {
		dst.set(col, row, t / w);
		filled[row * cols + col] = true;
		return true;
	}
	return false;
}

void processIDW(std::list<std::pair<int, int>>* rowq, std::mutex* qmtx, Band<float>* src, Band<float>* dst, std::mutex* dmtx, int size) {
	int row, block;
	const GridProps& sprops = src->props();
	const GridProps& dprops = dst->props();
	
	// Row data buffer.
	std::vector<float> rowbuf;

	int dcols = dst->props().cols();
	int drows = dst->props().rows();

	while(!rowq->empty()) {
		{
			std::lock_guard<std::mutex> lk(*qmtx);
			if(!rowq->empty()) {
				std::pair<int, int>& item = rowq->front();
				row = item.first;
				block = item.second;
				rowq->pop_front();
				std::cout << "Row " << row << " of " << sprops.rows() << "\n";
			} else {
				std::this_thread::yield();
				continue;
			}
		}

		if(row + block >= drows)
			block = drows - row;

		// Row data buffer.
		rowbuf.resize(dcols * block);
		std::fill(rowbuf.begin(), rowbuf.end(), dprops.nodata());

		float maxrad = size * size;
		float v0, v1;
		for(int b = 0; b < block; ++b) {
			for(int col = 0; col < dcols; ++col) {
				if((v1 = dst->get(col, row + b)) == dprops.nodata())
					continue;
				float s = 0;
				float w = 0;
				bool halt = false;
				int scol = sprops.toCol(dprops.toX(col));
				int srow = sprops.toRow(dprops.toY(row));
				for(int r = -size; !halt && r < size+ 1; ++r) {
					for(int c = -size; c < size + 1; ++c) {
						int cc = scol + c;
						int rr = srow + r;
						float d = (float) (c * c + r * r);
						if(d <= maxrad && sprops.hasCell(cc, rr) && (v0 = src->get(cc, rr)) != sprops.nodata()) {
							if(d == 0) {
								s = v0;
								w = 1;
								halt = true;
								break;
							} else {
								float w0 = 1.0f / d;
								s += v0 * w0;
								w += w0;
							}
						}
					}
				}
				if(w > 0)
					rowbuf[b * dcols + col] = v1 + s / w;
			}
		}
		std::lock_guard<std::mutex> lk(*dmtx);
		for(int b = 0; b < block; ++b)
			dst->setRow(row + b, rowbuf.data() + b * dcols);
	}
}

void processDW(std::list<std::pair<int, int>>* rowq, std::mutex* qmtx, Band<float>* src, Band<float>* dst, std::mutex* dmtx, int size) {
	int row, block;
	int half = size / 2 + 1; // Split the size in half for the radius of the kernel.
	const GridProps& sprops = src->props();
	const GridProps& dprops = dst->props();

	// Row data buffer.
	std::vector<float> rowbuf;

	int dcols = dst->props().cols();
	int drows = dst->props().rows();

	while(!rowq->empty()) {
		{
			std::lock_guard<std::mutex> lk(*qmtx);
			if(!rowq->empty()) {
				std::pair<int, int>& item = rowq->front();
				row = item.first;
				block = item.second;
				rowq->pop_front();
				std::cout << "Row " << row << " of " << sprops.rows() << "\n";
			} else {
				std::this_thread::yield();
				continue;
			}
		}

		if(row + block >= drows)
			block = drows - row;

		rowbuf.resize(dcols * block);
		std::fill(rowbuf.begin(), rowbuf.end(), dprops.nodata());

		float v0, v1;
		for(int b = 0; b < block; ++b) {
			for(int col = 0; col < dcols; ++col) {
				if((v1 = dst->get(col, row + b)) == dprops.nodata())
					continue;
				float s = 0;
				float w = 0;
				int scol = sprops.toCol(dprops.toX(col));
				int srow = sprops.toRow(dprops.toY(row));
				for(int r = -half; r < half + 1; ++r) {
					for(int c = -half; c < half + 1; ++c) {
						int cc = scol + c;
						int rr = srow + r;
						float d = 1.0f - std::sqrt((float) (c * c + r * r)) / half;
						if(d <= 1.0f && sprops.hasCell(cc, rr) && (v0 = src->get(cc, rr)) != sprops.nodata()) {
							s += v0 * d;
							w += d;
						}
					}
				}
				if(w > 0 && (v1 = dst->get(col, row)) != dprops.nodata())
					rowbuf[b * drows + col] = v1 + s / w;
			}
		}
		std::lock_guard<std::mutex> lk(*dmtx);
		for(int b = 0; b < block; ++b)
			dst->setRow(row + b, rowbuf.data() + b * dcols);
	}
}

void processCos(std::list<std::pair<int, int>>* rowq, std::mutex* qmtx, Band<float>* src, Band<float>* dst, std::mutex* dmtx, int size, float* cos) {
	float rad2 = std::pow(size / 2.0, 2.0) / 1000;	// Because the cosine lookup has 1000 elements.
	int row, block;
	const GridProps& sprops = src->props();
	while(!rowq->empty()) {
		{
			std::lock_guard<std::mutex> lk(*qmtx);
			if(!rowq->empty()) {
				std::pair<int, int>& item = rowq->front();
				row = item.first;
				block = item.second;
				rowq->pop_front();
				std::cout << "Row " << row << " of " << sprops.rows() << "\n";
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}
		}

		float v0;
		for(int col = 0; col < sprops.cols(); ++col) {
			float s = 0;
			float w = 0;
			bool halt = false;
			for(int r = -size / 2; !halt && r < size / 2 + 1; ++r) {
				for(int c = -size / 2; c < size / 2 + 1; ++c) {
					int cc = col + c;
					int rr = row + r;
					if(cc < 0 || rr < 0 || cc >= sprops.cols() || rr >= sprops.rows())
						continue;
					if((v0 = src->get(cc, rr)) != sprops.nodata()) {
						int d = (int) std::min(1000.0f, (float) (c * c + r * r) / rad2);
						//std::cout << d << " " << c << " " << r << " " << (c * c + r * r) << " " << rad2 << "\n";
						if(d < 1000){
							float w0 = cos[d];
							s += v0 * w0;
							w += 1;
						}
					}
				}
			}
			if(w > 0) {
				std::lock_guard<std::mutex> lk(*dmtx);
				dst->set(col, row, s / w);
			}
		}
	}
}


void processGauss(std::list<std::pair<int, int>>* rowq, std::mutex* qmtx, Band<float>* src, Band<float>* dst, std::mutex* dmtx, int size, float sigma) {
	int row, block;
	const GridProps& sprops = src->props();
	while(!rowq->empty()) {
		{
			std::lock_guard<std::mutex> lk(*qmtx);
			if(!rowq->empty()) {
				std::pair<int, int>& item = rowq->front();
				row = item.first;
				block = item.second;
				rowq->pop_front();
				std::cout << "Row " << row << " of " << sprops.rows() << "\n";
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}
		}

		float gain = 0.5;
		float v0;
		for(int col = 0; col < sprops.cols(); ++col) {
			float s = 0;
			float w = 0;
			bool halt = false;
			for(int r = -size / 2; !halt && r < size / 2 + 1; ++r) {
				for(int c = -size / 2; c < size / 2 + 1; ++c) {
					int cc = col + c;
					int rr = row + r;
					if(cc < 0 || rr < 0 || cc >= sprops.cols() || rr >= sprops.rows())
						continue;
					if((v0 = src->get(cc, rr)) != sprops.nodata()) {
						float w0 = std::exp(-0.5 * (c * c + r * r) / (sigma * sigma));
						//std::cout << d << " " << c << " " << r << " " << (c * c + r * r) << " " << rad2 << "\n";
						s += v0 * w0;
						w += gain;
					}
				}
			}
			if(w > 0) {
				std::lock_guard<std::mutex> lk(*dmtx);
				dst->set(col, row, s / w);
			}
		}
	}
}

class Pt {
public:
	double x, y, z;
	Pt(double x, double y, double z) : x(x), y(y), z(z) {}
	double& operator[](int idx) {
		switch(idx % 3) {
		case 0: return x;
		case 1: return y;
		case 2:
		default: return z;
		}
	}
};

int main(int argc, char** argv) {

	if(argc < 6) {
		std::cerr << "Usage: rastermerge [options] <<anchor file 1> <anchor band 1> [<anchor file 2> <anchor band 2> [...]]> <target file 2> <target band 2> <output file>\n"
				<< " -s <size>          The radius of the window in map units.\n"
				<< " -t <threads>       The number of threads.\n"
				<< " -m <method>        The method: idw, dw, gauss, cosine. Default IDW.\n"
				<< " -k <mask> <band>   A mask file. Pixel value 1 is kept.\n"
				<< " -r <mult>          A resample multiplier. 1 is no change, 2 doubles the side of a pixel, etc.\n"
				<< " -y                 If given, use in-core memory instead of mapped.\n"
				<< " -b <block size>    The height of processed rows. Blocks are processed in parallel. Default 1\n";
		return 1;
	}

	std::vector<std::string> files;
	std::vector<int> bands;
	float radius = 100;
	int tcount = 4;
	std::string method = "idw";
	int block = 1;
	std::string outfile;
	std::string maskfile;
	int maskband = 0;
	bool mapped = true;
	int resample = 1;

	for(int i = 1; i < argc; ++i) {
		std::string arg(argv[i]);
		if(arg == "-s") {
			radius = atof(argv[++i]);
		} else if(arg == "-y") {
			mapped = false;
		} else if(arg == "-b") {
			block = atoi(argv[++i]);
		} else if(arg == "-r") {
			resample = atoi(argv[++i]);
		} else if(arg == "-k") {
			maskfile = argv[++i];
			maskband = atoi(argv[++i]);
		} else if(arg == "-t") {
			tcount = atoi(argv[++i]);
			if(tcount < 1)
				tcount = 1;
		} else if(arg == "-m") {
			method = argv[++i];
		} else {
			if(i < argc - 1) {
				files.push_back(arg);
				bands.push_back(atoi(argv[++i]));
			} else {
				outfile = arg;
			}
		}
	}

	g_debug("Using mapped memory: " << mapped);

	std::string targetFile = files.back();
	int targetBand = bands.back();

	std::string filebase = outfile.substr(0, outfile.find('.'));
	std::cout << filebase << "\n";

	Band<float> rdiff;
	Band<float> target;
	GridProps tprops;
	{
		Band<float> anchor;
		GridProps aprops;
		GridProps mprops;
		
		target.init(targetFile, targetBand - 1, true, mapped);
		tprops = target.props();

		GridProps dprops(target.props());
		dprops.setNoData(-9999);

		GridProps rprops(dprops);
		if(resample > 1)
			rprops.setSize(rprops.cols() / resample + 1, rprops.rows() / resample + 1);
		rdiff.init("/tmp/rdiff.tif", rprops, mapped);
		rdiff.fill(0);

		loadRasters("/tmp/anchor.tif", files, bands, files.size() - 1, 1, anchor, mapped);
		aprops = anchor.props();

		bool hasMask = !maskfile.empty();
		Band<int> mask;
		if(hasMask) {
			mask.init(maskfile, maskband - 1, false, mapped);
			mprops = mask.props();
		}

		for(int row1 = 0; row1 < tprops.rows(); ++row1) {
			if(row1 % 100)
				g_debug("Row " << row1 << " of " << tprops.rows());
			float v1, v2;
			for(int col1 = 0; col1 < tprops.cols(); ++col1) {

				if((v1 = target.get(col1, row1)) == tprops.nodata() || std::isnan(v1))
					continue;

				double x = tprops.toX(col1);
				double y = tprops.toY(row1);
				int col2 = aprops.toCol(x);
				int row2 = aprops.toRow(y);
				int mc = mprops.toCol(x);
				int mr = mprops.toRow(y);
						
				if(!aprops.hasCell(col2, row2)
//						&& (!hasMask || (mprops.hasCell(mc, mr) && mask.get(mc, mr) == 1))
						|| (v2 = anchor.get(col2, row2)) == aprops.nodata() || std::isnan(v2))
					continue;
						
				rdiff.set(col1 / resample, row1 / resample, rdiff.get(col1 / resample, row1 / resample) + v2 - v1);
			}
		}

		for(int row = 0; row < rprops.rows(); ++row) {
			for(int col = 0; col < rprops.cols(); ++col) {
				rdiff.set(col, row, rdiff.get(col, row) / (resample * resample));
			}
		}
	}

	{

		Band<float> output(outfile, tprops, mapped);
		target.writeTo(output);	
		
		// Queue contains start index of row and number of rows.
		std::list<std::pair<int, int>> rowq;
		for(int row = 0; row < rdiff.props().rows(); row += block)
			rowq.emplace_back(row, block);

		std::mutex qmtx;
		std::mutex dmtx;
		std::vector<std::thread> threads;

		// Calculate the search radius in columns.
		int size = std::ceil((radius * 2) / std::abs(rdiff.props().resX()));
		if(resample > 1)
			size = size / resample + 1;
		if(size % 2 == 0)
			size++;

		if(method == "gauss") {
			for(int i = 0; i < tcount; ++i)
				threads.emplace_back(processGauss, &rowq, &qmtx, &rdiff, &output, &dmtx, size, (float) size * 0.25);
		} else if(method == "cosine") {
			float cos[1001];
			for(int i = 0; i <= 1000; ++i)
				cos[i] = std::cos((float) i / 1000 * M_PI) / 2.0 + 0.5;
			for(int i = 0; i < tcount; ++i)
				threads.emplace_back(processCos, &rowq, &qmtx, &rdiff, &output, &dmtx, size, cos);
		} else if(method == "idw") {
			for(int i = 0; i < tcount; ++i)
				threads.emplace_back(processIDW, &rowq, &qmtx, &rdiff, &output, &dmtx, size);
		} else if(method == "dw") {
			for(int i = 0; i < tcount; ++i)
				threads.emplace_back(processDW, &rowq, &qmtx, &rdiff, &output, &dmtx, size);
		} else {
			std::cerr << "Unknown method: " << method << "\n";
			return 1;
		}

		for(int i = 0; i < tcount; ++i) {
			if(threads[i].joinable())
				threads[i].join();
		}
	}

	return 0;
}

