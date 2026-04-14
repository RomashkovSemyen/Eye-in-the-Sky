#include <fstream>
#include <sstream>
#include <string>
#include <cmath>
#include <algorithm>
#include <vector>

const double EARTH_ANGULAR_VELOCITY = 7.2921150e-5;  // рад/с
const double RAD_TO_DEG = 180.0 / M_PI;

double normalize_angle(double angle) {
    return std::atan2(std::sin(angle), std::cos(angle));
}

int main() {
    const std::string input_filename = "satellite_1_orbit.csv";
    const std::string output_filename = "points.txt";

    std::ifstream infile(input_filename);
    if (!infile.is_open()) {
        return 1;
    }

    std::vector<std::pair<double, double>> points; // храним пары (phi_deg, lambda_deg)

    std::string line;
    while (std::getline(infile, line)) {
        // Замена запятых на пробелы для удобного чтения через stringstream
        std::replace(line.begin(), line.end(), ',', ' ');

        std::istringstream iss(line);
        double x, y, z, t;
        if (!(iss >> t >> x >> y >> z)) {
            continue; // пропускаем некорректные строки
        }

        double norm = std::sqrt(x*x + y*y + z*z);
        if (norm == 0.0) continue; // защита от деления на ноль

        double xn = x / norm;
        double yn = y / norm;
        double zn = z / norm;

        double phi_rad = std::asin(zn);
        double lambda0_rad = std::atan2(yn, xn);
        double lambda_corrected_rad = lambda0_rad - t * EARTH_ANGULAR_VELOCITY;
        double lambda_norm_rad = normalize_angle(lambda_corrected_rad);

        double phi_deg = phi_rad * RAD_TO_DEG;
        double lambda_deg = lambda_norm_rad * RAD_TO_DEG;

        points.emplace_back(phi_deg, lambda_deg);
    }
    infile.close();

    std::ofstream outfile(output_filename);
    if (!outfile.is_open()) {
        return 1;
    }

    outfile.precision(6);
    outfile << std::fixed << points.size() << "\n"; // первая строка – количество точек

    for (const auto& p : points) {
        outfile << p.first << " " << p.second << "\n";
    }

    return 0;
}