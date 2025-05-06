#include "imglib.h" // 包含头文件
#include <stdexcept>
#include <vector>
#include <cmath>   // for std::round, std::floor, std::abs
#include <cstring> // for memcpy
#include <limits>  // for numeric_limits
#include <algorithm> // for std::min, std::max

namespace img
{

    // --- 实现 adjust_brightness ---
    // (这个函数的实现可能已存在于您的代码库中，如果存在则无需重复添加)
    // 如果不存在，可以参考以下实现框架：
    void adjust_brightness(Image &img, double value)
    {
        if (img.empty())
        {
            throw std::runtime_error("adjust_brightness: 输入图像为空。");
        }

        size_t rows = img.get_rows();
        size_t cols = img.get_cols();
        int type = img.get_type();
        int depth = img.get_depth();
        int channels = img.get_channels();
        size_t pixel_size = img.get_pixel_size();

        // 根据深度类型处理
        switch (depth)
        {
        case IMG_8U:
        {
            int int_value = static_cast<int>(std::round(value)); // 四舍五入到整数
            for (size_t r = 0; r < rows; ++r)
            {
                unsigned char *row_ptr = img.ptr(r);
                for (size_t c = 0; c < cols * channels; ++c) // 直接遍历所有通道字节
                {
                    int new_val = static_cast<int>(row_ptr[c]) + int_value;
                    row_ptr[c] = static_cast<unsigned char>(std::max(0, std::min(255, new_val))); // 截断到 [0, 255]
                }
            }
            break;
        }
        case IMG_32F:
        {
            float float_value = static_cast<float>(value);
             for (size_t r = 0; r < rows; ++r)
            {
                float* row_ptr = reinterpret_cast<float*>(img.ptr(r));
                 for (size_t c = 0; c < cols * channels; ++c)
                {
                    row_ptr[c] += float_value;
                    // 浮点数通常不截断，除非有特定要求
                }
            }
            break;
        }
        // 添加对其他类型 (IMG_16U, IMG_32S, IMG_64F) 的支持...
        default:
            throw std::runtime_error("adjust_brightness: 不支持的图像深度类型。");
        }
    }

    // --- 实现 blend_images ---
    // (这个函数的实现可能已存在于您的代码库中，如果存在则无需重复添加)
    // 如果不存在，可以参考以下实现框架：
    void blend_images(const Image &img1, const Image &img2, Image &output, double alpha)
    {
        Image::check_compatibility(img1, img2); // 检查尺寸和类型是否兼容

        if (alpha < 0.0) alpha = 0.0;
        if (alpha > 1.0) alpha = 1.0;
        double beta = 1.0 - alpha;

        size_t rows = img1.get_rows();
        size_t cols = img1.get_cols();
        int type = img1.get_type();
        int depth = img1.get_depth();
        int channels = img1.get_channels();

        output.create(rows, cols, type); // 确保输出图像尺寸和类型正确

        // 根据深度类型处理
        switch (depth)
        {
        case IMG_8U:
        {
            for (size_t r = 0; r < rows; ++r)
            {
                const unsigned char *ptr1 = img1.ptr(r);
                const unsigned char *ptr2 = img2.ptr(r);
                unsigned char *out_ptr = output.ptr(r);
                for (size_t c = 0; c < cols * channels; ++c)
                {
                    double blended_val = alpha * static_cast<double>(ptr1[c]) + beta * static_cast<double>(ptr2[c]);
                    out_ptr[c] = static_cast<unsigned char>(std::max(0.0, std::min(255.0, std::round(blended_val))));
                }
            }
            break;
        }
        case IMG_32F:
        {
             for (size_t r = 0; r < rows; ++r)
            {
                const float *ptr1 = reinterpret_cast<const float*>(img1.ptr(r));
                const float *ptr2 = reinterpret_cast<const float*>(img2.ptr(r));
                float *out_ptr = reinterpret_cast<float*>(output.ptr(r));
                 for (size_t c = 0; c < cols * channels; ++c)
                {
                    out_ptr[c] = static_cast<float>(alpha * ptr1[c] + beta * ptr2[c]);
                }
            }
            break;
        }
        // 添加对其他类型的支持...
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
        if (times < 0) times += 4;

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