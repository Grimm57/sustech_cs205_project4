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
#include <algorithm> // 用于 std::transform (转换小写)
#include <cctype>

using namespace img;

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
}

/**
 * @brief 根据指定的维度和类型构造图像。
 * 会为图像数据和引用计数分配新的内存。
 */
Image::Image(size_t rows, size_t cols, int type)
    : m_rows(0), // 先初始化为安全状态
      m_cols(0),
      m_type(-1),
      m_channel_size(0),
      m_step(0),
      m_data_start(nullptr),
      m_datastorage(nullptr),
      m_refcount(nullptr)
{
    // 调用 allocate 函数来执行实际的内存分配和成员设置
    allocate(rows, cols, type);
}

/**
 * @brief 拷贝构造函数 (浅拷贝)。
 * 共享数据，增加引用计数。
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
    // 如果源对象确实拥有数据 (m_refcount 非空), 则增加引用计数
    if (m_refcount)
    {
        (*m_refcount)++;
    }
}

/**
 * @brief 移动构造函数。
 * 从源对象 `other` 接管资源，并将 `other` 置为空状态。
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
    // 将源对象置为空状态，防止其析构时释放资源
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
 * 减少引用计数，如果计数为零则释放内存。
 */
Image::~Image()
{
    release(); // 调用 release 来处理引用计数和内存释放
}

/**
 * @brief 拷贝赋值运算符。
 * 使当前对象共享 `other` 的数据。
 */
Image &Image::operator=(const Image &other)
{
    // 1. 处理自赋值: 如果源和目标是同一个对象，则不做任何事
    if (this == &other)
    {
        return *this;
    }

    // 2. 释放当前对象可能持有的资源
    // (release 会减少当前资源的引用计数，并在需要时释放内存)
    this->release();

    // 3. 复制源对象的成员到当前对象 (浅拷贝)
    m_rows = other.m_rows;
    m_cols = other.m_cols;
    m_type = other.m_type;
    m_channel_size = other.m_channel_size;
    m_step = other.m_step;
    m_data_start = other.m_data_start;
    m_datastorage = other.m_datastorage;
    m_refcount = other.m_refcount;

    // 4. 如果源对象拥有数据，则增加共享数据的引用计数
    if (m_refcount)
    {
        (*m_refcount)++;
    }

    // 5. 返回对当前对象的引用
    return *this;
}

/**
 * @brief 移动赋值运算符。
 * 从 `other` 接管资源，并将 `other` 置为空状态。
 */
Image &Image::operator=(Image &&other)
{
    // 1. 处理自赋值 
    if (this == &other)
    {
        return *this;
    }

    // 2. 释放当前对象可能持有的资源
    this->release();

    // 3. 从源对象 '窃取' 资源 (直接移动成员)
    m_rows = other.m_rows;
    m_cols = other.m_cols;
    m_type = other.m_type;
    m_channel_size = other.m_channel_size;
    m_step = other.m_step;
    m_data_start = other.m_data_start;
    m_datastorage = other.m_datastorage;
    m_refcount = other.m_refcount;

    // 4. 将源对象置为空状态，防止它在析构时释放被窃取的资源
    other.m_rows = 0;
    other.m_cols = 0;
    other.m_type = -1;
    other.m_channel_size = 0;
    other.m_step = 0;
    other.m_data_start = nullptr;
    other.m_datastorage = nullptr;
    other.m_refcount = nullptr;

    // 5. 返回对当前对象的引用
    return *this;
}

/**
 * @brief 创建图像的深拷贝（克隆）。
 * 分配新的内存并复制数据。
 */
Image Image::clone() const
{
    // 如果源图像为空，抛出异常 (根据头文件要求)
    if (empty())
    {
        throw std::runtime_error("Image::clone - 无法克隆空图像。");
    }

    // 1. 创建一个新的 Image 对象，具有相同的尺寸和类型。
    //    这会自动调用构造函数 Image(rows, cols, type)，分配新内存和引用计数器。
    Image new_image(m_rows, m_cols, m_type);

    // 2. 检查新图像是否成功创建 (理论上构造函数会处理，但可以加一层保险)
    if (new_image.empty())
    {
        // 这种情况通常是内存分配失败导致的，构造函数应该已经抛了 bad_alloc
        // 但如果构造函数逻辑有变，这里可以加个检查
        throw std::runtime_error("Image::clone - 克隆时创建新图像失败。");
    }

    std::memcpy(new_image.m_data_start, m_data_start, m_rows * m_step);

    // 4. 返回新创建的、包含独立数据副本的 Image 对象。
    return new_image;
}

/**
 * @brief (重新)分配图像数据存储空间。
 * 如果参数与当前状态相同，则不操作。否则先释放再分配。
 */
void Image::create(size_t rows, size_t cols, int type)
{
    // 如果请求的尺寸和类型与当前相同，则无需执行任何操作
    if (m_rows == rows && m_cols == cols && m_type == type && m_data_start != nullptr)
    {
        return;
    }

    // 如果参数不同，首先释放当前可能持有的资源
    release();

    // 然后调用 allocate 来分配新的内存并设置成员变量
    allocate(rows, cols, type);
}

/**
 * @brief 显式释放图像数据。
 * 减少引用计数，如果降为零则释放内存，并将对象置为空状态。
 */
void Image::release()
{
    // 只有当 m_refcount 不为空时，才表示当前对象指向了有效的、需要管理的数据块
    if (m_refcount)
    {
        // 原子地减少引用计数 (如果使用 std::atomic<int>)
        // (*m_refcount)--;
        // 如果使用普通 int* (非线程安全)
        (*m_refcount)--;

        // 检查引用计数是否降为零
        if (*m_refcount == 0)
        {
            // 引用计数为零，释放整个内存块 (包括引用计数本身)
            // m_datastorage 指向内存块的起始位置
            delete[] m_datastorage;
            // std::cout << "内存已释放" << std::endl; // 调试信息
        }
        // else {
        //     // std::cout << "引用计数减少: " << *m_refcount << std::endl; // 调试信息
        // }

        // 无论是否释放了内存，当前 Image 对象都不再拥有或引用该数据
        // 因此，将所有成员重置为空/初始状态
        m_refcount = nullptr; // 不再指向引用计数
    }

    // 重置其他成员变量，确保对象处于明确的空状态
    m_rows = 0;
    m_cols = 0;
    m_type = -1;
    m_channel_size = 0;
    m_step = 0;
    m_data_start = nullptr;
    m_datastorage = nullptr; // 即使没释放，当前对象不再负责它 (被 release 调用清除 m_refcount 后)
}

/**
 * @brief 内部辅助函数：分配内存块（包括引用计数区域）
 * @throws std::bad_alloc 如果内存分配失败。
 * @throws std::runtime_error 如果类型无效或尺寸为0。
 */
void Image::allocate(size_t rows, size_t cols, int type)
{
    // 1. 输入验证
    if (rows <= 0 || cols <= 0)
    {
        throw std::runtime_error("Image::allocate - 图像尺寸 (行或列) 不能小于等于零。");
    }

    int depth = IMG_DEPTH(type);
    int channels = IMG_CN(type);
    size_t channel_size = 0;

    // 根据深度确定每个通道的字节数
    switch (depth)
    {
    case IMG_8U:
        channel_size = 1;
        break; // unsigned char
    case IMG_16U:
        channel_size = 2;
        break; // unsigned short
    case IMG_32S:
        channel_size = 4;
        break; // int
    case IMG_32F:
        channel_size = 4;
        break; // float
    case IMG_64F:
        channel_size = 8;
        break; // double
    default:
        throw std::runtime_error("Image::allocate - 不支持的图像深度类型。");
    }

    if (channels <= 0) // 通道数至少为1
    {
        throw std::runtime_error("Image::allocate - 图像通道数必须大于零。");
    }

    // 2. 计算内存布局
    size_t pixel_size = channel_size * channels; // 每个像素的总字节数
    // 计算每行的字节数
    size_t step = cols * pixel_size;
    // 计算纯像素数据所需的总字节数
    size_t data_byte_size = rows * step;

    // 计算总共需要分配的内存大小：引用计数 (int) + 像素数据
    size_t total_size = sizeof(int) + data_byte_size;

    // 3. 分配内存
    // 使用 new unsigned char[] 分配原始字节块
    unsigned char *new_datastorage = nullptr;
    try
    {
        new_datastorage = new unsigned char[total_size];
    }
    catch (const std::bad_alloc &e)
    {
        release(); // 确保旧资源已释放 (虽然 create 调用时已释放)
        throw;     
    }

    // 4. 初始化内存和成员变量
    m_datastorage = new_datastorage; // 指向分配块的起始

    // 将引用计数放在内存块的开头
    m_refcount = reinterpret_cast<int *>(m_datastorage);
    *m_refcount = 1; // 新分配的内存，引用计数为 1

    // 像素数据的起始地址紧跟在引用计数之后
    m_data_start = m_datastorage + sizeof(int);

    // 设置图像的元数据
    m_rows = rows;
    m_cols = cols;
    m_type = type;
    m_channel_size = static_cast<unsigned char>(channel_size); // 存储计算出的通道大小
    m_step = step;
}

/**
 * @brief 返回指向指定行的起始位置的指针。
 */
unsigned char *Image::ptr(int r)
{
    // 检查行索引是否越界
    if (static_cast<size_t>(r) >= m_rows) // 同时处理了 r < 0 的情况 (因为 size_t 是无符号的)
    {
        throw std::out_of_range("Image::ptr - 行索引超出范围。");
    }
    if (empty()) // 虽然前面的检查可能已覆盖，但明确检查空图像更安全
    {
        throw std::logic_error("Image::ptr - 无法获取空图像的行指针。");
    }
    // 计算并返回第 r 行的起始地址
    return m_data_start + r * m_step;
}

/**
 * @brief 返回指向指定行的起始位置的常量指针。
 */
const unsigned char *Image::ptr(int r) const
{
    // 逻辑与非 const 版本相同，但返回 const 指针
    if (static_cast<size_t>(r) >= m_rows)
    {
        throw std::out_of_range("Image::ptr (const) - 行索引超出范围。");
    }
    if (empty())
    {
        throw std::logic_error("Image::ptr (const) - 无法获取空图像的行指针。");
    }
    return m_data_start + r * m_step;
}

// --- 图像与标量的算术操作 ---

// OP: 要执行的操作 (+, -, *, /)
// TYPE: C++ 数据类型 (unsigned char, float, etc.)
// CLAMP_FUNC: 用于钳位的函数 (对于浮点数，是 no-op)
#define IMAGE_SCALAR_OP_LOOP(OP, TYPE, CLAMP_FUNC)                                \
    for (size_t r = 0; r < m_rows; ++r)                                           \
    {                                                                             \
        unsigned char *row_ptr = this->ptr(r);                                    \
        for (size_t c = 0; c < m_cols; ++c)                                       \
        {                                                                         \
            TYPE *pixel_ptr = reinterpret_cast<TYPE *>(row_ptr + c * psz);        \
            for (int chan = 0; chan < cn; ++chan)                                 \
            {                                                                     \
                /* 先转换为 double 进行计算，再转回 TYPE */                         \
                double result = static_cast<double>(pixel_ptr[chan]) OP scalar_d; \
                pixel_ptr[chan] = CLAMP_FUNC(result);                             \
            }                                                                     \
        }                                                                         \
    }

template <typename T>
T clamp_value(double value)
{
    return static_cast<T>(value);
}

// 特化版本：unsigned char (8U)
template <>
unsigned char clamp_value<unsigned char>(double value)
{
    value = std::round(value); // 四舍五入到最近的整数
    return static_cast<unsigned char>(
        std::max(0.0, std::min(static_cast<double>(std::numeric_limits<unsigned char>::max()), value)));
}

// 特化版本：unsigned short (16U)
template <>
unsigned short clamp_value<unsigned short>(double value)
{
    value = std::round(value);
    return static_cast<unsigned short>(
        std::max(0.0, std::min(static_cast<double>(std::numeric_limits<unsigned short>::max()), value)));
}

// 特化版本：int (32S)
template <>
int clamp_value<int>(double value)
{
    value = std::round(value);
    return static_cast<int>(
        std::max(static_cast<double>(std::numeric_limits<int>::min()),
                 std::min(static_cast<double>(std::numeric_limits<int>::max()), value)));
}

/** @brief 图像与标量的复合加法赋值 (逐像素)。 */
Image &Image::operator+=(double scalar)
{
    if (empty())
        throw std::logic_error("Image::operator+= - 图像为空。");

    int depth = get_depth();
    int cn = get_channels();
    size_t psz = get_pixel_size();
    double scalar_d = scalar; // 避免在循环内重复转换

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
        throw std::runtime_error("Image::operator+= - 不支持的图像类型。");
    }
    return *this;
}

/** @brief 图像与标量的复合减法赋值 (逐像素)。 */
Image &Image::operator-=(double scalar)
{
    if (empty())
        throw std::logic_error("Image::operator-= - 图像为空。");

    int depth = get_depth();
    int cn = get_channels();
    size_t psz = get_pixel_size();
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
        throw std::runtime_error("Image::operator-= - 不支持的图像类型。");
    }
    return *this;
}

/** @brief 图像与标量的复合乘法赋值 (逐像素)。 */
Image &Image::operator*=(double scalar)
{
    if (empty())
        throw std::logic_error("Image::operator*= - 图像为空。");

    int depth = get_depth();
    int cn = get_channels();
    size_t psz = get_pixel_size();
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
        throw std::runtime_error("Image::operator*= - 不支持的图像类型。");
    }
    return *this;
}

/** @brief 图像与标量的复合除法赋值 (逐像素)。 */
Image &Image::operator/=(double scalar)
{
    if (empty())
        throw std::logic_error("Image::operator/= - 图像为空。");
    if (scalar == 0.0)
        throw std::runtime_error("Image::operator/= - 检测到除以零。");

    int depth = get_depth();
    int cn = get_channels();
    size_t psz = get_pixel_size();
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
        throw std::runtime_error("Image::operator/= - 不支持的图像类型。");
    }
    return *this;
}

// 非复合赋值版本 (创建副本进行操作)
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

// 友元函数实现 (标量在左侧)
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

// --- 图像与图像的算术操作 ---

// 辅助宏，用于简化图像间操作的类型分发和钳位
#define IMAGE_IMAGE_OP_LOOP(OP, TYPE, CLAMP_FUNC)                                                                        \
    for (size_t r = 0; r < m_rows; ++r)                                                                                  \
    {                                                                                                                    \
        unsigned char *row_ptr_this = this->ptr(r);                                                                      \
        const unsigned char *row_ptr_other = other.ptr(r);                                                               \
        for (size_t c = 0; c < m_cols; ++c)                                                                              \
        {                                                                                                                \
            TYPE *pixel_ptr_this = reinterpret_cast<TYPE *>(row_ptr_this + c * psz);                                     \
            const TYPE *pixel_ptr_other = reinterpret_cast<const TYPE *>(row_ptr_other + c * psz);                       \
            for (int chan = 0; chan < cn; ++chan)                                                                        \
            {                                                                                                            \
                /* 先转换为 double 进行计算，再转回 TYPE */                                                              \
                double result = static_cast<double>(pixel_ptr_this[chan]) OP static_cast<double>(pixel_ptr_other[chan]); \
                pixel_ptr_this[chan] = CLAMP_FUNC(result);                                                               \
            }                                                                                                            \
        }                                                                                                                \
    }

/**
 * @brief 检查两个图像是否兼容进行逐元素操作。
 */
void Image::check_compatibility(const Image &img1, const Image &img2)
{
    if (img1.empty() || img2.empty())
    {
        throw std::runtime_error("check_compatibility - 输入图像不能为空。");
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
 */
Image &Image::operator+=(const Image &other)
{
    check_compatibility(*this, other); // 检查兼容性

    int depth = get_depth();
    int cn = get_channels();
    size_t psz = get_pixel_size();

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
 */
Image &Image::operator-=(const Image &other)
{
    check_compatibility(*this, other); // 检查兼容性

    int depth = get_depth();
    int cn = get_channels();
    size_t psz = get_pixel_size();

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


Image Image::roi(size_t x, size_t y, size_t width, size_t height) const
{
    // 1. 检查当前图像是否为空
    if (this->empty())
    {
        throw std::logic_error("Image::roi - Cannot create ROI from an empty image.");
    }

    // 2. 检查ROI参数的有效性
    //    - ROI的右下角不能超出原图的右下角
    //    - ROI的宽度和高度必须大于0
    if (x >= m_cols || y >= m_rows ||                 // ROI的左上角就在原图外部或边缘
        width == 0 || height == 0 ||                  // ROI的尺寸为0
        x + width > m_cols || y + height > m_rows)    // ROI的右下角超出了原图边界
    {
        // 可以提供更详细的错误信息
        std::string error_msg = "Image::roi - ROI parameters are out of bounds or invalid. ";
        error_msg += "Original (rows, cols): (" + std::to_string(m_rows) + ", " + std::to_string(m_cols) + "). ";
        error_msg += "ROI (x, y, width, height): (" + std::to_string(x) + ", " + std::to_string(y) + ", " +
                       std::to_string(width) + ", " + std::to_string(height) + ").";
        throw std::out_of_range(error_msg);
    }

    // 3. 创建一个新的 Image 对象 (它会调用默认构造函数，初始为空)
    Image roi_view;

    // 4. 设置 roi_view 的成员以共享数据和引用计数
    roi_view.m_datastorage = this->m_datastorage; // 共享底层数据存储的起始指针
    roi_view.m_refcount = this->m_refcount;       // 共享引用计数指针

    // 5. 增加引用计数
    //    必须检查 m_refcount 是否为 nullptr，尽管在非空图像中它不应该是。
    //    this->m_datastorage 和 this->m_refcount 应该是被 this->allocate() 初始化的。
    //    如果 this 不是空的，那么 this->m_refcount 应该指向一个有效的 int。
    if (roi_view.m_refcount)
    {
        (*roi_view.m_refcount)++;
    }
    else
    {
        // 这种情况理论上不应该发生在非空图像上，除非 Image 类的内部状态管理有误
        // 或者它是一个通过特殊方式创建的、不管理自己数据的 Image 对象（但这不符合当前设计）
        // 为安全起见，可以抛出异常或断言失败
        assert(false && "Image::roi - Source image is not empty but has no refcount. Inconsistent state.");
        throw std::logic_error("Image::roi - Source image has inconsistent reference counting state.");
    }


    // 6. 设置 roi_view 的数据指针和维度信息
    //    m_data_start 指向此ROI视图在共享数据块中的实际起始位置
    //    注意：this->m_data_start 本身可能已经是一个ROI的起始点，如果Image对象可以嵌套创建ROI
    //    但根据我们之前的讨论，更简单的模型是 this->m_data_start 总是指向 this->m_datastorage 对于一个非ROI图像
    //    如果 Image 本身可以是一个 ROI 视图，那么计算基准应该是 this->m_data_start
    //    当前设计中，m_data_start 就是实际数据的开始，m_datastorage 是分配块的开始。
    //    如果原图本身就是一个ROI，它的 m_data_start 已经是指向子区域了。
    //    所以，新的ROI的 m_data_start 应该是基于当前视图的 m_data_start 计算。
    roi_view.m_data_start = this->m_data_start + (y * this->m_step) + (x * this->get_pixel_size());

    roi_view.m_rows = height;
    roi_view.m_cols = width;
    roi_view.m_type = this->m_type;
    roi_view.m_channel_size = this->m_channel_size;

    // ROI 的行步长 (m_step) 与父图像相同，因为内存布局没有改变
    roi_view.m_step = this->m_step;

    // 7. 返回创建的 ROI 视图
    return roi_view;
}
