/*
 * splinesmooth.cpp
 *
 *  Created on: Oct 27, 2019
 *      Author: rob
 */

#include <iostream>
#include <string>
#include <vector>

#include "util.hpp"
#include "grid.hpp"

using namespace geo::util;
using namespace geo::util::csv;
using namespace geo::grid;

void usage() {
	std::cout << "Usage: splinesmooth [options] <points> <columns> <outfile>\n"
			" -rx <res x>        The output grid resolution in x.\n"
			" -ry <res y>        The output grid resolution in y.\n"
			" -s <srid>          The projection of the output grid.\n"
			" -b <buffer>        A buffer around the maxima of the point set to define\n"
			"                    the bounds of the output raster.\n"
			" -t <raster>        A template raster. Supercedes the resolution, projection,"
			"                    srid and buffer parameters.\n"
			" -h                 If there's a header in the csv point file, use this switch.\n\n"
			" -m <smooth>        The smoothing parameter. If not given or less than or equal to zero, \n"
			"                    the number of input points is used.\n"
			" <points>           Is a CSV file containing at least x, y and z columns with zero or one header lines.\n"
			" <columns>          A comma-delimited list of indices of columns in the csv file\n"
			"                    for the x, y and z columns. An optional fourth column will be\n"
			"                    used for weights. This must be accompanied by the -m switch with\n"
			"                    a smoothing factor. If weights are not given, the std. deviation\n"
			"                    of the z-coordinates is used.\n"
			" <outfile>          The name of a geotiff to write output to.\n";
}

int main(int argc, char** argv) {

	std::vector<std::string> args;

	double xres = 0, yres = 0;		// Output resolution if no template is given.
	int srid = 0;					// Spatial reference ID for the output (must match csv file).
	double buffer = 0;				// Buffer to add to the point bounds if no template is given.
	std::string tpl;				// Filename of template raster.
	std::string projection;			// Raster output projection.
	bool header = false;			// True if there is one field header in the csv file.
	std::vector<int> columns;		// Column indices for the csv file.
	double minx, miny, maxx, maxy;	// The dataset bounds. If there's a template, use those bounds, otherwise use the buffered point bounds.
	bool hasTemplate = false;		// Set to true if a template is loaded.
	GridProps props;				// Properties for the output grid.
	double smooth = 0;				// The smoothing parameter for the bivariate spline.

	for(int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if(arg == "-rx") {
			xres = atof(argv[++i]);
		} else if(arg == "-ry") {
			yres = atof(argv[++i]);
		} else if(arg == "-s") {
			srid = atoi(argv[++i]);
		} else if(arg == "-b") {
			buffer = atof(argv[++i]);
		} else if(arg == "-t") {
			tpl = argv[++i];
		} else if(arg == "-h") {
			header = true;
		} else if(arg == "-m") {
			smooth = atof(argv[++i]);
			if(smooth < 0)
				smooth = 0;
		} else {
			args.push_back(argv[i]);
		}
	}

	if(args.size() < 3) {
		std::cerr << "Too few arguments.\n";
		usage();
		return 1;
	}

	if(tpl.empty() && (xres == 0 || yres == 0)) {
		std::cerr << "If a template is not given, xres and yres must be nonzero.\n";
		usage();
		return 1;
	}

	// Parse the column indices.
	{
		std::vector<std::string> cs;
		split(std::back_inserter(cs), args[1], ",");
		for(const std::string& c : cs)
			columns.push_back(atoi(c.c_str()));
		if(columns.size() < 3) {
			std::cerr << "Too few csv columns. There must be three.\n";
			usage();
			return 1;
		}
	}

	// Load the template raster.
	if(!tpl.empty()) {
		try {
			Grid<float> tplg(tpl);
			const GridProps tprops = tplg.props();
			const Bounds& tbounds = tprops.bounds();
			xres = tprops.resX();
			yres = tprops.resY();
			projection = tprops.projection();
			minx = tbounds.minx();
			miny = tbounds.miny();
			maxx = tbounds.maxx();
			maxy = tbounds.maxy();
			props = tprops;
			hasTemplate = true;
		} catch(const std::runtime_error& ex) {
			std::cerr << "Failed to load template raster.\n";
			usage();
			return 1;
		}
	}

	// Load the CSV data.
	std::vector<double> x;
	std::vector<double> y;
	std::vector<double> z;
	std::vector<double> w;
	{
		CSV csv(args[0], header);
		std::vector<CSVValue> cx = csv.column(columns[0]);
		std::vector<CSVValue> cy = csv.column(columns[1]);
		std::vector<CSVValue> cz = csv.column(columns[2]);
		std::vector<CSVValue> cw;
		if(columns.size() > 3)
			cw = csv.column(columns[3]);
		if(!(cx.size() == cy.size() && cy.size() == cz.size())) {
			std::cerr << "Input coordinate arrays must be the same length.\n";
			usage();
			return 1;
			if(!cw.empty() && cx.size() != cw.size()) {
				std::cerr << "The weights list must be the same length as the coordinate arrays.\n";
				usage();
				return 1;
			}
		}
		for(const CSVValue& v : cx)
			x.push_back(v.asDouble());
		for(const CSVValue& v : cy)
			y.push_back(v.asDouble());
		for(const CSVValue& v : cz)
			z.push_back(v.asDouble());
		if(!cw.empty()) {
			for(const CSVValue& v : cw)
				w.push_back(v.asDouble());
		}
	}

	// Set the bounds if there's no template.
	if(!hasTemplate) {
		minx = miny = std::numeric_limits<double>::max();
		maxx = maxy = std::numeric_limits<double>::lowest();

		for(double xx : x) {
			if(xx < minx) minx = xx;
			if(xx > maxx) maxx = xx;
		}
		for(double yy : y) {
			if(yy < miny) miny = yy;
			if(yy > maxy) maxy = yy;
		}

		minx -= buffer;
		miny -= buffer;
		maxx += buffer;
		maxy += buffer;

		props.setResolution(xres, yres);
		props.setProjection(projection);
		props.setSrid(srid);
		props.setBounds(Bounds(minx, miny, maxx, maxy));
	}

	props.setNoData(-9999.0);
	props.setDataType(DataType::Float32);
	props.setWritable(true);
	props.setBands(1);

	Grid<float> outgrid(args[2], props);

	BivariateSpline bvs;
	bvs.init(smooth, x, y, z, w, minx, miny, maxx, maxy);

	x.clear();
	y.clear();
	z.clear();
	w.clear();

	int cols = props.cols();
	int rows = props.rows();
	x.resize(1);
	y.resize(1);
	z.resize(1);
	for(int r = 0; r < rows; ++r) {
		for(int c = 0; c < cols; ++c) {
			x[0] = props.toX(c);	// TODO: This only seems to work one cell at a time...
			y[0] = props.toY(r);
			bvs.evaluate(x, y, z);
			outgrid.set(x[0], y[0], z[0], 0);
		}
	}



}

