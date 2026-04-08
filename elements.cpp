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
const double V_INF_ESCAPE_SOLAR = 12.3;  // км/с – добавочная скорость для ухода из Солнечной системы
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
    double x, y, z, vx, vy, vz;
    infile >> x >> y >> z >> vx >> vy >> vz;
    infile.close();

    Vector r = {x, y, z};
    Vector v = {vx, vy, vz};
    double r_mod = norm(r);
    double v_mod = norm(v);
    double v2 = dot(v, v);

    // Если начальная точка уже под поверхностью – немедленное разрушение
    if (r_mod < R_EARTH) {
        std::ofstream outfile("elems.txt");
        outfile << "a = ?\ne = ?\ni = ?\nW = ?\nw = ?\n";
        outfile << "Спутник уже под поверхностью Земли. Разрушение неизбежно.\n";
        return 0;
    }

    // 1. Большая полуось (может быть отрицательной для гиперболы)
    double inv_a = 2.0 / r_mod - v2 / MU_KM3;
    double a = 1.0 / inv_a;

    // 2. Удельный момент импульса
    Vector l = cross(r, v);
    double l_mod = norm(l);

    // 3. Вектор эксцентриситета (Лапласа-Рунге-Ленца)
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

    // 5. Вектор восходящего узла N
    Vector N;
    double l_xy2 = l.x*l.x + l.y*l.y;
    if (l_xy2 < EPS) {
        N = {1.0, 0.0, 0.0};   // экваториальная орбита – узел не определён, принимаем за Ox
    } else {
        double n_xy = std::sqrt(l_xy2);
        N = {-l.y / n_xy, l.x / n_xy, 0.0};
    }

    // 6. Долгота восходящего узла (однозначно через atan2)
    double W_rad = std::atan2(N.y, N.x);
    double W = W_rad * RAD_TO_DEG;
    if (W < 0.0) W += 360.0;

    // 7. Аргумент перицентра
    double w = 0.0;
    if (e > EPS) {
        double cosw = (N.x*e_vec.x + N.y*e_vec.y) / e;
        double sinw = (N.x*e_vec.y - N.y*e_vec.x) / e;
        if (cosw > 1.0) cosw = 1.0; if (cosw < -1.0) cosw = -1.0;
        if (sinw > 1.0) sinw = 1.0; if (sinw < -1.0) sinw = -1.0;
        w = std::atan2(sinw, cosw) * RAD_TO_DEG;
        if (w < 0.0) w += 360.0;
    }

    // Расстояние в перицентре (универсальная формула)
    double r_peri = 0.0;
    if (e > EPS) {
        r_peri = (l_mod * l_mod) / (MU_KM3 * (1.0 + e));
    } else {
        r_peri = std::abs(a);   // для круговой e=0
    }

    // Радиальная скорость (знак: отрицательная = приближение к центру)
    double v_r = dot(r, v) / r_mod;

    // Вторая космическая скорость на текущей высоте
    double v_escape = std::sqrt(2.0 * MU_KM3 / r_mod);

    // Третья космическая скорость (уход из Солнечной системы)
    double v_3 = std::sqrt(v_escape * v_escape + V_INF_ESCAPE_SOLAR * V_INF_ESCAPE_SOLAR);

    // Формирование сообщения о статусе
    std::string status;

    if (e < 1.0 - EPS) {   // Эллиптическая орбита
        if (r_peri < R_EARTH - EPS) {
            status = "Ваш спутник скоро разобьётся о поверхность идеально сферической безатмосферной Земли";
        } else {
            status = "Запуск успешен";
        }
    }
    else {   // Параболическая (e ≈ 1) или гиперболическая (e > 1) траектория
        if (r_peri < R_EARTH - EPS && v_r < -EPS) {
            status = "Ваш спутник скоро разобьётся о поверхность идеально сферической безатмосферной Земли";
        }
        else if (v_mod >= v_escape - EPS) {
            if (v_mod >= v_3 - EPS) {
                status = "упс, ваш спутник улетел к далёким звёздам";
            } else {
                status = "упс, ваш спутник улетел бороздить просторы солнечной системы";
            }
        }
        else {
            // Этот случай теоретически невозможен (при e≥1 скорость ≥ v_escape),
            // но оставлен для защиты от численных погрешностей.
            status = "Запуск успешен (неопределённость)";
        }
    }

    // Вывод результатов в файл
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
    outfile << status << "\n";

    outfile.close();
    return 0;
}