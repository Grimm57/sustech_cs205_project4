#include "imglib.h" // 包含头文件
#include <stdexcept>
#include <vector>
#include <cmath>     // for std::round, std::floor, std::abs
#include <cstring>   // for memcpy
#include <limits>    // for numeric_limits
#include <algorithm> // for std::min, std::max

namespace img
{

    void blend_images(const Image &img1, const Image &img2, Image &output, double alpha)
    {
        // 上述检查在 Image::operator+ 中已经通过 Image::operator+= 间接调用了 check_compatibility，可以省略。
        alpha = std::max(0.0, std::min(1.0, alpha));
        double beta = 1.0 - alpha;
        std::string F_NAME = "blend_images";
        try
        {
            Image::check_compatibility(img1, img2, "blend"); // 检查尺寸和类型是否兼容
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error(IMG_ERROR_PREFIX(F_NAME) + "图像不兼容: " + e.what());
        }

        int num_channels = img1.get_channels();
        int target_type_double = IMG_MAKETYPE(IMG_64F, num_channels);
        try
        {
            // 将输入图像转换为双精度类型，然后执行混合操作
            Image term1_double = img1.convert_to(target_type_double);
            Image term2_double = img2.convert_to(target_type_double);

            output = (term1_double * alpha) + (term2_double * beta);
            output.convert_to(img1.get_type());
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error(IMG_ERROR_PREFIX(F_NAME) + "混合图像时发生错误: " + e.what());
        }
    }
    /**
     * @brief 调整图像的亮度。
     * 此函数通过将指定值加到图像的每个像素的每个通道上（包括Alpha通道，如果存在）来调整亮度。
     * 它利用 Image 类的 operator+= 实现。
     * @param img 要修改的图像 (in-place)。
     * @param value 要增加或减少的亮度值。正值增加亮度，负值减少亮度。
     * @throw std::runtime_error 如果输入图像为空，或者在操作过程中发生其他错误。
     */
    void adjust_brightness(Image &img, double value)
    {
        const std::string F_NAME = "adjust_brightness";
        if (img.empty())
        {
            // Image::operator+= 内部也会检查空图像，但在此处提供更明确的上下文错误信息
            throw std::runtime_error(IMG_ERROR_PREFIX(F_NAME) + "输入图像为空，无法调整亮度。");
        }

        try
        {
            // 使用 Image 类重载的 operator+= 来调整亮度
            // operator+= 内部会处理不同数据类型、饱和计算（截断）以及可能的错误
            img += value;
        }
        catch (const std::exception &e)
        {
            // 捕获来自 operator+= 的任何异常，并附加当前函数的上下文信息
            throw std::runtime_error(IMG_ERROR_PREFIX(F_NAME) + "调整图像亮度时发生错误: " + e.what());
        }
    }
} // namespace img