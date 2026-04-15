#include <iostream>
#include <fstream>
#include <cmath>
#include <iomanip>

// Искусственные параметры Земли
const double M_EARTH = 6.0e24;          // кг
const double G = 6.67430e-11;           // м³/(кг·с²)
const double MU = G * M_EARTH;           // м³/с²
const double MU_KM3 = MU / 1e9;          // км³/с² ≈ 4.00458e5
const double R_EARTH = 6400.0;           // км
const double EPS = 1e-12;
const double RAD_TO_DEG = 180.0 / M_PI;

struct Vector { double x, y, z; };
double dot(const Vector& a, const Vector& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vector cross(const Vector& a, const Vector& b) { return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x }; }
double norm(const Vector& v) { return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z); }

int main() {
    std::ifstream infile("init.txt");
    if (!infile) {
        std::cerr << "Ошибка: не удалось открыть init.txt\n";
        return 1;
    }
    double lambda_deg, phi_deg, h, vx, vy, vz;
    infile >> lambda_deg >> phi_deg >> h >> vx >> vy >> vz;
    infile.close();

    double lambda_rad = lambda_deg * M_PI / 180.0;
    double phi_rad   = phi_deg   * M_PI / 180.0;
    double r_center = R_EARTH + h;

    double x = r_center * cos(phi_rad) * cos(lambda_rad);
    double y = r_center * cos(phi_rad) * sin(lambda_rad);
    double z = r_center * sin(phi_rad);

    Vector r = {x, y, z};
    Vector v = {vx, vy, vz};
    double r_mod = norm(r);
    double v_mod = norm(v);
    double v2 = dot(v, v);

    if (r_mod < R_EARTH) {
        std::ofstream outfile("elems.txt");
        outfile << "a = ?\ne = ?\ni = ?\nW = ?\nw = ?\nnu = ?\n";
        outfile << "Спутник уже под поверхностью Земли. Разрушение неизбежно.\n";
        return 0;
    }

    double inv_a = 2.0 / r_mod - v2 / MU_KM3;
    double a = 1.0 / inv_a;

    Vector l = cross(r, v);
    double l_mod = norm(l);

    Vector v_cross_l = cross(v, l);
    Vector e_vec = { v_cross_l.x/MU_KM3 - r.x/r_mod,
                     v_cross_l.y/MU_KM3 - r.y/r_mod,
                     v_cross_l.z/MU_KM3 - r.z/r_mod };
    double e = norm(e_vec);

    double cos_i = l.z / l_mod;
    if (cos_i > 1.0) cos_i = 1.0;
    if (cos_i < -1.0) cos_i = -1.0;
    double i = std::acos(cos_i) * RAD_TO_DEG;

    Vector N;
    double l_xy2 = l.x*l.x + l.y*l.y;
    if (l_xy2 < EPS) {
        N = {1.0, 0.0, 0.0};
    } else {
        double n_xy = std::sqrt(l_xy2);
        N = {-l.y / n_xy, l.x / n_xy, 0.0};
    }

    double W_rad = std::atan2(N.y, N.x);
    double W = W_rad * RAD_TO_DEG;
    if (W < 0.0) W += 360.0;

    double w = 0.0;
    if (e > EPS) {
        double cosw = (N.x*e_vec.x + N.y*e_vec.y) / e;
        double sinw = (N.x*e_vec.y - N.y*e_vec.x) / e;
        if (cosw > 1.0) cosw = 1.0; if (cosw < -1.0) cosw = -1.0;
        if (sinw > 1.0) sinw = 1.0; if (sinw < -1.0) sinw = -1.0;
        w = std::atan2(sinw, cosw) * RAD_TO_DEG;
        if (w < 0.0) w += 360.0;
    }

    double r_peri = 0.0;
    if (e > EPS) {
        r_peri = (l_mod * l_mod) / (MU_KM3 * (1.0 + e));
    } else {
        r_peri = std::abs(a);
    }

    double v_r = dot(r, v) / r_mod;
    double v_escape = std::sqrt(2.0 * MU_KM3 / r_mod);

    // ====== Добавленный блок: истинная аномалия ======
    double nu_deg = 0.0;
    if (e > EPS) {
        double p = (l_mod * l_mod) / MU_KM3;
        double cos_nu = (p / r_mod - 1.0) / e;
        if (cos_nu > 1.0) cos_nu = 1.0;
        if (cos_nu < -1.0) cos_nu = -1.0;
        double nu = std::acos(cos_nu);
        if (v_r < 0.0) {
            nu = 2.0 * M_PI - nu;
        }
        nu_deg = nu * RAD_TO_DEG;
    }
    // ===============================================

    std::string status;
    if (l_mod < EPS) {
        if (v_mod < v_escape - EPS) {
            status = "Ваш спутник скоро разобьётся о поверхность идеально сферической безатмосферной Земли";
        } else {
            status = "упс, ваш спутник улетел бороздить просторы солнечной системы";
        }
    }
    else if (e < 1.0 - EPS) {
        if (r_peri < R_EARTH - EPS) {
            status = "Ваш спутник скоро разобьётся о поверхность идеально сферической безатмосферной Земли";
        } else {
            status = "Запуск успешен";
        }
    }
    else {
        if (r_peri < R_EARTH - EPS && v_r < -EPS) {
            status = "Ваш спутник скоро разобьётся о поверхность идеально сферической безатмосферной Земли";
        }
        else if (v_mod >= v_escape - EPS) {
            status = "упс, ваш спутник улетел бороздить просторы солнечной системы";
        }
        else {
            status = "Запуск успешен";
        }
    }

    std::ofstream outfile("elems.txt");
    if (!outfile) {
        std::cerr << "Ошибка: не удалось создать elems.txt\n";
        return 1;
    }
    outfile << std::fixed << std::setprecision(3);
    outfile << "a = " << a << "\n";
    outfile << "e = " << e << "\n";
    outfile << "i = " << i << "\n";
    outfile << "W = " << W << "\n";
    outfile << "w = " << w << "\n";
    outfile << "nu = " << nu_deg << "\n";
    outfile << status << "\n";

    outfile.close();
    return 0;
}