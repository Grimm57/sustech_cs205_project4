#pragma once
#include <cstddef>
#include <string>
#include <vector>

enum class CVType
{
    CV_8U,  // 8位无符号整数 (uchar)
    CV_8S,  // 8位有符号整数 (schar)
    CV_16U, // 16位无符号整数 (ushort)
    CV_16S, // 16位有符号整数 (short)
    CV_32S, // 32位有符号整数 (int)
    CV_32F, // 32位浮点数 (float)
    CV_64F, // 64位浮点数 (double)
    CV_16F  // 16位浮点数 (半精度)
};

enum class CVChannel
{
    C1 = 1, // 单通道
    C3 = 3, // 三通道
    C4 = 4  // 四通道
};

// CV_MAKETYPE(depth, cn) 将深度和通道数组合成一个类型
constexpr int CV_MAKETYPE(CVType depth, CVChannel cn)
{
    return static_cast<int>(depth) + (static_cast<int>(cn) << 3);
}

// 常用类型定义
constexpr int CV_8UC1 = CV_MAKETYPE(CVType::CV_8U, CVChannel::C1);
constexpr int CV_8UC3 = CV_MAKETYPE(CVType::CV_8U, CVChannel::C3);
constexpr int CV_8UC4 = CV_MAKETYPE(CVType::CV_8U, CVChannel::C4);

constexpr int CV_16UC1 = CV_MAKETYPE(CVType::CV_16U, CVChannel::C1);
constexpr int CV_16UC3 = CV_MAKETYPE(CVType::CV_16U, CVChannel::C3);
constexpr int CV_16UC4 = CV_MAKETYPE(CVType::CV_16U, CVChannel::C4);

constexpr int CV_32FC1 = CV_MAKETYPE(CVType::CV_32F, CVChannel::C1);
constexpr int CV_32FC3 = CV_MAKETYPE(CVType::CV_32F, CVChannel::C3);
constexpr int CV_32FC4 = CV_MAKETYPE(CVType::CV_32F, CVChannel::C4);

constexpr int CV_MAT_DEPTH(int type)
{
    return type & 7;
}

constexpr int CV_MAT_CN(int type)
{
    return (type >> 3) + 1;
}

enum class Error_Type
{
    OUT_OF_MEMORY,
    INSUFFICIENT_DISK_SPACE,
    DIVIDE_BY_ZERO,
    INVALID_ARGUMENT,
    INVALID_OPERATION,
    UNSUPPORTED_FORMAT,
    UNSUPPORTED_OPERATION,
    UNSUPPORTED_IMAGE_SIZE,
    UNSUPPORTED_IMAGE_TYPE,
    UNSUPPORTED_IMAGE_CHANNEL,
    UNSUPPORTED_IMAGE_DEPTH,
    UNSUPPORTED_IMAGE_DATA,
    UNSUPPORTED_IMAGE_DATA_TYPE,
    FILE_NOT_FOUND
};

class Mat
{
public:
    // 构造函数
    Mat();
    Mat(size_t rows, size_t cols, int type); // 使用type替代channels
    Mat(size_t rows, size_t cols, int type, const void *data);

    Mat(const Mat &other); // soft_copy
    Mat clone() const;     // 深拷贝

    Mat(Mat &&other) noexcept; // 移动构造函数

    ~Mat();

    // 赋值运算符
    Mat &operator=(const Mat &other);
    Mat &operator=(Mat &&other) noexcept; // 移动赋值运算符

    // Getters
    size_t rows() const { return m_rows; }
    size_t cols() const { return m_cols; }

    int type() const { return m_type; }
    int channels() const { return CV_MAT_CN(m_type); } // 从type中提取通道数
    int depth() const { return CV_MAT_DEPTH(m_type); } // 从type中提取深度

    // 数据指针
    void *data() { return m_data; }
    const void *data() const { return m_data; }

    // 像素访问
    template <typename T>
    T &at(int row, int col);

    template <typename T>
    const T &at(int row, int col) const;

    // Core functionality
    void create(size_t rows, size_t cols, int type);
    void release();
    bool empty() const;

    // 子矩阵提取
    Mat roi(size_t startRow, size_t startCol, size_t height, size_t width) const;

private:
    size_t m_rows;   // 图像高度
    size_t m_cols;   // 图像宽度
    int m_type;      // 数据类型
    void *m_data;    // 像素数据指针
    int *m_refcount; // 引用计数
};

// 解码器基类
class ImageDecoder
{
public:
    virtual ~ImageDecoder() = default;

    // 检查文件格式是否支持
    virtual bool checkFormat(const std::string &filename) const = 0;

    // 解码图像文件
    virtual Mat decode(const std::string &filename, int flags = 0) const = 0;
};

// BMP 解码器
class BMPDecoder : public ImageDecoder
{
public:
    bool checkFormat(const std::string &filename) const override;
    Mat decode(const std::string &filename, int flags = 0) const override;
};

// PNG 解码器
class PNGDecoder : public ImageDecoder
{
public:
    bool checkFormat(const std::string &filename) const override;
    Mat decode(const std::string &filename, int flags = 0) const override;
};

// JPEG 解码器
class JPEGDecoder : public ImageDecoder
{
public:
    bool checkFormat(const std::string &filename) const override;
    Mat decode(const std::string &filename, int flags = 0) const override;
};

// 查找合适的解码器
ImageDecoder *findDecoder(const std::string &filename);

// 全局函数，用于读取图像文件
Mat imread(const std::string &filename, int flags = 0);

// 全局函数，用于写入图像文件
bool imwrite(const std::string &filename, const Mat &img, const std::vector<int> &params = std::vector<int>());

// 全局函数，用于显示图像
void imshow(const std::string &windowName, const Mat &img);
// 全局函数，用于调整图像亮度
void adjustBrightness(Mat &img, int value);
// 全局函数，用于融合两张图像
void blendImages(const Mat &img1, const Mat &img2, Mat &output, double alpha = 0.5);
// 全局函数，用于显示帮助信息
void showHelp(const std::string &windowName);