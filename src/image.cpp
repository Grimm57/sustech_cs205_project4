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
     * @brief 将图像深度转换为可读的字符串表示。
     * @param depth 图像深度的数值。
     * @return 深度的字符串表示。
     */
    std::string depth_to_string(int depth)
    {
        switch (depth)
        {
        case IMG_8U:
            return "IMG_8U";
        case IMG_16U:
            return "IMG_16U";
        case IMG_32S:
            return "IMG_32S";
        case IMG_32F:
            return "IMG_32F";
        case IMG_64F:
            return "IMG_64F";
        default:
            return "未知深度";
        }
    }

    /**
     * @brief 显式释放图像数据。
     * 减少共享数据的引用计数。如果引用计数降为零，底层内存将被释放。
     * 对于手动对齐的内存，需要释放最初分配的原始内存块。
     * 然后将当前 Image 对象重置为空状态。
     */
    void Image::release()
    {
        if (m_refcount) // m_refcount 非空意味着 m_datastorage 也曾被有效设置
        {
            (*m_refcount)--;
            if (*m_refcount == 0)
            {
                // m_datastorage 指向对齐后的引用计数区。
                const size_t pointer_size = sizeof(unsigned char *);
                unsigned char *location_to_copy = m_datastorage - pointer_size;

                unsigned char *original_ptr_to_delete = nullptr; 
                std::memcpy(&original_ptr_to_delete, location_to_copy, pointer_size);
                delete[] original_ptr_to_delete; // 释放最初分配的整个内存块

                // m_refcount 指向 m_datastorage 的一部分，该内存已随 original_ptr_to_delete 被释放。
            }
            m_refcount = nullptr; // 无论是否释放内存，当前 Image 对象不再指向该引用计数
        }

        // 重置 Image 对象的状态
        m_rows = 0;
        m_cols = 0;
        m_type = -1;
        m_channel_size = 0;
        m_step = 0;
        m_data_start = nullptr;
        m_datastorage = nullptr; // m_datastorage 指向的内存（如果是对齐块的一部分）已随原始块释放
    }

    /**
     * @brief 内部辅助函数：分配内存块（包括引用计数区域和图像数据区域）。
     * 使用手动对齐确保引用计数区域对于 int 类型是对齐的。
     * @param rows 图像的行数。
     * @param cols 图像的列数。
     * @param type 图像类型。
     * @throw std::invalid_argument 如果图像类型无效、维度无效、或无法确定通道大小。
     * @throw std::overflow_error 如果内存大小计算发生溢出。
     * @throw std::runtime_error 如果内存分配失败或手动对齐失败。
     */
    void Image::allocate(size_t rows, size_t cols, int type)
    {
        const std::string F_NAME = "";
        if (type < 0)
        {
            throw std::invalid_argument(IMG_ERROR_PREFIX(F_NAME) + "图像类型不可以为负数。收到类型: " + std::to_string(type));
        }

        if (rows == 0 || cols == 0)
        {
            throw std::invalid_argument(IMG_ERROR_PREFIX(F_NAME) + "图像的行数和列数必须大于零。收到 rows: " + std::to_string(rows) + ", cols: " + std::to_string(cols));
        }

        int depth = IMG_DEPTH(type); // IMG_DEPTH 和 IMG_CN 从宏定义改为了inline函数
        int channel_cnt = IMG_CN(type);
        size_t channel_size = 0;

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
            throw std::invalid_argument(IMG_ERROR_PREFIX(F_NAME) + "不支持的图像深度类型。收到 type: " + std::to_string(type) + " (解析出的深度: " + depth_to_string(depth) + ")");
        }

        if (!(channel_cnt == 1 || channel_cnt == 3 || channel_cnt == 4))
        {
            throw std::invalid_argument(IMG_ERROR_PREFIX(F_NAME) + "不支持的图像通道数。收到 type: " + std::to_string(type) + " (解析出的通道数: " + std::to_string(channel_cnt) + ")");
        }

        size_t pixel_size = channel_size * channel_cnt;
        size_t step = 0;
        size_t total_pixel_size = 0;

        if (cols > std::numeric_limits<size_t>::max() / pixel_size)
        {
            throw std::overflow_error(IMG_ERROR_PREFIX(F_NAME) + "计算每行字节数 (step) 时发生溢出。cols: " + std::to_string(cols) + ", pixel_size: " + std::to_string(pixel_size));
        }
        step = cols * pixel_size; // 每行字节数

        if (rows > std::numeric_limits<size_t>::max() / step)
        {
            throw std::overflow_error(IMG_ERROR_PREFIX(F_NAME) + "计算总数据字节数时发生溢出。rows: " + std::to_string(rows) + ", step: " + std::to_string(step));
        }
        total_pixel_size = rows * step; // 总字节数

        // 为了使用c++的 placement new 来在已经分配的内存上进行存储，我们需要计算对齐要求并且保证数据对齐存储
        const size_t align_number = alignof(int);               // 获取 int 的对齐的地址要整出多少
        const size_t refcount_size = sizeof(int);            // 引用计数的大小
        const size_t pointer_size = sizeof(unsigned char *); // 给指向真实的数据开始位置的指针留出位置
        // 计算需要存储的和图像有关的数据的大小,引用计数 + 图像数据
        size_t total_payload_size = refcount_size + total_pixel_size;
        if (refcount_size > std::numeric_limits<size_t>::max() - total_pixel_size)
        {
            throw std::overflow_error(IMG_ERROR_PREFIX(F_NAME) + "计算引用计数和图像数据总大小时溢出。");
        }
        // 还需要加上填充的位数
        size_t padding = (align_number > 1) ? (align_number - 1) : 0;

        // 计算总的原始分配请求大小，包括对齐所需的额外填充
        // align_number - 1 是为了保证能找到对齐位置，如果 align_number 是1则不需要额外填充
        // 一般int要四位对齐，所以最多需要往前顶3位来填充
        size_t allocation_request_size = total_payload_size + pointer_size + padding;

        if (total_payload_size > std::numeric_limits<size_t>::max() - pointer_size - padding)
        {
            throw std::overflow_error(IMG_ERROR_PREFIX(F_NAME) + "计算总分配请求大小时发生溢出（考虑对齐填充）。");
        }
        unsigned char *original_raw_ptr = nullptr; // 堆上的,指向实际分配的内存块的指针
        try
        {
            // 值初始化 () 会将分配的内存清零
            original_raw_ptr = new unsigned char[allocation_request_size]();
            // 在release函数中会delete[] original_raw_ptr
        }
        catch (const std::bad_alloc &e)
        {
            throw std::runtime_error(
                IMG_ERROR_PREFIX(F_NAME) + "内存分配失败 (原始请求)。尝试分配 " +
                std::to_string(allocation_request_size) + " 字节。"
                                                          " 原始错误: " +
                e.what());
        }

        // 至少要留出 pointer_size 的空间来存储指向原始内存块的指针, 之后从原始内存块的开始位置开始流出几个用于对齐的空白字节
        // void没有对齐要求，所以可以直接用 reinterpret_cast 来转换
        void *align_start = reinterpret_cast<void *>(original_raw_ptr + pointer_size); // 这个指针是尝试找到对齐的地址的起始地址
        // 计算对齐可用的空间
        size_t space_available = allocation_request_size - pointer_size;
        // total_payload_size 是 要对齐的内存大小,space_available是可用内存大小,会返回满足align_number对齐的地址
        void *aligned_start_for_payload = std::align(align_number, total_payload_size, align_start, space_available);
        // m_datastorage 现在指向对齐后的引用计数区的开始
        m_datastorage = static_cast<unsigned char *>(aligned_start_for_payload);

        // //二维指针
        // unsigned char **stored_original_ptr_location = reinterpret_cast<unsigned char **>(m_datastorage - pointer_size);
        // *stored_original_ptr_location = original_raw_ptr;
        unsigned char *raw_pointer_start = m_datastorage - pointer_size;
        std::memcpy(raw_pointer_start, &original_raw_ptr, pointer_size);

        // 现在 m_datastorage 指向对齐的内存区域，可以在上面构造 int 作为引用计数
        m_refcount = new (m_datastorage) int(1); // Placement new

        // m_data_start 指向引用计数之后的数据区
        m_data_start = m_datastorage + refcount_size;

        // 设置 Image 对象的其他成员
        m_rows = rows;
        m_cols = cols;
        m_type = type;
        m_channel_size = channel_size;
        m_step = step;
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
     * @throw std::runtime_error 如果 allocate() 抛出任何异常 (信息会被包装)。
     */
    Image::Image(size_t rows, size_t cols, int type)
        : m_rows(0), // 初始化以防 allocate 失败
          m_cols(0),
          m_type(-1),
          m_channel_size(0),
          m_step(0),
          m_data_start(nullptr),
          m_datastorage(nullptr),
          m_refcount(nullptr)
    {
        const std::string F_NAME = "构造函数";
        try
        {
            // allocate 将会设置所有成员变量
            this->allocate(rows, cols, type);
        }
        catch (const std::exception &e)
        {
            // 如果 allocate 失败，当前对象应保持有效的空状态
            // (已通过成员初始化列表实现)。
            // 重新抛出异常，添加构造函数上下文。
            throw std::runtime_error(
                IMG_ERROR_PREFIX(F_NAME) + "创建图像失败 (rows: " + std::to_string(rows) +
                ", cols: " + std::to_string(cols) +
                ", type: " + std::to_string(type) + "). 内部错误: " + e.what());
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
        const std::string F_NAME = "Create";
        release();
        try
        {
            this->allocate(rows, cols, type);
        }
        catch (const std::exception &e)
        {
            // 如果 allocate 失败，this->release() 已经将对象置于有效的空状态。
            throw std::runtime_error(
                IMG_ERROR_PREFIX(F_NAME) + "重新分配图像失败 (rows: " + std::to_string(rows) +
                ", cols: " + std::to_string(cols) +
                ", type: " + std::to_string(type) + "). 内部错误: " + e.what());
        }
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
        const std::string F_NAME = "clone";
        if (empty())
        {
            throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "无法克隆一个空的图像。");
        }
        Image new_image; // 调用默认构造函数
        try
        {
            // create 将调用 allocate，后者会分配新的 m_datastorage（含引用计数）
            // 并设置 new_image 的所有成员。
            new_image.create(m_rows, m_cols, m_type);
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error(IMG_ERROR_PREFIX(F_NAME) + "克隆时创建新图像失败. 原始错误: " + e.what());
        }

        if (m_data_start && new_image.m_data_start && m_rows > 0 && m_cols > 0) // 确保行列都大于0
        {
            std::memcpy(new_image.m_data_start, m_data_start, m_rows * m_step);
        }

        return new_image;
    }

    /**
     * @brief 返回指向指定行的起始位置的指针。
     * 非const用于可能的修改行数据的情况
     * @param r 行索引 (从0开始)。
     * @return 指向第 `r` 行起始像素的 `unsigned char*` 指针。
     * @throw std::out_of_range 如果行索引 r 超出边界。
     * @throw std::logic_error 如果图像为空，无法获取行指针。
     */
    unsigned char *Image::get_rowptr(int r)
    {
        std::string F_NAME = "get_rowptr";
        if (static_cast<size_t>(r) >= m_rows || r < 0)
        {
            throw std::out_of_range(IMG_ERROR_PREFIX(F_NAME) + "行索引超出范围。");
        }
        if (this->empty())
        {
            throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "无法获取空图像的行指针。");
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
    const unsigned char *Image::get_rowptr(int r) const
    {
        std::string F_NAME = "get_rowptr";
        if (static_cast<size_t>(r) >= m_rows || r < 0)
        {
            throw std::out_of_range(IMG_ERROR_PREFIX(F_NAME) + "行索引超出范围。");
        }
        if (this->empty())
        {
            throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "无法获取空图像的行指针。");
        }

        return m_data_start + r * m_step;
    }

// 辅助宏
#define IMAGE_SCALAR_OP_LOOP(OP, TYPE, truncate_value)                                \
    for (size_t r = 0; r < m_rows; ++r)                                           \
    {                                                                             \
        unsigned char *row_ptr = this->get_rowptr(r);                             \
        for (size_t c = 0; c < m_cols; ++c)                                       \
        {                                                                         \
            TYPE *pixel_ptr = reinterpret_cast<TYPE *>(row_ptr + c * pixel_size); \
            for (int chan = 0; chan < channel_cnt; ++chan)                        \
            {                                                                     \
                double result = static_cast<double>(pixel_ptr[chan]) OP scalar_d; \
                pixel_ptr[chan] = truncate_value(result);                             \
            }                                                                     \
        }                                                                         \
    }

    // 这一个模板的核心功能是把double类型的值转换成对应的图像的数值类型,对于整数类型的值会进行范围限制,超过时进行截断
    template <typename T>
    T truncate_value(double value)
    {
        return static_cast<T>(value);
    }

    template <>
    unsigned char truncate_value<unsigned char>(double value)
    {
        value = std::round(value);
        return static_cast<unsigned char>(
            std::max(0.0, std::min(255.0, value)));
    }

    template <>
    unsigned short truncate_value<unsigned short>(double value)
    {
        value = std::round(value);
        return static_cast<unsigned short>(
            std::max(0.0, std::min(65535.0, value)));
    }

    // int的大小在不同平台上可能会有所不同,所以这里使用了std::numeric_limits来获取int的最大值和最小值
    template <>
    int truncate_value<int>(double value)
    {
        value = std::round(value);
        return static_cast<int>(
            std::max(static_cast<double>(std::numeric_limits<int>::min()),
                     std::min(static_cast<double>(std::numeric_limits<int>::max()), value)));
    }

    Image &Image::operator+=(double scalar)
    {
        const std::string F_NAME = "图像+标量";
        if (empty())
            throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "图像为空，无法执行标量加法。");

        int depth = get_depth();              // 依赖 m_type
        int channel_cnt = get_channels();     // 依赖 m_type
        size_t pixel_size = get_pixel_size(); // 依赖 m_type, m_channel_size
        double scalar_d = scalar;

        switch (depth)
        {
        case IMG_8U:
            IMAGE_SCALAR_OP_LOOP(+, unsigned char, truncate_value<unsigned char>);
            break;
        case IMG_16U:
            IMAGE_SCALAR_OP_LOOP(+, unsigned short, truncate_value<unsigned short>);
            break;
        case IMG_32S:
            IMAGE_SCALAR_OP_LOOP(+, int, truncate_value<int>);
            break;
        case IMG_32F:
            IMAGE_SCALAR_OP_LOOP(+, float, truncate_value<float>);
            break;
        case IMG_64F:
            IMAGE_SCALAR_OP_LOOP(+, double, truncate_value<double>);
            break;
        default:
            throw std::invalid_argument(IMG_ERROR_PREFIX(F_NAME) + "不支持的图像深度类型 (" + depth_to_string(depth) + ") 进行标量加法。");
        }
        return *this;
    }
    // operator-=, operator*=, operator/= 类似

    Image &Image::operator-=(double scalar)
    {
        const std::string F_NAME = "图像-标量";
        if (empty())
            throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "图像为空，无法执行标量减法。");
        int depth = get_depth();
        int channel_cnt = get_channels();
        size_t pixel_size = get_pixel_size();
        double scalar_d = scalar;
        switch (depth)
        {
        case IMG_8U:
            IMAGE_SCALAR_OP_LOOP(-, unsigned char, truncate_value<unsigned char>);
            break;
        case IMG_16U:
            IMAGE_SCALAR_OP_LOOP(-, unsigned short, truncate_value<unsigned short>);
            break;
        case IMG_32S:
            IMAGE_SCALAR_OP_LOOP(-, int, truncate_value<int>);
            break;
        case IMG_32F:
            IMAGE_SCALAR_OP_LOOP(-, float, truncate_value<float>);
            break;
        case IMG_64F:
            IMAGE_SCALAR_OP_LOOP(-, double, truncate_value<double>);
            break;
        default:
            throw std::invalid_argument(IMG_ERROR_PREFIX(F_NAME) + "不支持的图像深度类型 (" + depth_to_string(depth) + ") 进行标量减法。");
        }
        return *this;
    }

    Image &Image::operator*=(double scalar)
    {
        const std::string F_NAME = "图像*标量";
        if (empty())
            throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "图像为空，无法执行标量乘法。");
        int depth = get_depth();
        int channel_cnt = get_channels();
        size_t pixel_size = get_pixel_size();
        double scalar_d = scalar;
        switch (depth)
        {
        case IMG_8U:
            IMAGE_SCALAR_OP_LOOP(*, unsigned char, truncate_value<unsigned char>);
            break;
        case IMG_16U:
            IMAGE_SCALAR_OP_LOOP(*, unsigned short, truncate_value<unsigned short>);
            break;
        case IMG_32S:
            IMAGE_SCALAR_OP_LOOP(*, int, truncate_value<int>);
            break;
        case IMG_32F:
            IMAGE_SCALAR_OP_LOOP(*, float, truncate_value<float>);
            break;
        case IMG_64F:
            IMAGE_SCALAR_OP_LOOP(*, double, truncate_value<double>);
            break;
        default:
            throw std::invalid_argument(IMG_ERROR_PREFIX(F_NAME) + "不支持的图像深度类型 (" + depth_to_string(depth) + ") 进行标量乘法。");
        }
        return *this;
    }

    Image &Image::operator/=(double scalar)
    {
        const std::string F_NAME = "图像*标量";
        if (empty())
            throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "图像为空，无法执行标量除法。");
        if (std::abs(scalar) < std::numeric_limits<double>::epsilon())
            throw std::runtime_error(IMG_ERROR_PREFIX(F_NAME) + "检测到除以零或接近零的数。");
        int depth = get_depth();
        int channel_cnt = get_channels();
        size_t pixel_size = get_pixel_size();
        double scalar_d = scalar;
        switch (depth)
        {
        case IMG_8U:
            IMAGE_SCALAR_OP_LOOP(/, unsigned char, truncate_value<unsigned char>);
            break;
        case IMG_16U:
            IMAGE_SCALAR_OP_LOOP(/, unsigned short, truncate_value<unsigned short>);
            break;
        case IMG_32S:
            IMAGE_SCALAR_OP_LOOP(/, int, truncate_value<int>);
            break;
        case IMG_32F:
            IMAGE_SCALAR_OP_LOOP(/, float, truncate_value<float>);
            break;
        case IMG_64F:
            IMAGE_SCALAR_OP_LOOP(/, double, truncate_value<double>);
            break;
        default:
            throw std::invalid_argument(IMG_ERROR_PREFIX(F_NAME) + "不支持的图像深度类型 (" + depth_to_string(depth) + ") 进行标量除法。");
        }
        return *this;
    }

    Image Image::operator+(double scalar) const
    {
        Image result = this->clone();
        result += scalar;
        return result;
    }

    Image Image::operator-(double scalar) const
    {
        Image result = this->clone();
        result -= scalar;
        return result;
    }

    Image Image::operator*(double scalar) const
    {
        Image result = this->clone();
        result *= scalar;
        return result;
    }

    Image Image::operator/(double scalar) const
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

#define IMAGE_IMAGE_OP_LOOP(OP, TYPE, truncate_value)                                                                        \
    for (size_t r = 0; r < m_rows; ++r)                                                                                  \
    {                                                                                                                    \
        unsigned char *row_ptr_this = this->get_rowptr(r);                                                               \
        const unsigned char *row_ptr_other = other.get_rowptr(r);                                                        \
        for (size_t c = 0; c < m_cols; ++c)                                                                              \
        {                                                                                                                \
            TYPE *pixel_ptr_this = reinterpret_cast<TYPE *>(row_ptr_this + c * pixel_size);                              \
            const TYPE *pixel_ptr_other = reinterpret_cast<const TYPE *>(row_ptr_other + c * pixel_size);                \
            for (int chan = 0; chan < channel_cnt; ++chan)                                                               \
            {                                                                                                            \
                double result = static_cast<double>(pixel_ptr_this[chan]) OP static_cast<double>(pixel_ptr_other[chan]); \
                pixel_ptr_this[chan] = truncate_value(result);                                                               \
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
    void Image::check_compatibility(const Image &img1, const Image &img2, const std::string &operation_name)
    {
        const std::string F_CONTEXT = IMG_ERROR_PREFIX(operation_name) + "兼容性检查失败: ";

        if (img1.empty() || img2.empty())
        {
            throw std::logic_error(F_CONTEXT + "输入图像不能为空。img1 is " + (img1.empty() ? "empty" : "not empty") + ", img2 is " + (img2.empty() ? "empty" : "not empty") + ".");
        }
        if (img1.get_rows() != img2.get_rows() || img1.get_cols() != img2.get_cols())
        {
            throw std::invalid_argument(F_CONTEXT + "图像尺寸不匹配。"
                                                    " img1 (rows,cols): (" +
                                        std::to_string(img1.get_rows()) + "," + std::to_string(img1.get_cols()) + "),"
                                                                                                                  " img2 (rows,cols): (" +
                                        std::to_string(img2.get_rows()) + "," + std::to_string(img2.get_cols()) + ").");
        }
        if (img1.get_type() != img2.get_type())
        {
            throw std::invalid_argument(F_CONTEXT + "图像类型不匹配。"
                                                    " img1 type: " +
                                        std::to_string(img1.get_type()) + " (" + depth_to_string(img1.get_depth()) + "C" + std::to_string(img1.get_channels()) + "),"
                                                                                                                                                                 " img2 type: " +
                                        std::to_string(img2.get_type()) + " (" + depth_to_string(img2.get_depth()) + "C" + std::to_string(img2.get_channels()) + ").");
        }
    }

    Image &Image::operator+=(const Image &other)
    {
        const std::string F_NAME = "图像相加";
        check_compatibility(*this, other, F_NAME);

        int depth = get_depth();
        int channel_cnt = get_channels();
        size_t pixel_size = get_pixel_size();

        switch (depth)
        {
        case IMG_8U:
            IMAGE_IMAGE_OP_LOOP(+, unsigned char, truncate_value<unsigned char>);
            break;
        case IMG_16U:
            IMAGE_IMAGE_OP_LOOP(+, unsigned short, truncate_value<unsigned short>);
            break;
        case IMG_32S:
            IMAGE_IMAGE_OP_LOOP(+, int, truncate_value<int>);
            break;
        case IMG_32F:
            IMAGE_IMAGE_OP_LOOP(+, float, truncate_value<float>);
            break;
        case IMG_64F:
            IMAGE_IMAGE_OP_LOOP(+, double, truncate_value<double>);
            break;
        default:
            throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "内部错误: 不支持的图像深度 (" + depth_to_string(depth) + ") 在类型匹配后仍然出现。");
        }
        return *this;
    }

    Image &Image::operator-=(const Image &other)
    {
        const std::string F_NAME = "图像相减";
        check_compatibility(*this, other, F_NAME);
        int depth = get_depth();
        int channel_cnt = get_channels();
        size_t pixel_size = get_pixel_size();
        switch (depth)
        {
        case IMG_8U:
            IMAGE_IMAGE_OP_LOOP(-, unsigned char, truncate_value<unsigned char>);
            break;
        case IMG_16U:
            IMAGE_IMAGE_OP_LOOP(-, unsigned short, truncate_value<unsigned short>);
            break;
        case IMG_32S:
            IMAGE_IMAGE_OP_LOOP(-, int, truncate_value<int>);
            break;
        case IMG_32F:
            IMAGE_IMAGE_OP_LOOP(-, float, truncate_value<float>);
            break;
        case IMG_64F:
            IMAGE_IMAGE_OP_LOOP(-, double, truncate_value<double>);
            break;
        default:
            throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "内部错误: 不支持的图像深度 (" + depth_to_string(depth) + ") 在类型匹配后仍然出现。");
        }
        return *this;
    }

    Image Image::operator+(const Image &other) const
    {
        Image result = this->clone();
        result += other;
        return result;
    }
    
    Image Image::operator-(const Image &other) const
    {
        Image result = this->clone();
        result -= other;
        return result;
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
        const std::string F_NAME = "roi";
        if (empty())
        {
            throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "无法从空图像创建 ROI。");
        }

        if ((width > 0 || height > 0) && (m_rows == 0 || m_cols == 0) && !m_data_start)
        { // 如果源是0x0且无数据区，但请求非0 ROI
            throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "无法从无像素数据的源图像创建非空 ROI。");
        }

        if (x > m_cols || (width > 0 && x + width > m_cols) ||
            y > m_rows || (height > 0 && y + height > m_rows))
        {
            bool x_at_edge_ok = (x == m_cols && width == 0);
            bool y_at_edge_ok = (y == m_rows && height == 0);

            if (!((x_at_edge_ok || x < m_cols) && (y_at_edge_ok || y < m_rows) &&
                  (width == 0 || x + width <= m_cols) && (height == 0 || y + height <= m_rows)))
            {
                std::string error_msg = IMG_ERROR_PREFIX(F_NAME) + "ROI 参数越界或无效。";
                error_msg += " 原始图像 (行, 列): (" + std::to_string(m_rows) + ", " + std::to_string(m_cols) + ").";
                error_msg += " ROI 请求 (x, y, 宽度, 高度): (" + std::to_string(x) + ", " + std::to_string(y) + ", " +
                             std::to_string(width) + ", " + std::to_string(height) + ").";
                throw std::out_of_range(error_msg);
            }
        }

        Image roi_view;
        roi_view.m_datastorage = this->m_datastorage; // 共享整个底层内存块
        roi_view.m_refcount = this->m_refcount;       // 共享引用计数 (它指向 m_datastorage 的开头)

        (*roi_view.m_refcount)++; // 增加共享数据的引用

        // m_data_start 指向源图像数据区的开始 (跳过引用计数)
        // ROI 的 m_data_start 是基于源的 m_data_start 进行偏移
        if (this->m_data_start)
        { // 只有当源图像有数据区时，偏移才有意义
            roi_view.m_data_start = this->m_data_start + (y * this->m_step) + (x * this->get_pixel_size());
        }
        roi_view.m_rows = height;
        roi_view.m_cols = width;
        roi_view.m_type = this->m_type;
        roi_view.m_channel_size = this->m_channel_size;
        roi_view.m_step = this->m_step; // ROI 的 step 与原图相同

        return roi_view;
    }

    /**
     * @brief 将图像转换为另一种数据类型。
     * 创建一个具有指定 `new_type` 的新图像。像素值将直接从源类型转换为目标类型，
     * 对于整数类型间的转换，或从浮点到整数的转换，会进行饱和处理 (例如，值被截断到目标类型的有效范围内)。
     * 对于从整数到浮点数的转换，通常是精确的。
     *
     * @param new_type 新图像的目标数据类型 (例如, IMG_8UC1, IMG_32FC3)。
     *                 `new_type` 中的通道数必须与源图像匹配。
     * @return 一个包含转换后数据和类型的新 Image 对象。
     * @throw std::logic_error 如果源图像为空 (无引用计数或无数据指针进行转换)，
     *                         或者源图像与 `new_type` 指定的通道数不匹配。
     * @throw std::invalid_argument 如果 `new_type` 无效 (例如，不支持的深度或通道数)。
     * @throw std::runtime_error 如果为新图像分配内存失败。
     */
    Image Image::convert_to(int new_type) const
    {
        const std::string F_NAME = "convert_to";
         if (this->get_type() == new_type)
        {
            return this->clone(); // 直接返回深拷贝，clone 会处理异常
        }
        //检查合法性
        if (empty())
        {
            throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "无法转换从未初始化的图像 (无引用计数)。");
        }
        int src_channels = this->get_channels();
        int new_channel_cnt = 0;// 解析 new_type 中的通道数
        int new_depth = 0;
        //暂时不需要检查type合法性,因为后边会用create函数来检查
        try
        {
            new_channel_cnt = IMG_CN(new_type);
            new_depth = IMG_DEPTH(new_type);

            // 验证解析出的通道数是否受支持
            if (!(new_channel_cnt == 1 || new_channel_cnt == 3 || new_channel_cnt == 4))
            {
                throw std::invalid_argument("从 new_type (" + std::to_string(new_type) + ") 解析出的通道数 (" + std::to_string(new_channel_cnt) + ") 不受支持。");
            }
            // 验证解析出的深度是否受支持
            bool valid_depth = false;
            switch (new_depth)
            {
            case IMG_8U:
            case IMG_16U:
            case IMG_32S:
            case IMG_32F:
            case IMG_64F:
                valid_depth = true;
                break;
            }
            if (!valid_depth)
            {
                throw std::invalid_argument("new_type (" + std::to_string(new_type) + ") 解析出的深度 (" + depth_to_string(new_depth) + ") 不受支持。");
            }
        }
        catch (const std::exception &e_parse)
        { 
            throw std::invalid_argument(IMG_ERROR_PREFIX(F_NAME) + "目标类型 new_type (" + std::to_string(new_type) + ") 无效: " + e_parse.what());
        }
        //目前不支持修改通道数量
        if (src_channels != new_channel_cnt)
        {
            throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "源类型和目标类型的通道数不匹配。"
                                                              " 源通道数: " +
                                   std::to_string(src_channels) +
                                   ", 请求的目标类型 " + std::to_string(new_type) + " 解析出的通道数: " + std::to_string(new_channel_cnt));
        }

        Image dst_image;
        try
        {
            // create 会调用 allocate, 后者会再次验证 new_type 的深度和通道组合的有效性
            dst_image.create(m_rows, m_cols, new_type);
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error(IMG_ERROR_PREFIX(F_NAME) + "创建目标图像 (type: " + std::to_string(new_type) + ") 失败. 内部错误: " + e.what());
        }

        // if (!this->m_data_start)
        // {
        //     throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "源图像数据指针 (m_data_start) 为空，但图像尺寸非零。无法执行像素转换。");
        // }

        int src_depth = this->get_depth();

        size_t src_pixel_byte_size = this->get_pixel_size();
        size_t dst_pixel_byte_size = dst_image.get_pixel_size();
        unsigned char src_channel_byte_size = this->m_channel_size;
        unsigned char dst_channel_byte_size = dst_image.m_channel_size;

        for (size_t r = 0; r < m_rows; ++r)
        {
            const unsigned char *src_row_ptr = this->get_rowptr(r);
            unsigned char *dst_row_ptr = dst_image.get_rowptr(r);

            for (size_t c = 0; c < m_cols; ++c)
            {
                for (int ch = 0; ch < src_channels; ++ch) 
                {
                    const unsigned char *p_src_channel = src_row_ptr + c * src_pixel_byte_size + ch * src_channel_byte_size;
                    unsigned char *p_dst_channel = dst_row_ptr + c * dst_pixel_byte_size + ch * dst_channel_byte_size;

                    // 读取源像素值并转换为 double 作为中间表示
                    double intermediate_val_double = 0.0;
                    switch (src_depth)
                    {
                    case IMG_8U:
                        intermediate_val_double = static_cast<double>(*reinterpret_cast<const unsigned char *>(p_src_channel));
                        break;
                    case IMG_16U:
                        intermediate_val_double = static_cast<double>(*reinterpret_cast<const unsigned short *>(p_src_channel));
                        break;
                    case IMG_32S:
                        intermediate_val_double = static_cast<double>(*reinterpret_cast<const int *>(p_src_channel));
                        break;
                    case IMG_32F:
                        intermediate_val_double = static_cast<double>(*reinterpret_cast<const float *>(p_src_channel));
                        break;
                    case IMG_64F:
                        intermediate_val_double = *reinterpret_cast<const double *>(p_src_channel);
                        break;
                    default:
                        // 此处理论上不应到达，因为源图像类型在创建时已验证
                        throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "内部错误: 遇到未处理的源图像深度 (" + depth_to_string(src_depth) + ").");
                    }

                    // 将中间值转换为目标类型，并使用 truncate_value 进行饱和处理
                    switch (new_depth)
                    {
                    case IMG_8U:
                        *reinterpret_cast<unsigned char *>(p_dst_channel) = truncate_value<unsigned char>(intermediate_val_double);
                        break;
                    case IMG_16U:
                        *reinterpret_cast<unsigned short *>(p_dst_channel) = truncate_value<unsigned short>(intermediate_val_double);
                        break;
                    case IMG_32S:
                        *reinterpret_cast<int *>(p_dst_channel) = truncate_value<int>(intermediate_val_double);
                        break;
                    case IMG_32F:
                        *reinterpret_cast<float *>(p_dst_channel) = truncate_value<float>(intermediate_val_double);
                        break; // truncate_value<float> 只是 static_cast
                    case IMG_64F:
                        *reinterpret_cast<double *>(p_dst_channel) = truncate_value<double>(intermediate_val_double);
                        break; // truncate_value<double> 只是 static_cast
                    default:
                        // 此处理论上不应到达，因为目标图像类型在创建 dst_image 时已通过 allocate 验证
                        throw std::logic_error(IMG_ERROR_PREFIX(F_NAME) + "内部错误: 遇到未处理的目标图像深度 (" + depth_to_string(new_depth) + ").");
                    }
                }
            }
        }
        return dst_image;
    }

    /**
     * @brief 打印图像的基本信息，包括尺寸、类型、通道数等。
     */
    void Image::show_info(std::ostream &os) const
    {
        if (this->empty())
        {
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
