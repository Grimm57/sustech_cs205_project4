#include "imglib.h" // 包含头文件
#include <stdexcept>
#include <vector>
#include <cmath>     // for std::round, std::floor, std::abs
#include <cstring>   // for memcpy
#include <limits>    // for numeric_limits
#include <algorithm> // for std::min, std::max

namespace img
{

    // 模板辅助函数，处理特定数据类型 T
    template <typename T>
    void adjust_brightness_typed(Image &img, double value)
    {
        size_t rows = img.get_rows();
        size_t cols = img.get_cols();
        int channels = img.get_channels();

        // 根据目标类型 T 转换调整值
        T typed_value;
        if constexpr (std::is_floating_point_v<T>)
        {
            typed_value = static_cast<T>(value);
        }
        else if constexpr (std::is_integral_v<T>)
        {
            typed_value = static_cast<T>(std::round(value)); // 整数类型四舍五入
        }
        else
        {
            // 可以选择抛出错误或进行默认处理
            throw std::runtime_error("adjust_brightness_typed: 不支持的数据类型。");
        }

        for (size_t r = 0; r < rows; ++r)
        {
            // 获取行指针并转换为目标类型 T*
            T *row_ptr = reinterpret_cast<T *>(img.ptr(r));
            for (size_t c = 0; c < cols * channels; ++c)
            {
                if (channels == 4 && (c % channels == 3)) // Alpha 通道索引是 channels - 1
                {
                    continue;
                }
                // 执行加法
                // 对于浮点数，直接相加
                if constexpr (std::is_floating_point_v<T>)
                {
                    row_ptr[c] += typed_value;
                }
                // 对于整数类型，需要进行饱和运算（防止溢出）
                else if constexpr (std::is_integral_v<T>)
                {
                    // 使用 double 进行中间计算以避免整数溢出
                    double result = static_cast<double>(row_ptr[c]) + static_cast<double>(typed_value); // 使用转换后的typed_value

                    // 获取类型的最大最小值
                    constexpr double min_val = static_cast<double>(std::numeric_limits<T>::min());
                    constexpr double max_val = static_cast<double>(std::numeric_limits<T>::max());

                    // 截断到类型的有效范围
                    result = std::max(min_val, std::min(max_val, result));

                    row_ptr[c] = static_cast<T>(result); // 转回目标类型 T
                }
            }
        }
    }

    // 主 adjust_brightness 函数，负责类型分发
    void adjust_brightness(Image &img, double value)
    {
        if (img.empty())
        {
            throw std::runtime_error("adjust_brightness: 输入图像为空。");
        }

        int depth = img.get_depth();

        // 支持switch中的深度，支持1，3，4通道
        switch (depth)
        {
        case IMG_8U:
            adjust_brightness_typed<unsigned char>(img, value);
            break;
        case IMG_16U:
            adjust_brightness_typed<unsigned short>(img, value);
            break;
        case IMG_32S:
            adjust_brightness_typed<int>(img, value);
            break;
        case IMG_32F:
            adjust_brightness_typed<float>(img, value);
            break;
        case IMG_64F:
            adjust_brightness_typed<double>(img, value);
            break;
        default:
            throw std::runtime_error("adjust_brightness: 不支持的图像深度类型。");
        }
    }

    // --- 实现 blend_images ---
    template <typename T>
    void blend_images_typed(const Image &img1, const Image &img2, Image &output, double alpha, double beta)
    {
        size_t rows = img1.get_rows();
        size_t cols = img1.get_cols();
        int channels = img1.get_channels();

        for (size_t r = 0; r < rows; ++r)
        {
            // 获取行指针并转换为目标类型 T*
            const T *ptr1 = reinterpret_cast<const T *>(img1.ptr(r));
            const T *ptr2 = reinterpret_cast<const T *>(img2.ptr(r));
            T *out_ptr = reinterpret_cast<T *>(output.ptr(r));

            for (size_t c = 0; c < cols * channels; ++c)
            {
                // 执行混合计算
                // 对于浮点数，直接计算
                if constexpr (std::is_floating_point_v<T>)
                {
                    out_ptr[c] = static_cast<T>(alpha * static_cast<double>(ptr1[c]) + beta * static_cast<double>(ptr2[c]));
                }
                // 对于整数类型，使用 double 进行中间计算并进行饱和运算
                else if constexpr (std::is_integral_v<T>)
                {
                    double blended_val = alpha * static_cast<double>(ptr1[c]) + beta * static_cast<double>(ptr2[c]);

                    // 获取类型的最大最小值
                    constexpr double min_val = static_cast<double>(std::numeric_limits<T>::min());
                    constexpr double max_val = static_cast<double>(std::numeric_limits<T>::max());

                    // 四舍五入并截断到类型的有效范围
                    blended_val = std::round(blended_val); // 对混合结果四舍五入
                    blended_val = std::max(min_val, std::min(max_val, blended_val));

                    out_ptr[c] = static_cast<T>(blended_val); // 转回目标类型 T
                }
                else
                {
                    // 理论上不会执行到这里，因为主函数会检查类型
                    throw std::runtime_error("blend_images_typed: 不支持的数据类型。");
                }
            }
        }
    }

    // 主 blend_images 函数，负责检查和类型分发
    void blend_images(const Image &img1, const Image &img2, Image &output, double alpha)
    {
        Image::check_compatibility(img1, img2); // 检查尺寸和类型是否兼容

        // 确保 alpha 在 [0, 1] 范围内
        alpha = std::max(0.0, std::min(1.0, alpha));
        double beta = 1.0 - alpha;

        size_t rows = img1.get_rows();
        size_t cols = img1.get_cols();
        int type = img1.get_type();
        int depth = img1.get_depth();

        output.create(rows, cols, type); // 确保输出图像尺寸和类型正确

        // 根据图像深度调用相应的模板实例
        switch (depth)
        {
        case IMG_8U:
            blend_images_typed<unsigned char>(img1, img2, output, alpha, beta);
            break;
        case IMG_16U: // 示例：添加对 16 位无符号整数的支持
            blend_images_typed<unsigned short>(img1, img2, output, alpha, beta);
            break;
        case IMG_32S: // 示例：添加对 32 位有符号整数的支持
            blend_images_typed<int>(img1, img2, output, alpha, beta);
            break;
        case IMG_32F:
            blend_images_typed<float>(img1, img2, output, alpha, beta);
            break;
        case IMG_64F: // 示例：添加对 64 位浮点数的支持
            blend_images_typed<double>(img1, img2, output, alpha, beta);
            break;
        // 可以根据需要添加对 IMG_8S, IMG_16S 等其他类型的支持
        default:
            throw std::runtime_error("blend_images: 不支持的图像深度类型。");
        }
    }

    // --- 实现 rotate_images ---
    Image rotate_images(const Image &src, int times)
    {
        if (src.empty())
        {
            throw std::runtime_error("rotate_images: 输入图像为空。");
        }

        times %= 4;
        if (times < 0)
            times += 4;

        if (times == 0)
        {
            return src.clone(); // 返回副本
        }

        size_t src_rows = src.get_rows();
        size_t src_cols = src.get_cols();
        int type = src.get_type();
        size_t pixel_size = src.get_pixel_size();
        size_t src_step = src.get_step();

        size_t dst_rows = src_rows;
        size_t dst_cols = src_cols;

        if (times == 1 || times == 3) // 90 或 270 度旋转
        {
            dst_rows = src_cols;
            dst_cols = src_rows;
        }

        Image dst(dst_rows, dst_cols, type);
        size_t dst_step = dst.get_step();

        for (size_t src_r = 0; src_r < src_rows; ++src_r)
        {
            const unsigned char *src_row_ptr = src.ptr(src_r);
            for (size_t src_c = 0; src_c < src_cols; ++src_c)
            {
                size_t dst_r = 0, dst_c = 0;
                switch (times)
                {
                case 1: // 顺时针90度
                    dst_r = src_c;
                    dst_c = src_rows - 1 - src_r;
                    break;
                case 2: // 180度
                    dst_r = src_rows - 1 - src_r;
                    dst_c = src_cols - 1 - src_c;
                    break;
                case 3: // 顺时针270度 (逆时针90度)
                    dst_r = src_cols - 1 - src_c;
                    dst_c = src_r;
                    break;
                }

                const unsigned char *src_pixel_ptr = src_row_ptr + src_c * pixel_size;
                unsigned char *dst_pixel_ptr = dst.ptr(dst_r) + dst_c * pixel_size;
                memcpy(dst_pixel_ptr, src_pixel_ptr, pixel_size);
            }
        }

        return dst;
    }

    // --- 实现 resize_images (临近插值) ---
    Image resize_images(const Image &src, size_t new_rows, size_t new_cols)
    {
        if (src.empty())
        {
            throw std::runtime_error("resize_images: 输入图像为空。");
        }
        if (new_rows == 0 || new_cols == 0)
        {
            throw std::runtime_error("resize_images: 目标尺寸无效。");
        }

        size_t src_rows = src.get_rows();
        size_t src_cols = src.get_cols();
        int type = src.get_type();
        size_t pixel_size = src.get_pixel_size();
        size_t src_step = src.get_step();

        Image dst(new_rows, new_cols, type);
        size_t dst_step = dst.get_step();

        double y_ratio = static_cast<double>(src_rows) / new_rows;
        double x_ratio = static_cast<double>(src_cols) / new_cols;

        for (size_t dst_r = 0; dst_r < new_rows; ++dst_r)
        {
            unsigned char *dst_row_ptr = dst.ptr(dst_r);
            // 计算对应的源行索引 (临近插值)
            size_t src_r = static_cast<size_t>(std::floor(dst_r * y_ratio));
            // 边界检查
            src_r = std::min(src_r, src_rows - 1);

            const unsigned char *src_row_ptr = src.ptr(src_r);

            for (size_t dst_c = 0; dst_c < new_cols; ++dst_c)
            {
                // 计算对应的源列索引 (临近插值)
                size_t src_c = static_cast<size_t>(std::floor(dst_c * x_ratio));
                // 边界检查
                src_c = std::min(src_c, src_cols - 1);

                const unsigned char *src_pixel_ptr = src_row_ptr + src_c * pixel_size;
                unsigned char *dst_pixel_ptr = dst_row_ptr + dst_c * pixel_size;
                memcpy(dst_pixel_ptr, src_pixel_ptr, pixel_size);
            }
        }

        return dst;
    }

} // namespace img