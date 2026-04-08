#include <iostream>
#include <fstream>
#include <cmath>
#include <iomanip>

// Искусственные параметры Земли
const double M_EARTH = 2.0e30;      // кг
const double G = 6.67430e-11;       // м³/(кг·с²)
const double MU = G * M_EARTH;       // м³/с²
const double MU_KM3 = MU / 1e9;      // км³/с² ≈ 4.00458e5
const double R_EARTH = 697000.0;       // км
const double EPS = 1e-12;
const double RAD_TO_DEG = 180.0 / M_PI;

struct Vector { double x, y, z; };
double dot(const Vector& a, const Vector& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vector cross(const Vector& a, const Vector& b) { return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x }; }
double norm(const Vector& v) { return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z); }

int main() {
    std::ifstream infile("init.txt");
    if (!infile) { std::cerr << "Ошибка: не удалось открыть init.txt\n"; return 1; }
    double x, y, z, vx, vy, vz;
    infile >> x >> y >> z >> vx >> vy >> vz;
    infile.close();

    Vector r = {x, y, z};
    Vector v = {vx, vy, vz};
    double r_mod = norm(r);
    double v2 = dot(v, v);

    // 1. Большая полуось
    double inv_a = 2.0 / r_mod - v2 / MU_KM3;
    double a = 1.0 / inv_a;   // для гиперболы будет отрицательной

    // 2. Удельный момент импульса
    Vector l = cross(r, v);
    double l_mod = norm(l);

    // 3. Вектор эксцентриситета
    Vector v_cross_l = cross(v, l);
    Vector e_vec = { v_cross_l.x/MU_KM3 - r.x/r_mod,
                     v_cross_l.y/MU_KM3 - r.y/r_mod,
                     v_cross_l.z/MU_KM3 - r.z/r_mod };
    double e = norm(e_vec);

    // 4. Наклонение
    double cos_i = l.z / l_mod;
    if (cos_i > 1.0) cos_i = 1.0;
    if (cos_i < -1.0) cos_i = -1.0;
    double i = std::acos(cos_i) * RAD_TO_DEG;

    // 5. Вектор восходящего узла N = (Nx, Ny, 0)
    Vector N;
    double l_xy2 = l.x*l.x + l.y*l.y;
    if (l_xy2 < EPS) {
        N = {1.0, 0.0, 0.0};   // экваториальная орбита, узел не определён – принимаем за Ox
    } else {
        double n_xy = std::sqrt(l_xy2);
        N = {-l.y / n_xy, l.x / n_xy, 0.0};
    }

    // 6. Долгота восходящего узла (ОДНОЗНАЧНО через atan2)
    double W_rad = std::atan2(N.y, N.x);
    double W = W_rad * RAD_TO_DEG;
    if (W < 0.0) W += 360.0;

    // 7. Аргумент перицентра (однозначно)
    double w = 0.0;
    if (e > EPS) {
        double cosw = (N.x*e_vec.x + N.y*e_vec.y) / e;
        double sinw = (N.x*e_vec.y - N.y*e_vec.x) / e;
        if (cosw > 1.0) cosw = 1.0; if (cosw < -1.0) cosw = -1.0;
        if (sinw > 1.0) sinw = 1.0; if (sinw < -1.0) sinw = -1.0;
        w = std::atan2(sinw, cosw) * RAD_TO_DEG;
        if (w < 0.0) w += 360.0;
    }

    // Округление a и e до 3 знаков для проверки перицентра
    double a_rounded = std::round(a * 1000.0) / 1000.0;
    double e_rounded = std::round(e * 1000.0) / 1000.0;
    double r_peri = a_rounded * (1.0 - e_rounded);
    bool crash = (r_peri < R_EARTH && a_rounded > 0);

    // Проверка на вторую космическую скорость
    double v_escape = std::sqrt(2.0 * MU_KM3 / r_mod);
    bool escape = (norm(v) >= v_escape - EPS);

    // Вывод результатов
    std::ofstream outfile("elems.txt");
    if (!outfile) { std::cerr << "Ошибка: не удалось создать elems.txt\n"; return 1; }
    outfile << std::fixed << std::setprecision(3);
    outfile << "a = " << a << "\n";
    outfile << "e = " << e << "\n";
    outfile << "i = " << i << "\n";
    outfile << "W = " << W << "\n";          // теперь один вариант
    outfile << "w = " << w << "\n";

    // Сообщения
    if (crash) {
        outfile << "Ваш спутник скоро разобьётся о поверхность идеально сферической безатмосферной Земли\n";
    } else if (a_rounded > 0) {
        outfile << "Запуск успешен\n";
    } else {
        outfile << "Траектория гиперболическая\n";
    }

    if (escape) {
        outfile << "упс, вы улетели к далёким звёздам\n";
    }

    outfile.close();
    return 0;
}