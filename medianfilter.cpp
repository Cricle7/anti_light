#define STB_IMAGE_IMPLEMENTATION
#include "./stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "./stb/stb_image_write.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>

static constexpr int w = 100;
static constexpr int h = 100;
using namespace std;

double calculateK(double xb) {
    if (xb < 20) {
        return 2.5;
    } else if (xb >= 20 && xb <= 100) {
        return 1 + (1.5 * (100 - xb)) / 80;
    } else if (xb > 100 && xb < 200) {
        return 1;
    } else {
        return 1 + (xb - 200) / 35;
    }
}
float f(float x, float y) {
    return cos(x) + cos(y);
}

float g(float x) {
    return sin(x) + sin(x * 3) / 3 + sin(x * 5) / 5;
}

float s(float x, float y) {
    return (f(x / 15, y / 15) > 0 ? 1 : 0.4) + 0.05 * f(x, y) + 0.05 * g(sqrt(x * x + y * y)) - 0.3 + 0.019 *x;
}

void generate(uint8_t* image, int width, int height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            image[y * width + x] = static_cast<uint8_t>(std::clamp(int(s(x-w/2, y-h/2) * 255), 0, 255));
        }
    }
}
void padImage(const uint8_t* src, uint8_t* dst, int width, int height, int pad) {
    int paddedWidth = width + 2 * pad;
    int paddedHeight = height + 2 * pad;

    // 填充中间部分
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            dst[(y + pad) * paddedWidth + (x + pad)] = src[y * width + x];
        }
    }

    // 填充上下边缘
    for (int y = 0; y < pad; ++y) {
        for (int x = 0; x < width; ++x) {
            dst[y * paddedWidth + (x + pad)] = src[x];
            dst[(height + pad + y) * paddedWidth + (x + pad)] = src[(height - 1) * width + x];
        }
    }

    // 填充左右边缘
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < pad; ++x) {
            dst[(y + pad) * paddedWidth + x] = src[y * width];
            dst[(y + pad) * paddedWidth + (width + pad + x)] = src[y * width + (width - 1)];
        }
    }

    // 填充四个角落
    for (int y = 0; y < pad; ++y) {
        for (int x = 0; x < pad; ++x) {
            dst[y * paddedWidth + x] = src[0];
            dst[y * paddedWidth + (width + pad + x)] = src[width - 1];
            dst[(height + pad + y) * paddedWidth + x] = src[(height - 1) * width];
            dst[(height + pad + y) * paddedWidth + (width + pad + x)] = src[(height - 1) * width + (width - 1)];
        }
    }
}

// 中值滤波函数
void medianFilter(const uint8_t* src, uint8_t* dst, int width, int height, int ksize) {
    int half_ksize = ksize / 2;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            vector<int> neighborhood;

            for (int ky = -half_ksize; ky <= half_ksize; ++ky) {
                for (int kx = -half_ksize; kx <= half_ksize; ++kx) {
                    int ny = clamp(y + ky, 0, height - 1);
                    int nx = clamp(x + kx, 0, width - 1);
                    neighborhood.push_back(src[ny * width + nx]);
                }
            }

            sort(neighborhood.begin(), neighborhood.end());
            dst[y * width + x] = neighborhood[neighborhood.size() / 2];
        }
    }
}

// 背景提取函数
void getBackground(const uint8_t* src, uint8_t* dst, int width, int height, int win_2) {
    int winsize = 2 * win_2 + 1;
    int pad = win_2;
    int paddedWidth = width + 2 * pad;
    int paddedHeight = height + 2 * pad;
    vector<uint8_t> padded(paddedWidth * paddedHeight, 0);

    // 边缘填充
    padImage(src, padded.data(), width, height, pad);

    // 中值滤波去噪
    medianFilter(padded.data(), padded.data(), paddedWidth, paddedHeight, 9);

    // 背景提取
    for (int y = win_2; y < height + win_2; ++y) {
        for (int x = win_2; x < width + win_2; ++x) {
            vector<int> neighborhood;

            for (int ky = -win_2; ky <= win_2; ++ky) {
                for (int kx = -win_2; kx <= win_2; ++kx) {
                    neighborhood.push_back(padded[(y + ky) * paddedWidth + (x + kx)]);
                }
            }

            sort(neighborhood.begin(), neighborhood.end());
            int sum = 0;
            for (int i = 0; i < 5; ++i) {
                sum += neighborhood[neighborhood.size() - 1 - i];
            }
            dst[(y - win_2) * width + (x - win_2)] = sum / 5;
        }
    }
}

void illuminationCompensation(uint8_t* original_image, const uint8_t* background_image, int width, int height) {
    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {
            int x = original_image[i * width + j];
            int xb = background_image[i * width + j];

            double k = calculateK(xb);

            if (xb > x) {
                if (k * (xb - x) <= 0.75 * 255) {
                    original_image[i * width + j] = static_cast<int>(std::round(0.75 * 255));
                } else {
                    original_image[i * width + j] = 255;
                }
            } else {
                original_image[i * width + j] = 255 - k * (xb - x);
            }
        }
    }
}
void nonLocalMeansDenoising(const uint8_t* src, uint8_t* dst, int width, int height, int searchWindowSize, int blockSize, float h) {
    int halfSearchWindowSize = searchWindowSize / 2;
    int halfBlockSize = blockSize / 2;

    // 将源图像数据扩展为包含边缘填充的图像
    int paddedWidth = width + 2 * halfBlockSize;
    int paddedHeight = height + 2 * halfBlockSize;
    vector<uint8_t> padded(paddedWidth * paddedHeight, 0);

    // 使用padImage进行边缘填充
    padImage(src, padded.data(), width, height, halfBlockSize);

    // 对图像进行去噪
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sumWeights = 0.0;
            float sumValues = 0.0;

            for (int dy = -halfSearchWindowSize; dy <= halfSearchWindowSize; ++dy) {
                for (int dx = -halfSearchWindowSize; dx <= halfSearchWindowSize; ++dx) {
                    float dist = 0.0;

                    for (int by = -halfBlockSize; by <= halfBlockSize; ++by) {
                        for (int bx = -halfBlockSize; bx <= halfBlockSize; ++bx) {
                            int refY = y + by + halfBlockSize;
                            int refX = x + bx + halfBlockSize;
                            int neiY = y + dy + by + halfBlockSize;
                            int neiX = x + dx + bx + halfBlockSize;

                            float diff = padded[refY * paddedWidth + refX] - padded[neiY * paddedWidth + neiX];
                            dist += diff * diff;
                        }
                    }

                    float weight = exp(-dist / (h * h));
                    sumWeights += weight;
                    sumValues += weight * padded[(y + dy + halfBlockSize) * paddedWidth + (x + dx + halfBlockSize)];
                }
            }

            dst[y * width + x] = static_cast<uint8_t>(sumValues / sumWeights);
        }
    }
}

void saveImage(const char* filename, const uint8_t* image, int width, int height) {
    stbi_write_jpg(filename, width, height, 1, image, 100);
}

void gaussianBlur(const uint8_t* src, uint8_t* dst, int width, int height, int ksize) {
    int pad = ksize / 2;
    std::vector<uint8_t> padded((width + 2 * pad) * (height + 2 * pad), 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            padded[(y + pad) * (width + 2 * pad) + (x + pad)] = src[y * width + x];
        }
    }

    for (int y = pad; y < height + pad; ++y) {
        for (int x = pad; x < width + pad; ++x) {
            int sum = 0;
            for (int ky = -pad; ky <= pad; ++ky) {
                for (int kx = -pad; kx <= pad; ++kx) {
                    sum += padded[(y + ky) * (width + 2 * pad) + (x + kx)];
                }
            }
            dst[(y - pad) * width + (x - pad)] = sum / (ksize * ksize);
        }
    }
}

// 差合比处理函数
void differenceOfGaussians(const uint8_t* src, uint8_t* dst, int width, int height, int ksize1, int ksize2) {
    std::vector<uint8_t> blur1(width * height, 0);
    std::vector<uint8_t> blur2(width * height, 0);

    // 进行两次不同尺度的高斯模糊
    gaussianBlur(src, blur1.data(), width, height, ksize1);
    gaussianBlur(src, blur2.data(), width, height, ksize2);

    // 计算两个模糊图像的差值
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int diff = static_cast<int>(blur1[y * width + x]) - static_cast<int>(blur2[y * width + x]);
            dst[y * width + x] = static_cast<uint8_t>(std::clamp(diff + 128, 0, 255)); // 偏移到中间灰度
        }
    }
}

int main() {
    uint8_t image[w * h];
    generate(image, w, h);

    uint8_t* filtered_img = new uint8_t[w * h];
    getBackground(image, filtered_img, w, h, 4);

    saveImage("/home/circle7/Project/digital_image_processing/anti_light/original.jpg", image, w, h);
    saveImage("/home/circle7/Project/digital_image_processing/anti_light/filtered.jpg", filtered_img, w, h);

    // 进行光照补偿
    illuminationCompensation(image, filtered_img, w, h);
    
    // 保存补偿后的图像
    saveImage("/home/circle7/Project/digital_image_processing/anti_light/compensated.jpg", image, w, h);
    uint8_t result[w * h];
    differenceOfGaussians(image, result, w, h, 3, 5);
    saveImage("/home/circle7/Project/digital_image_processing/anti_light/difference.jpg", result, w, h);

    uint8_t denoised_image[w * h];
    nonLocalMeansDenoising(image, denoised_image, w, h, 7, 3, 10.0f);
    saveImage("/home/circle7/Project/digital_image_processing/anti_light/denoised.jpg", denoised_image, w, h);
    delete[] filtered_img;
    return 0;
}
