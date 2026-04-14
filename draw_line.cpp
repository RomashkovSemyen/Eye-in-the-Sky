#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>
#include <cmath>

const int THICKNESS = 3;
const int DEFAULT_STEP = 50;

void drawSquare(unsigned char* data, int width, int height,
                int cx, int cy, int radius,
                unsigned char r, unsigned char g, unsigned char b) {
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int x = cx + dx;
            int y = cy + dy;
            if (x >= 0 && x < width && y >= 0 && y < height) {
                int idx = (y * width + x) * 3;
                data[idx] = r;
                data[idx + 1] = g;
                data[idx + 2] = b;
            }
        }
    }
}

void drawThickLine(unsigned char* data, int width, int height,
                   int x0, int y0, int x1, int y1,
                   unsigned char r, unsigned char g, unsigned char b) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (true) {
        drawSquare(data, width, height, x0, y0, THICKNESS, r, g, b);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void drawPoint(unsigned char* data, int width, int height,
               int x, int y, unsigned char r, unsigned char g, unsigned char b) {
    drawSquare(data, width, height, x, y, THICKNESS, r, g, b);
}

unsigned char* copyImage(unsigned char* src, int width, int height, int channels) {
    unsigned char* dst = new unsigned char[width * height * channels];
    std::memcpy(dst, src, width * height * channels);
    return dst;
}

inline int lonToX(double lon, int width) {
    return static_cast<int>(std::round((lon + 180.0) / 360.0 * (width - 1)));
}

inline int latToY(double lat, int height) {
    return static_cast<int>(std::round((90.0 - lat) / 180.0 * (height - 1)));
}

inline double longitudeDiff(double lon1, double lon2) {
    double diff = std::fabs(lon1 - lon2);
    return (diff > 180.0) ? 360.0 - diff : diff;
}

inline double angularDistance(double lat1, double lon1, double lat2, double lon2) {
    double dlat = std::fabs(lat1 - lat2);
    double dlon = longitudeDiff(lon1, lon2);
    return std::sqrt(dlat*dlat + dlon*dlon);
}

int main(int argc, char* argv[]) {
    if (argc < 4 || argc > 5) {
        std::cerr << "Usage: " << argv[0] << " <input.jpg> <output.jpg> <coords.txt> [step=" << DEFAULT_STEP << "]\n";
        std::cerr << "  step : take every step-th point (default " << DEFAULT_STEP << ")\n";
        std::cerr << "Output: a single image with the full trajectory.\n";
        return 1;
    }

    const char* inputFile  = argv[1];
    const char* outputFile = argv[2];
    const char* coordsFile = argv[3];
    int step = DEFAULT_STEP;
    if (argc == 5) {
        step = std::atoi(argv[4]);
        if (step <= 0) {
            std::cerr << "Error: step must be positive.\n";
            return 1;
        }
    }

    // Чтение всех координат из файла
    std::ifstream fin(coordsFile);
    if (!fin.is_open()) {
        std::cerr << "Error: cannot open coordinates file " << coordsFile << "\n";
        return 1;
    }

    int N;
    if (!(fin >> N)) {
        std::cerr << "Error: failed to read N from " << coordsFile << "\n";
        return 1;
    }
    if (N < 1) {
        std::cerr << "Error: N must be at least 1.\n";
        return 1;
    }

    std::vector<double> dx_all(N), dy_all(N);
    for (int i = 0; i < N; ++i) {
        if (!(fin >> dx_all[i] >> dy_all[i])) {
            std::cerr << "Error: not enough coordinates (expected " << 2*N << ").\n";
            return 1;
        }
    }
    fin.close();

    // Прореживание: берём каждую step-ю точку (начиная с индекса 0)
    std::vector<double> dx, dy;
    for (int i = 0; i < N; i += step) {
        dx.push_back(dx_all[i]);
        dy.push_back(dy_all[i]);
    }
    int M = dx.size();
    std::cout << "Using " << M << " points out of " << N << " (step = " << step << ").\n";

    // Загрузка исходного изображения
    int width, height, channels;
    unsigned char* original = stbi_load(inputFile, &width, &height, &channels, 3);
    if (!original) {
        std::cerr << "Error: cannot load image " << inputFile << "\n";
        return 1;
    }

    if (width != 2048 || height != 1025) {
        std::cerr << "Warning: image size is " << width << "x" << height
                  << ", expected 2048x1025. Coordinates may be incorrect.\n";
    }

    // Преобразование географических координат в пиксельные
    std::vector<int> px(M), py(M);
    for (int i = 0; i < M; ++i) {
        px[i] = lonToX(dy[i], width);
        py[i] = latToY(dx[i], height);
    }

    // Создаём финальное изображение как копию исходного
    unsigned char* frame = copyImage(original, width, height, 3);

    // Рисуем все соединительные линии (если расстояние меньше 100°)
    for (int i = 0; i < M - 1; ++i) {
        if (angularDistance(dx[i], dy[i], dx[i+1], dy[i+1]) > 100.0) {
            continue;
        }
        drawThickLine(frame, width, height, px[i], py[i], px[i+1], py[i+1], 255, 0, 0);
    }

    // Рисуем все точки (поверх линий)
    for (int i = 0; i < M; ++i) {
        drawPoint(frame, width, height, px[i], py[i], 255, 0, 0);
    }

    // Сохраняем результат
    if (!stbi_write_jpg(outputFile, width, height, 3, frame, 90)) {
        std::cerr << "Error: cannot write image " << outputFile << "\n";
        delete[] frame;
        stbi_image_free(original);
        return 1;
    }

    std::cout << "Saved final image to " << outputFile << " with " << M << " points.\n";

    delete[] frame;
    stbi_image_free(original);
    return 0;
}