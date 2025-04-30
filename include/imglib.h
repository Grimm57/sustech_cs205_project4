#pragma once
#include <cstddef>
#include <string>
#include <vector>

#define CV_8U 0  // 8位无符号整数 (uchar)
#define CV_16U 2 // 16位无符号整数 (ushort)

#define CV_8S 1  // 8位有符号整数 (schar)
#define CV_16S 3 // 16位有符号整数 (short)
#define CV_32S 4 // 32位有符号整数 (int)

#define CV_32F 5 // 32位浮点数 (float)
#define CV_64F 6 // 64位浮点数 (double)
#define CV_16F 7 // 16位浮点数 (半精度)

#define CV_MAKETYPE(depth, cn) ((depth) + ((cn) << 3)) 
#define CV_MAT_DEPTH(type) ((type) & 7)//取低三位
#define CV_MAT_CN(type) (((type) >> 3) + 1)//取高五位

// 8位无符号整型 - 不同通道组合
#define CV_8UC1 CV_MAKETYPE(CV_8U, 1)
#define CV_8UC3 CV_MAKETYPE(CV_8U, 3)
#define CV_8UC4 CV_MAKETYPE(CV_8U, 4)

// 8位有符号整型 - 不同通道组合
#define CV_8SC1 CV_MAKETYPE(CV_8S, 1)
#define CV_8SC3 CV_MAKETYPE(CV_8S, 3)
#define CV_8SC4 CV_MAKETYPE(CV_8S, 4)

// 16位无符号整型 - 不同通道组合
#define CV_16UC1 CV_MAKETYPE(CV_16U, 1)
#define CV_16UC3 CV_MAKETYPE(CV_16U, 3)
#define CV_16UC4 CV_MAKETYPE(CV_16U, 4)

// 16位有符号整型 - 不同通道组合
#define CV_16SC1 CV_MAKETYPE(CV_16S, 1)
#define CV_16SC3 CV_MAKETYPE(CV_16S, 3)
#define CV_16SC4 CV_MAKETYPE(CV_16S, 4)

// 32位有符号整型 - 不同通道组合
#define CV_32SC1 CV_MAKETYPE(CV_32S, 1)
#define CV_32SC3 CV_MAKETYPE(CV_32S, 3)
#define CV_32SC4 CV_MAKETYPE(CV_32S, 4)

// 32位浮点型 - 不同通道组合
#define CV_32FC1 CV_MAKETYPE(CV_32F, 1)
#define CV_32FC3 CV_MAKETYPE(CV_32F, 3)
#define CV_32FC4 CV_MAKETYPE(CV_32F, 4)

// 64位浮点型 - 不同通道组合
#define CV_64FC1 CV_MAKETYPE(CV_64F, 1)
#define CV_64FC3 CV_MAKETYPE(CV_64F, 3)
#define CV_64FC4 CV_MAKETYPE(CV_64F, 4)

// enum class Error_Type
// {
//     OUT_OF_MEMORY,
//     INSUFFICIENT_DISK_SPACE,
//     DIVIDE_BY_ZERO,
//     INVALID_ARGUMENT,
//     INVALID_OPERATION,
//     UNSUPPORTED_FORMAT,
//     UNSUPPORTED_OPERATION,
//     UNSUPPORTED_IMAGE_SIZE,
//     UNSUPPORTED_IMAGE_TYPE,
//     UNSUPPORTED_IMAGE_CHANNEL,
//     UNSUPPORTED_IMAGE_DEPTH,
//     UNSUPPORTED_IMAGE_DATA,
//     UNSUPPORTED_IMAGE_DATA_TYPE,
//     FILE_NOT_FOUND
// };

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

    virtual bool checkFormat(const std::string &filename) const = 0;

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