#include "imglib.h"
#include <cstring>
#include <stdexcept>
#include <iostream>

// 默认构造函数
Mat::Mat() : m_rows(0), m_cols(0), m_type(0), m_data(nullptr), m_refcount(nullptr) {}

// 指定大小和类型的构造函数
Mat::Mat(size_t rows, size_t cols, int type) : m_rows(rows), m_cols(cols), m_type(type), m_data(nullptr), m_refcount(nullptr)
{
    create(rows, cols, type);
}

// 指定大小、类型和数据的构造函数
Mat::Mat(size_t rows, size_t cols, int type, const void *data) : Mat(rows, cols, type)
{
    if (data)
    {
        size_t bytesPerChannel = 0;

        // 根据深度类型计算每通道的字节大小
        switch (CV_MAT_DEPTH(type))
        {
        case CV_8U: // 8位无符号整数
        case CV_8S: // 8位有符号整数
            bytesPerChannel = 1;
            break;
        case CV_16U: // 16位无符号整数
        case CV_16S: // 16位有符号整数
        case CV_16F: // 16位浮点数（半精度）
            bytesPerChannel = 2;
            break;
        case CV_32S: // 32位有符号整数
        case CV_32F: // 32位浮点数
            bytesPerChannel = 4;
            break;
        case CV_64F: // 64位浮点数
            bytesPerChannel = 8;
            break;
        default:
            throw std::invalid_argument("Unsupported depth type");
        }

        // 计算数据大小
        size_t dataSize = rows * cols * CV_MAT_CN(type) * bytesPerChannel;
        // 将传入的数据复制到 m_data
        std::memcpy(m_data, data, dataSize);
    }
}

// 拷贝构造函数
Mat::Mat(const Mat &other) : m_rows(other.m_rows), m_cols(other.m_cols), m_type(other.m_type), m_data(other.m_data), m_refcount(other.m_refcount)
{
    if (m_refcount)
    {
        ++(*m_refcount);
    }
}

// 移动构造函数
Mat::Mat(Mat &&other) noexcept : m_rows(other.m_rows), m_cols(other.m_cols), m_type(other.m_type), m_data(other.m_data), m_refcount(other.m_refcount)
{
    other.m_data = nullptr;
    other.m_refcount = nullptr;
}

// 析构函数
Mat::~Mat()
{
    release();
}

// 赋值运算符
Mat &Mat::operator=(const Mat &other)
{
    if (this != &other)
    {
        release();
        m_rows = other.m_rows;
        m_cols = other.m_cols;
        m_type = other.m_type;
        m_data = other.m_data;
        m_refcount = other.m_refcount;
        if (m_refcount)
        {
            ++(*m_refcount);
        }
    }
    return *this;
}

// 移动赋值运算符
Mat &Mat::operator=(Mat &&other) noexcept
{
    if (this != &other)
    {
        release();
        m_rows = other.m_rows;
        m_cols = other.m_cols;
        m_type = other.m_type;
        m_data = other.m_data;
        m_refcount = other.m_refcount;
        other.m_data = nullptr;
        other.m_refcount = nullptr;
    }
    return *this;
}

// 创建矩阵
void Mat::create(size_t rows, size_t cols, int type)
{
    release();
    m_rows = rows;
    m_cols = cols;
    m_type = type;
    size_t dataSize = rows * cols * CV_MAT_CN(type) * (CV_MAT_DEPTH(type) == CV_8U ? 1 : 2); // 简单假设
    m_data = new unsigned char[dataSize];
    m_refcount = new int(1);
}

// 释放矩阵
void Mat::release()
{
    if (m_refcount && --(*m_refcount) == 0)
    {
        delete[] static_cast<unsigned char *>(m_data);
        delete m_refcount;
    }
    m_data = nullptr;
    m_refcount = nullptr;
}

// 检查矩阵是否为空
bool Mat::empty() const
{
    return m_data == nullptr;
}

// 提取子矩阵
Mat Mat::roi(size_t startRow, size_t startCol, size_t height, size_t width) const
{
    if (startRow + height > m_rows || startCol + width > m_cols)
    {
        throw std::out_of_range("ROI out of bounds");
    }
    Mat subMat(height, width, m_type);
    for (size_t i = 0; i < height; ++i)
    {
        std::memcpy(static_cast<unsigned char *>(subMat.m_data) + i * width * CV_MAT_CN(m_type),
                    static_cast<unsigned char *>(m_data) + ((startRow + i) * m_cols + startCol) * CV_MAT_CN(m_type),
                    width * CV_MAT_CN(m_type));
    }
    return subMat;
}

template <typename T>
const T &Mat::at(int row, int col) const
{
    if (row < 0 || row >= static_cast<int>(m_rows) || col < 0 || col >= static_cast<int>(m_cols))
    {
        throw std::out_of_range("矩阵访问越界");
    }
    size_t channels = CV_MAT_CN(m_type);       // 获取通道数
    size_t elementSize = sizeof(T) * channels; // 每个元素的字节大小
    return *reinterpret_cast<const T *>(static_cast<const unsigned char *>(m_data) + (row * m_cols + col) * elementSize);
}