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
        std::cerr << "Usage: " << argv[0] << " <input.jpg> <output_base.jpg> <coords.txt> [step=" << DEFAULT_STEP << "]\n";
        std::cerr << "  step : take every step-th point (default " << DEFAULT_STEP << ")\n";
        std::cerr << "Will generate output_base_1.jpg, output_base_2.jpg, ...\n";
        return 1;
    }

    const char* inputFile  = argv[1];
    std::string outputBase = argv[2];
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

    // Генерация изображений для каждого k от 1 до M
    for (int k = 1; k <= M; ++k) {
        unsigned char* frame = copyImage(original, width, height, 3);

        if (k == 1) {
            drawPoint(frame, width, height, px[0], py[0], 255, 0, 0);
        } else {
            for (int i = 0; i < k - 1; ++i) {
                if (angularDistance(dx[i], dy[i], dx[i+1], dy[i+1]) > 100.0) {
                    continue;
                }
                drawThickLine(frame, width, height, px[i], py[i], px[i+1], py[i+1], 255, 0, 0);
            }
        }

        std::ostringstream outName;
        std::string base = outputBase;
        size_t dotPos = base.rfind('.');
        if (dotPos != std::string::npos && base.substr(dotPos) == ".jpg") {
            outName << base.substr(0, dotPos) << "_" << k << ".jpg";
        } else {
            outName << base << "_" << k << ".jpg";
        }
        std::string outFile = outName.str();

        if (!stbi_write_jpg(outFile.c_str(), width, height, 3, frame, 90)) {
            std::cerr << "Error: cannot write image " << outFile << "\n";
            delete[] frame;
            stbi_image_free(original);
            return 1;
        }

        std::cout << "Saved " << outFile << " (" << k << " point(s)/segment(s))\n";
        delete[] frame;
    }

    stbi_image_free(original);
    std::cout << "All done. Generated " << M << " images.\n";
    return 0;
}