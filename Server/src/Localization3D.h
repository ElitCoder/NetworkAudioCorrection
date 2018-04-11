#ifndef LOCALIZATION3D_H
#define LOCALIZATION3D_H

#include <vector>
#include <string>
#include <array>

using Localization3DInput = std::vector<std::pair<std::string, std::vector<double>>>;

class Localization3D {
public:
	static std::vector<std::array<double, 3>> run(const Localization3DInput& input, bool fast_calcuation);
};

#endif