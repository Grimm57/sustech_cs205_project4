#include "../include/imglib.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdint>
#include <fstream>
#include <vector>
#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <cassert>

namespace img
{

/**
 * @brief 显式释放图像数据。
 * 减少共享数据的引用计数。如果引用计数降为零，底层内存将被释放。
 * 然后将当前 Image 对象重置为空状态。
 */
void Image::release()
{
    // 如果refcount是null的话说明没有实际数据只有个文件头(默认构造函数或者是一个已经被release过的对象或是被移动所有权的对象)
    // 直接置空就好
    // 否则需要减小refcount或是释放内存
    if (m_refcount)
    {
        (*m_refcount)--;
        if (*m_refcount == 0)
        {
            delete[] m_datastorage; // 对应allocate中的 new unsigned char[total_size]
        }
        // 这个image不再指向任何引用次数
        m_refcount = nullptr;
    }
    m_rows = 0;
    m_cols = 0;
    m_type = -1;
    m_channel_size = 0;
    m_step = 0;
    m_data_start = nullptr;
    m_datastorage = nullptr;
    // 初始化会默认构造的状态
}

/**
 * @brief 内部辅助函数：分配内存块（包括引用计数区域）
 * @throw std::runtime_error 如果内存分配失败。
 * @throw std::invalid_argument 如果图像类型无效或维度为零导致无法计算有效的内存大小。
 * @throw std::overflow_error 计算发生溢出
 */
void Image::allocate(size_t rows, size_t cols, int type)
{

    // 防止用户错误输入负数的type导致整个宏的计算系统崩溃
    if (type < 0)
    {
        throw std::invalid_argument("图像类型不可以为负数。");
    }
    // && 优先级大于 ||
    // 处理行数或列数单独为0的情况
    // 不需要检查 rows 和 cols 是否为负数，因为它们是 size_t 类型
    if (cols > 0 && rows == 0 || cols == 0 && rows > 0)
    {
        throw std::invalid_argument("图像的行数或列数不可以单独为0。");
    }

    // depth 和 channel_cnt 的元组与type一一对应,所以可以通过这两个值来判断type是否合法
    int depth = IMG_DEPTH(type);
    int channel_cnt = IMG_CN(type);
    size_t channel_size = 0; // 每个通道的字节数

    switch (depth)
    {
    case IMG_8U:
        channel_size = 1;
        break;
    case IMG_16U:
        channel_size = 2;
        break;
    case IMG_32S:
        channel_size = 4;
        break;
    case IMG_32F:
        channel_size = 4;
        break;
    case IMG_64F:
        channel_size = 8;
        break;
    default:
        throw std::invalid_argument("不支持的图像深度类型。");
    }

    // 通道数必须有效
    if (channel_cnt != 1 && channel_cnt != 3 && channel_cnt != 4)
    {
        throw std::invalid_argument("不支持的图像通道数。");
    }

    size_t pixel_size = channel_size * channel_cnt; // 每个像素的字节大小

    size_t step_length = 0;            // 每行的字节数
    size_t current_data_byte_size = 0; // 当前数据的字节大小

    if (rows > 0 && cols > 0)
    {
        if (pixel_size != 0 && cols > std::numeric_limits<size_t>::max() / pixel_size)
        {
            throw std::overflow_error("计算时发生溢出。");
        }
        step_length = cols * pixel_size;

        if (step_length != 0 && rows > std::numeric_limits<size_t>::max() / step_length)
        {
            throw std::overflow_error("计算时发生溢出。");
        }
        current_data_byte_size = rows * step_length;
    }
    // else
    // {
    //     // rows == 0 且 cols == 0,单一一个为零的情况已经在最开始就被排除了
    //     // current_data_byte_size = 0;
    // }

    size_t size_for_refcount = sizeof(int);
    size_t total_alloc_size;

    if (current_data_byte_size > std::numeric_limits<size_t>::max() - size_for_refcount)
    {
        throw std::overflow_error("计算时发生溢出。");
    }
    total_alloc_size = size_for_refcount + current_data_byte_size;
    unsigned char *new_datastorage_ptr = nullptr;
    try
    {
        new_datastorage_ptr = new unsigned char[total_alloc_size];
    }
    catch (const std::bad_alloc &e)
    {
        // 回复到空状态
        this->release();

        throw std::runtime_error(
            std::string("内存分配失败。尝试分配的大小: ") +
            std::to_string(total_alloc_size) + " 字节。错误信息: " + e.what());
    }

    m_datastorage = new_datastorage_ptr;

    if (m_datastorage)
    { // 仅当实际分配了内存时才设置引用计数和数据指针
        m_refcount = reinterpret_cast<int *>(m_datastorage);
        *m_refcount = 1;
        m_data_start = m_datastorage + size_for_refcount;
    }
    else
    {
        // 图像为empty
        m_refcount = nullptr;
        m_data_start = nullptr;
    }

    m_rows = rows;
    m_cols = cols;
    m_type = type;
    m_channel_size = static_cast<unsigned char>(channel_size);
    m_step = step_length;
}

/** @brief 默认构造函数。创建一个空的 Image 对象。 */
Image::Image()
    : m_rows(0),
      m_cols(0),
      m_type(-1),
      m_channel_size(0),
      m_step(0),
      m_data_start(nullptr),
      m_datastorage(nullptr),
      m_refcount(nullptr)
{
} // 空构造函数不会分配内存,可以使用create来修改图像的内存分配

/**
 * @brief 根据指定的维度和类型构造图像。
 * @param rows 图像的行数。
 * @param cols 图像的列数。
 * @param type 图像类型 (例如 IMG_8UC3, IMG_32FC1)。
 * @throw std::runtime_error allocate() 抛出的异常。
 */
Image::Image(size_t rows, size_t cols, int type)
    : m_rows(rows),
      m_cols(cols),
      m_type(type),
      m_channel_size(0),
      m_step(0),
      m_data_start(nullptr),
      m_datastorage(nullptr),
      m_refcount(nullptr)
{
    try
    {
        this->allocate(rows, cols, type);
    }
    catch (const std::exception &e)
    {
        // 捕获 allocate 抛出的异常并添加上下文信息
        throw std::runtime_error(
            std::string("创建图像失败 (rows: ") + std::to_string(rows) +
            ", cols: " + std::to_string(cols) +
            ", type: " + std::to_string(type) +
            "). 错误信息: " + e.what());
        // 抛出上层函数异常
    }
}

/**
 * @brief 软拷贝函数，增加ref_count。
 * @param other 源 Image 对象。
 */
Image::Image(const Image &other)
    : m_rows(other.m_rows),
      m_cols(other.m_cols),
      m_type(other.m_type),
      m_channel_size(other.m_channel_size),
      m_step(other.m_step),
      m_data_start(other.m_data_start),
      m_datastorage(other.m_datastorage),
      m_refcount(other.m_refcount)
{
    if (other.empty())
    {
        std::cerr << "警告: 无意义行为--软拷贝构造的源图像为空,可以直接使用默认构造函数 \n";
    }
    if (other.m_refcount) // 理论上一定为真
    {
        (*other.m_refcount)++;
    }
}

/**
 * @brief 移动构造函数。
 * 从源对象 `other` 接管资源，并将 `other` 置为空状态。
 * 不涉及数据拷贝和引用计数的更改
 * @param other 源 Image 对象 (将被修改为空)。
 */
Image::Image(Image &&other)
    : m_rows(other.m_rows),
      m_cols(other.m_cols),
      m_type(other.m_type),
      m_channel_size(other.m_channel_size),
      m_step(other.m_step),
      m_data_start(other.m_data_start),
      m_datastorage(other.m_datastorage),
      m_refcount(other.m_refcount)
{
    if (other.empty())
    {
        std::cerr << "警告: 无意义行为--移动接管构造的源图像为空 \n";
    }
    other.m_rows = 0;
    other.m_cols = 0;
    other.m_type = -1;
    other.m_channel_size = 0;
    other.m_step = 0;
    other.m_data_start = nullptr;
    other.m_datastorage = nullptr;
    other.m_refcount = nullptr;
}

/**
 * @brief 析构函数。
 * 减少底层数据的引用计数,同时清空信息头。如果引用计数降为零，则释放内存。
 */
Image::~Image()
{
    this->release();
}
/**
 * @brief 赋值运算符(软拷贝)。
 * 使当前对象共享 `other` 的数据。
 * 如果被赋值对象之前拥有数据，会先减少其引用计数（可能释放）。
 * 然后增加 `other` 指向数据的引用计数。
 * 允许把图像赋值为空图像
 * @param other 源 Image 对象。
 * @return 对当前对象的引用。
 */
Image &Image::operator=(const Image &other)
{
    if (this == &other)
    {
        return *this;
    }
    this->release();
    m_rows = other.m_rows;
    m_cols = other.m_cols;
    m_type = other.m_type;
    m_channel_size = other.m_channel_size;
    m_step = other.m_step;
    m_data_start = other.m_data_start;
    m_datastorage = other.m_datastorage;
    m_refcount = other.m_refcount;
    if (m_refcount)
    {
        (*m_refcount)++;
    }
    return *this; // 返回this所指向的对象,也就是图片自身的一个引用
}
/**
 * @brief 移动赋值运算符。
 * 从 `other` 接管资源，并将 `other` 置为空状态。
 * 如果当前对象之前拥有数据，会先释放。
 * @param other 源 Image 对象 (将被修改为空)。
 * @return 对当前对象的引用。
 */
Image &Image::operator=(Image &&other)
{
    if (this == &other)
    {
        return *this;
    }
    this->release();
    m_rows = other.m_rows;
    m_cols = other.m_cols;
    m_type = other.m_type;
    m_channel_size = other.m_channel_size;
    m_step = other.m_step;
    m_data_start = other.m_data_start;
    m_datastorage = other.m_datastorage;
    m_refcount = other.m_refcount;
    // 把other置空
    other.m_rows = 0;
    other.m_cols = 0;
    other.m_type = -1;
    other.m_channel_size = 0;
    other.m_step = 0;
    other.m_data_start = nullptr;
    other.m_datastorage = nullptr;
    other.m_refcount = nullptr;
    return *this;
}

/**
 * @brief 创建图像的深拷贝（克隆）。
 * 分配新的内存（包括新的引用计数），并将当前图像的像素数据完整复制过去。
 * 返回的新 Image 对象拥有独立的数据副本，其引用计数初始化为1。
 * 声明为const,因为clone不会改变原来的对象
 * @return 一个包含独立数据的新 Image 对象。
 * @throw std::logic_error 如果原始图像为空，无法克隆有效数据 (尽管技术上可以克隆一个空图像)。
 * @throw std::runtime_error allocate() 抛出的异常。
 */
Image Image::clone() const
{

    // if (this->empty())
    // {
    //     throw std::logic_error("无法克隆空图像。");
    // }
    // 如果是空图像,在allocate中会抛出异常
    // 异常会由上层构造函数捕获并且抛出整理后的信息
    Image new_image = Image(m_rows, m_cols, m_type);
    std::memcpy(new_image.m_data_start, m_data_start, m_rows * m_step);

    return new_image;
}

/**
 * @brief 这个函数的作用是给之前设定的图像重新分配内存
 * 如果 `rows`, `cols`, `type` 都与当前对象相同，则不执行任何操作。
 * 否则，先调用 `release()` 释放当前可能持有的资源，然后分配新空间。
 * @param rows 新的行数。
 * @param cols 新的列数。
 * @param type 新的图像类型。
 * @throw std::runtime_error allocate() 抛出的异常。
 */
void Image::create(size_t rows, size_t cols, int type)
{
    if (m_rows == rows && m_cols == cols && m_type == type)
    {
        return;
    }
    release();
    try
    {
        this->allocate(rows, cols, type);
    }
    catch (const std::exception &e)
    {
        // 捕获 allocate 抛出的异常并添加上下文信息
        throw std::runtime_error(
            std::string("创建图像失败 (rows: ") + std::to_string(rows) +
            ", cols: " + std::to_string(cols) +
            ", type: " + std::to_string(type) +
            "). 错误信息: " + e.what());
    }
}

/**
 * @brief 返回指向指定行的起始位置的指针。
 * 非const用于可能的修改行数据的情况
 * @param r 行索引 (从0开始)。
 * @return 指向第 `r` 行起始像素的 `unsigned char*` 指针。
 * @throw std::out_of_range 如果行索引 r 超出边界。
 * @throw std::logic_error 如果图像为空，无法获取行指针。
 */
unsigned char *Image::ptr(int r)
{

    if (static_cast<size_t>(r) >= m_rows || r < 0)
    {
        throw std::out_of_range("行索引超出范围。");
    }
    if (this->empty())
    {
        throw std::logic_error("无法获取空图像的行指针。");
    }

    return m_data_start + r * m_step;
}
/**
 * @brief 返回指向指定行的起始位置的常量指针。
 * @param r 行索引 (从0开始)。
 * @return 指向第 `r` 行起始像素的 `const unsigned char*` 指针。
 * @throw std::out_of_range 如果行索引 r 超出边界。
 * @throw std::logic_error 如果图像为空，无法获取行指针。
 */
const unsigned char *Image::ptr(int r) const
{

    if (static_cast<size_t>(r) >= m_rows || r < 0)
    {
        throw std::out_of_range("行索引超出范围。");
    }
    if (empty())
    {
        throw std::logic_error("无法获取空图像的行指针。");
    }
    return m_data_start + r * m_step;
}

// 辅助宏
#define IMAGE_SCALAR_OP_LOOP(OP, TYPE, CLAMP_FUNC)                                \
    for (size_t r = 0; r < m_rows; ++r)                                           \
    {                                                                             \
        unsigned char *row_ptr = this->ptr(r);                                    \
        for (size_t c = 0; c < m_cols; ++c)                                       \
        {                                                                         \
            TYPE *pixel_ptr = reinterpret_cast<TYPE *>(row_ptr + c * pixel_size); \
            for (int chan = 0; chan < channel_cnt; ++chan)                        \
            {                                                                     \
                double result = static_cast<double>(pixel_ptr[chan]) OP scalar_d; \
                pixel_ptr[chan] = CLAMP_FUNC(result);                             \
            }                                                                     \
        }                                                                         \
    }

// 这一个模板的核心功能是把double类型的值转换成对应的图像的数值类型
template <typename T>
T clamp_value(double value)
{
    return static_cast<T>(value);
}

template <>
unsigned char clamp_value<unsigned char>(double value)
{
    value = std::round(value);
    return static_cast<unsigned char>(
        std::max(0.0, std::min(255.0, value)));
}

template <>
unsigned short clamp_value<unsigned short>(double value)
{
    value = std::round(value);
    return static_cast<unsigned short>(
        std::max(0.0, std::min(65535.0, value)));
}

// int的大小在不同平台上可能会有所不同,所以这里使用了std::numeric_limits来获取int的最大值和最小值
template <>
int clamp_value<int>(double value)
{
    value = std::round(value);
    return static_cast<int>(
        std::max(static_cast<double>(std::numeric_limits<int>::min()),
                 std::min(static_cast<double>(std::numeric_limits<int>::max()), value)));
}
/**
 * @brief 图像与标量的加法赋值 (逐像素)。
 * @throw std::logic_error 如果图像为空。
 * @throw std::invalid_argument 如果图像类型不支持此算术操作。
 */
Image &Image::operator+=(double scalar)
{
    if (empty())
        throw std::logic_error("图像为空。");

    int depth = get_depth();
    int channel_cnt = get_channels();
    size_t pixel_size = get_pixel_size();
    double scalar_d = scalar;

    switch (depth)
    {
    case IMG_8U:
        IMAGE_SCALAR_OP_LOOP(+, unsigned char, clamp_value<unsigned char>);
        break;
    case IMG_16U:
        IMAGE_SCALAR_OP_LOOP(+, unsigned short, clamp_value<unsigned short>);
        break;
    case IMG_32S:
        IMAGE_SCALAR_OP_LOOP(+, int, clamp_value<int>);
        break;
    case IMG_32F:
        IMAGE_SCALAR_OP_LOOP(+, float, clamp_value<float>);
        break;
    case IMG_64F:
        IMAGE_SCALAR_OP_LOOP(+, double, clamp_value<double>);
        break;
    default:
        throw std::invalid_argument("不支持的图像类型。");
    }
    return *this;
}
/**
 * @brief 图像与标量的减法赋值 (逐像素)。
 * @throw std::logic_error 如果图像为空。
 * @throw std::invalid_argument 如果图像类型不支持此算术操作。
 */
Image &Image::operator-=(double scalar)
{
    if (empty())
        throw std::logic_error("图像为空。");

    int depth = get_depth();
    int channel_cnt = get_channels();
    size_t pixel_size = get_pixel_size();
    double scalar_d = scalar;

    switch (depth)
    {
    case IMG_8U:
        IMAGE_SCALAR_OP_LOOP(-, unsigned char, clamp_value<unsigned char>);
        break;
    case IMG_16U:
        IMAGE_SCALAR_OP_LOOP(-, unsigned short, clamp_value<unsigned short>);
        break;
    case IMG_32S:
        IMAGE_SCALAR_OP_LOOP(-, int, clamp_value<int>);
        break;
    case IMG_32F:
        IMAGE_SCALAR_OP_LOOP(-, float, clamp_value<float>);
        break;
    case IMG_64F:
        IMAGE_SCALAR_OP_LOOP(-, double, clamp_value<double>);
        break;
    default:
        throw std::runtime_error("不支持的图像类型。");
    }
    return *this;
}
/**
 * @brief 图像与标量的乘法赋值 (逐像素)。
 * @throw std::logic_error 如果图像为空。
 * @throw std::invalid_argument 如果图像类型不支持此算术操作。
 */
Image &Image::operator*=(double scalar)
{
    if (empty())
        throw std::logic_error("图像为空。");

    int depth = get_depth();
    int channel_cnt = get_channels();
    size_t pixel_size = get_pixel_size();
    double scalar_d = scalar;

    switch (depth)
    {
    case IMG_8U:
        IMAGE_SCALAR_OP_LOOP(*, unsigned char, clamp_value<unsigned char>);
        break;
    case IMG_16U:
        IMAGE_SCALAR_OP_LOOP(*, unsigned short, clamp_value<unsigned short>);
        break;
    case IMG_32S:
        IMAGE_SCALAR_OP_LOOP(*, int, clamp_value<int>);
        break;
    case IMG_32F:
        IMAGE_SCALAR_OP_LOOP(*, float, clamp_value<float>);
        break;
    case IMG_64F:
        IMAGE_SCALAR_OP_LOOP(*, double, clamp_value<double>);
        break;
    default:
        throw std::runtime_error("不支持的图像类型。");
    }
    return *this;
}
/**
 * @brief 图像与标量的除法赋值 (逐像素)。
 * @throw std::logic_error 如果图像为空。
 * @throw std::runtime_error 除零检查。
 * @throw std::invalid_argument 如果图像类型不支持此算术操作。
 */
Image &Image::operator/=(double scalar)
{
    if (empty())
        throw std::logic_error("图像为空。");
    if (scalar == 0.0)
        throw std::runtime_error("检测到除以零。");

    int depth = get_depth();
    int channel_cnt = get_channels();
    size_t pixel_size = get_pixel_size();
    double scalar_d = scalar;

    switch (depth)
    {
    case IMG_8U:
        IMAGE_SCALAR_OP_LOOP(/, unsigned char, clamp_value<unsigned char>);
        break;
    case IMG_16U:
        IMAGE_SCALAR_OP_LOOP(/, unsigned short, clamp_value<unsigned short>);
        break;
    case IMG_32S:
        IMAGE_SCALAR_OP_LOOP(/, int, clamp_value<int>);
        break;
    case IMG_32F:
        IMAGE_SCALAR_OP_LOOP(/, float, clamp_value<float>);
        break;
    case IMG_64F:
        IMAGE_SCALAR_OP_LOOP(/, double, clamp_value<double>);
        break;
    default:
        throw std::runtime_error("不支持的图像类型。");
    }
    return *this;
}

Image Image::operator+(double scalar)
{
    Image result = this->clone();
    result += scalar;
    return result;
}

Image Image::operator-(double scalar)
{
    Image result = this->clone();
    result -= scalar;
    return result;
}

Image Image::operator*(double scalar)
{
    Image result = this->clone();
    result *= scalar;
    return result;
}

Image Image::operator/(double scalar)
{
    Image result = this->clone();
    result /= scalar;
    return result;
}

/// 友元函数
Image operator+(double scalar, const Image &img)
{
    Image result = img.clone();
    result += scalar;
    return result;
}

Image operator*(double scalar, const Image &img)
{
    Image result = img.clone();
    result *= scalar;
    return result;
}

#define IMAGE_IMAGE_OP_LOOP(OP, TYPE, CLAMP_FUNC)                                                                        \
    for (size_t r = 0; r < m_rows; ++r)                                                                                  \
    {                                                                                                                    \
        unsigned char *row_ptr_this = this->ptr(r);                                                                      \
        const unsigned char *row_ptr_other = other.ptr(r);                                                               \
        for (size_t c = 0; c < m_cols; ++c)                                                                              \
        {                                                                                                                \
            TYPE *pixel_ptr_this = reinterpret_cast<TYPE *>(row_ptr_this + c * pixel_size);                              \
            const TYPE *pixel_ptr_other = reinterpret_cast<const TYPE *>(row_ptr_other + c * pixel_size);                \
            for (int chan = 0; chan < channel_cnt; ++chan)                                                               \
            {                                                                                                            \
                double result = static_cast<double>(pixel_ptr_this[chan]) OP static_cast<double>(pixel_ptr_other[chan]); \
                pixel_ptr_this[chan] = CLAMP_FUNC(result);                                                               \
            }                                                                                                            \
        }                                                                                                                \
    }

/**
 * @brief 静态函数,用于检查两个图像是否兼容进行逐元素操作。
 * 兼容条件：两者都非空，具有相同的行数、列数和类型。
 * @param img1 第一个图像。
 * @param img2 第二个图像。
 * @throw std::logic_error 如果任一图像为空。
 * @throw std::runtime_error 如果图像尺寸或类型不匹配。
 */
void Image::check_compatibility(const Image &img1, const Image &img2)
{
    if (img1.empty() || img2.empty())
    {
        throw std::logic_error("check_compatibility - 输入图像不能为空。");
    }
    if (img1.get_rows() != img2.get_rows() || img1.get_cols() != img2.get_cols())
    {
        throw std::runtime_error("check_compatibility - 图像尺寸不匹配。");
    }
    if (img1.get_type() != img2.get_type())
    {
        throw std::runtime_error("check_compatibility - 图像类型不匹配。");
    }
}
/**
 * @brief 图像与图像的复合加法赋值 (逐元素)。
 * 要求两个图像具有相同的尺寸和类型。
 * @param other 要加到当前图像上的另一个图像。
 * @throws std::runtime_error 如果图像不兼容或图像类型有误。
 */
Image &Image::operator+=(const Image &other)
{
    try
    {
        check_compatibility(*this, other);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(std::string("Image::operator+= (Image) - 图像兼容性检查失败: ") + e.what());
    }

    int depth = get_depth();
    int channel_cnt = get_channels();
    size_t pixel_size = get_pixel_size();

    switch (depth)
    {
    case IMG_8U:
        IMAGE_IMAGE_OP_LOOP(+, unsigned char, clamp_value<unsigned char>);
        break;
    case IMG_16U:
        IMAGE_IMAGE_OP_LOOP(+, unsigned short, clamp_value<unsigned short>);
        break;
    case IMG_32S:
        IMAGE_IMAGE_OP_LOOP(+, int, clamp_value<int>);
        break;
    case IMG_32F:
        IMAGE_IMAGE_OP_LOOP(+, float, clamp_value<float>);
        break;
    case IMG_64F:
        IMAGE_IMAGE_OP_LOOP(+, double, clamp_value<double>);
        break;
    default:
        throw std::runtime_error("Image::operator+= (Image) - 不支持的图像类型。");
    }
    return *this;
}
/**
 * @brief 图像与图像的复合减法赋值 (逐元素)。
 * 要求两个图像具有相同的尺寸和类型。
 * @param other 从当前图像减去的另一个图像。
 * @throws std::runtime_error 如果图像不兼容或图像类型有误。
 */
Image &Image::operator-=(const Image &other)
{
    try
    {
        check_compatibility(*this, other);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(std::string("Image::operator-= (Image) - 图像兼容性检查失败: ") + e.what());
    }

    int depth = get_depth();
    int channel_cnt = get_channels();
    size_t pixel_size = get_pixel_size();

    switch (depth)
    {
    case IMG_8U:
        IMAGE_IMAGE_OP_LOOP(-, unsigned char, clamp_value<unsigned char>);
        break;
    case IMG_16U:
        IMAGE_IMAGE_OP_LOOP(-, unsigned short, clamp_value<unsigned short>);
        break;
    case IMG_32S:
        IMAGE_IMAGE_OP_LOOP(-, int, clamp_value<int>);
        break;
    case IMG_32F:
        IMAGE_IMAGE_OP_LOOP(-, float, clamp_value<float>);
        break;
    case IMG_64F:
        IMAGE_IMAGE_OP_LOOP(-, double, clamp_value<double>);
        break;
    default:
        throw std::runtime_error("Image::operator-= (Image) - 不支持的图像类型。");
    }
    return *this;
}

/**
 * @brief 创建一个表示当前图像感兴趣区域（ROI）的新 Image 对象（视图）。
 * 这个新的 Image 对象与原始图像共享底层的像素数据和引用计数。
 * 对ROI视图的修改会直接反映在原始图像的对应区域。
 * ROI视图的 m_step 与原始图像的 m_step 相同。(因为要跳到下一行的同一个位置)
 * @param x ROI左上角在当前图像中的列索引（0-based）。
 * @param y ROI左上角在当前图像中的行索引（0-based）。
 * @param width ROI的宽度（列数）。
 * @param height ROI的高度（行数）。
 * @return 一个新的 Image 对象，它是原始图像指定区域的一个视图。
 * @throw std::logic_error 如果原始图像为空。
 * @throw std::out_of_range 如果ROI参数 (start_x, start_y, width, height) 超出原始图像边界或形成无效区域。
 */
Image Image::roi(size_t x, size_t y, size_t width, size_t height) const
{
    if (this->empty())
    {
        throw std::logic_error("Image::roi - 无法从空图像创建 ROI。");
    }
    if (x >= m_cols || y >= m_rows ||
        width == 0 || height == 0 ||
        x + width > m_cols || y + height > m_rows)
    {
        std::string error_msg = "Image::roi - ROI 参数越界或无效。";
        error_msg += "原始图像 (行, 列): (" + std::to_string(m_rows) + ", " + std::to_string(m_cols) + ")。";
        error_msg += "ROI (x, y, 宽度, 高度): (" + std::to_string(x) + ", " + std::to_string(y) + ", " +
                     std::to_string(width) + ", " + std::to_string(height) + ")。";
        throw std::out_of_range(error_msg);
    }

    Image roi_view; // 调用默认构造函数,现在图像是空的
    // roi与原图像共享数据
    roi_view.m_datastorage = this->m_datastorage;
    roi_view.m_refcount = this->m_refcount;
    if (roi_view.m_refcount)
    {
        (*roi_view.m_refcount)++;
    }
    else
    {
        // 理论上讲不应该发生,因为this假如是非空图像就一定有引用计数,为了防止潜在的异常,这里做了一个检查
        throw std::logic_error("Image::roi - 源图像引用计数状态不一致。");
    }
    roi_view.m_data_start = this->m_data_start + (y * this->m_step) + (x * this->get_pixel_size());

    roi_view.m_rows = height;
    roi_view.m_cols = width;
    roi_view.m_type = this->m_type;
    roi_view.m_channel_size = this->m_channel_size;

    roi_view.m_step = this->m_step;

    return roi_view;
}

/**
 * @brief 将图像转换为另一种数据类型，并可选择进行缩放和移位。
 * 创建一个具有指定 `new_type` 的新图像。像素值根据以下公式进行转换：
 * \f[ \text{dst}(I) = \text{saturate\_cast<new\_type>} (\text{src}(I) \cdot \text{scale} + \text{shift}) \f]
 * 其中 saturate_cast<new_type> 表示转换为 new_type 并进行饱和处理（例如，对于 IMG_8U，值被截断到 [0, 255] 范围）。
 *
 * @param new_type 新图像的目标数据类型 (例如, IMG_8UC1, IMG_32FC3)。
 *                 `new_type` 中的通道数必须与源图像匹配。
 * @param scale 可选的缩放因子，在类型转换前应用于像素值。默认为 1.0。
 * @param shift 可选的偏移量，在缩放后、类型转换前加到像素值上。默认为 0.0。
 * @return 一个包含转换后数据和类型的新 Image 对象。
 * @throw std::logic_error 如果源图像为空，或者源图像与 `new_type` 指定的通道数不匹配。
 * @throw std::runtime_error 如果 `new_type` 无效 (例如，不支持的深度，此错误由 Image 构造函数通过 allocate 抛出并被包装)，
 *                           或者为新图像分配内存失败 (同样由 Image 构造函数传播)。
 */
Image Image::convert_to(int new_type, double scale, double shift) const
{
    if (this->empty())
    {
        throw std::logic_error("Image::convertTo - 无法转换空图像。");
    }

    int src_channels = this->get_channels();
    int dst_channels_from_new_type = IMG_CN(new_type);

    if (src_channels != dst_channels_from_new_type)
    {
        throw std::logic_error("Image::convertTo - 源类型和目标类型的通道数不匹配。"
                               "源通道数: " +
                               std::to_string(src_channels) +
                               ", 请求的目标通道数: " + std::to_string(dst_channels_from_new_type));
    }

    // 优化路径：如果源类型和目标类型相同
    if (this->get_type() == new_type)
    {
        Image dst_image = this->clone(); // 深拷贝，dst_image 类型与 this 相同
        // 仅当 scale/shift 非默认值时才应用操作
        if (scale != 1.0)
        {
            dst_image *= scale; // 使用已实现的 operator*=
        }
        if (shift != 0.0)
        {
            dst_image += shift; // 使用已实现的 operator+=
        }
        return dst_image;
    }

    // 通用路径：类型不同，或者作为回退（尽管上面已处理相同类型）
    // 创建目标图像。Image 构造函数将通过 allocate 验证 new_type (包括深度) 并处理分配错误。
    Image dst_image(m_rows, m_cols, new_type); // 可能抛出 std::runtime_error (来自构造函数对 allocate 的调用)

    int src_depth = this->get_depth();
    int dst_depth = IMG_DEPTH(new_type); // dst_image.get_depth() 也可以
    // num_channels 已在前面获取为 src_channels，且已验证与 dst_channels_from_new_type 相等

    unsigned char src_channel_byte_size = this->m_channel_size;
    unsigned char dst_channel_byte_size = dst_image.m_channel_size; // 从新创建的 dst_image 获取

    for (size_t r = 0; r < m_rows; ++r)
    {
        const unsigned char *src_row_ptr = this->ptr(r);
        unsigned char *dst_row_ptr = dst_image.ptr(r);

        for (size_t c = 0; c < m_cols; ++c)
        {
            for (int ch = 0; ch < src_channels; ++ch)
            {
                const unsigned char *p_src_channel = src_row_ptr + c * this->get_pixel_size() + ch * src_channel_byte_size;
                unsigned char *p_dst_channel = dst_row_ptr + c * dst_image.get_pixel_size() + ch * dst_channel_byte_size;

                double current_val_double = 0.0;

                // 1. 从源图像读取值并转换为 double
                switch (src_depth)
                {
                case IMG_8U:
                    current_val_double = static_cast<double>(*reinterpret_cast<const unsigned char *>(p_src_channel));
                    break;
                case IMG_16U:
                    current_val_double = static_cast<double>(*reinterpret_cast<const unsigned short *>(p_src_channel));
                    break;
                case IMG_32S:
                    current_val_double = static_cast<double>(*reinterpret_cast<const int *>(p_src_channel));
                    break;
                case IMG_32F:
                    current_val_double = static_cast<double>(*reinterpret_cast<const float *>(p_src_channel));
                    break;
                case IMG_64F:
                    current_val_double = *reinterpret_cast<const double *>(p_src_channel);
                    break;
                default:
                    // 此情况理论上不应发生，因为源图像类型在创建时已验证
                    // 但为健壮性考虑，可以抛出一个内部错误
                    throw std::logic_error("Image::convertTo - 遇到未处理的源图像深度。");
                }

                // 2. 应用缩放和移位
                current_val_double = current_val_double * scale + shift;

                // 3. 转换为目标类型并写入目标图像 (使用 clamp_value<T> 进行饱和处理)
                switch (dst_depth)
                {
                case IMG_8U:
                    *reinterpret_cast<unsigned char *>(p_dst_channel) = clamp_value<unsigned char>(current_val_double);
                    break;
                case IMG_16U:
                    *reinterpret_cast<unsigned short *>(p_dst_channel) = clamp_value<unsigned short>(current_val_double);
                    break;
                case IMG_32S:
                    *reinterpret_cast<int *>(p_dst_channel) = clamp_value<int>(current_val_double);
                    break;
                case IMG_32F:
                    *reinterpret_cast<float *>(p_dst_channel) = clamp_value<float>(current_val_double);
                    break;
                case IMG_64F:
                    *reinterpret_cast<double *>(p_dst_channel) = clamp_value<double>(current_val_double);
                    break;
                default:
                    // 此情况理论上不应发生，因为目标图像类型 (new_type) 在 dst_image 构造时已通过 allocate 验证
                    throw std::logic_error("Image::convertTo - 遇到未处理的目标图像深度。");
                }
            }
        }
    }
    return dst_image;
}

/**
 * @brief 将图像深度转换为可读的字符串表示。
 * @param depth 图像深度的数值。
 * @return 深度的字符串表示。
 */
std::string depth_to_string(int depth) {
    switch (depth) {
        case IMG_8U: return "IMG_8U";
        case IMG_16U: return "IMG_16U";
        case IMG_32S: return "IMG_32S";
        case IMG_32F: return "IMG_32F";
        case IMG_64F: return "IMG_64F";
        default: return "未知深度";
    }
}

/**
 * @brief 打印图像的基本信息，包括尺寸、类型、通道数等。
 */
void Image::show_info(std::ostream &os) const {
    if (this->empty()) {
        os << "图像为空。" << std::endl;
        return;
    }

    os << "图像信息：" << std::endl;
    os << "尺寸: " << m_rows << " x " << m_cols << std::endl;
    os << "类型: " << m_type 
       << " (深度: " << depth_to_string(get_depth()) 
       << ", 通道数: " << get_channels() << ")" << std::endl;
    os << "每通道字节大小: " << m_channel_size << std::endl;
    os << "每行步长 (字节): " << m_step << std::endl;
    os << "引用计数: " << (m_refcount ? *m_refcount : 0) << std::endl;
}
    std::ostream &operator<<(std::ostream &os, const Image &img)
    {
        img.show_info(os); // 调用 show_info，将流传递进去
        return os;
    }
}

