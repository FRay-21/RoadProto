#include "domain/alignment/StationFormatter.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace roadproto::domain::alignment {

std::wstring StationFormatter::format(double station)
{
    const auto safeStation = std::max(0.0, station);
    const auto kilometer = static_cast<int>(std::floor(safeStation / 1000.0));
    const auto meter = safeStation - static_cast<double>(kilometer) * 1000.0;
    const auto roundedMeter = std::round(meter);

    std::wstringstream stream;
    stream << L"K" << kilometer << L"+";

    if (std::fabs(meter - roundedMeter) < 1e-6) {
        stream << std::setw(3) << std::setfill(L'0') << static_cast<int>(roundedMeter);
    } else {
        stream << std::setw(7) << std::setfill(L'0') << std::fixed << std::setprecision(3) << meter;
    }

    return stream.str();
}

} // namespace roadproto::domain::alignment
