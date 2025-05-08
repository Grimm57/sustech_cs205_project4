#include "../include/imglib.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <cstring>

namespace img
{
    ////////////// 工厂中支持的文件类型 //////////////
    class BMPHandler : public img::ImageIOHandler
    {

    // 内置对于BMP图像的支持
#pragma pack(push, 1)
        typedef struct
        {
            uint16_t bfType;
            uint32_t bfSize;
            uint16_t bfReserved1;
            uint16_t bfReserved2;
            uint32_t bfOffBits;
        } BMPFileHeader;

        typedef struct
        {
            uint32_t biSize;
            int32_t biWidth;
            int32_t biHeight;
            uint16_t biPlanes;
            uint16_t biBitCount;
            uint32_t biCompression;
            uint32_t biSizeImage;
            int32_t biXPelsPerMeter;
            int32_t biYPelsPerMeter;
            uint32_t biClrUsed;
            uint32_t biClrImportant;
        } BMPInfoHeader;
#pragma pack(pop)

    public:
        Image read(const std::string &filename) const override
        {
            std::ifstream file(filename, std::ios::binary);
            if (!file.is_open())
            {
                std::cerr << "BMP 读取错误: 无法打开文件: " << filename << std::endl;
                return Image(); // 如果没有正确的读取会返回空图片
            }

            BMPFileHeader fileHeader = {};
            BMPInfoHeader infoHeader = {};

            file.read(reinterpret_cast<char *>(&fileHeader), sizeof(BMPFileHeader));

            if (!file || file.gcount() != sizeof(BMPFileHeader))
            {
                // 用户报错
                std::cerr << "BMP 读取错误: 读取文件头失败: " << filename << std::endl;
                // // 调试报错
                // std::cerr << "BMPHandler::read 错误: 从文件 " << filename << " 读取 BMP 文件头失败 (预期 " << sizeof(BMPFileHeader) << " 字节, 实际读取 " << file.gcount() << " 字节)." << std::endl;
                return Image();
            }

            if (fileHeader.bfType != 0x4D42) // 'BM'
            {
                // 用户报错
                std::cerr << "BMP 读取错误: 文件格式无效 (不是 BMP 文件): " << filename << std::endl;
                // // 调试报错
                // std::cerr << "BMPHandler::read 错误: 文件 " << filename << " 不是有效的 BMP 文件 (文件头标识 bfType 不是 0x4D42)." << std::endl;
                return Image();
            }

            file.read(reinterpret_cast<char *>(&infoHeader), sizeof(BMPInfoHeader));
            if (!file || file.gcount() != sizeof(BMPInfoHeader))
            {
                // 用户报错
                std::cerr << "BMP 读取错误: 读取信息头失败: " << filename << std::endl;
                // // 调试报错
                // std::cerr << "BMPHandler::read 错误: 从文件 " << filename << " 读取 BMP 信息头失败 (预期 " << sizeof(BMPInfoHeader) << " 字节, 实际读取 " << file.gcount() << " 字节)." << std::endl;
                return Image();
            }

            if ((infoHeader.biBitCount != 24 && infoHeader.biBitCount != 32) || infoHeader.biCompression != 0)
            {
                // 用户报错
                std::cerr << "BMP 读取错误: 不支持的 BMP 格式 (仅支持 24/32 位未压缩图像): " << filename << std::endl;
                // // 调试报错
                // std::cerr << "BMPHandler::read 错误: 文件 " << filename << " 包含不支持的 BMP 格式 (位深度 biBitCount=" << infoHeader.biBitCount << ", 压缩方式 biCompression=" << infoHeader.biCompression << "). 仅支持 24 位或 32 位未压缩图像." << std::endl;
                return Image();
            }
            if (infoHeader.biWidth <= 0 || infoHeader.biHeight == 0)
            {
                // 用户报错
                std::cerr << "BMP 读取错误: 无效的图像尺寸: " << filename << std::endl;
                // // 调试报错
                // std::cerr << "BMPHandler::read 错误: 文件 " << filename << " 包含无效的图像尺寸 (宽度 biWidth=" << infoHeader.biWidth << ", 高度 biHeight=" << infoHeader.biHeight << ")." << std::endl;
                return Image();
            }

            int width = infoHeader.biWidth;
            int height = std::abs(infoHeader.biHeight);
            bool topDown = (infoHeader.biHeight < 0);
            int channels = infoHeader.biBitCount / 8;
            int imageType = IMG_MAKETYPE(IMG_8U, channels);

            // 使用 Image(rows, cols, type) 构造函数
            Image img(height, width, imageType);
            if (img.empty())
            {
                // 用户报错 (这个比较内部，可能还是调试信息更合适)
                std::cerr << "BMP 处理错误: 创建内部图像对象失败." << std::endl;
                // // 调试报错
                // std::cerr << "BMPHandler::read 错误: 尝试为文件 " << filename << " 创建 Image 对象 (尺寸 " << height << "x" << width << ", 类型 " << imageType << ") 时失败." << std::endl;
                return Image();
            }

            int rowPadding = (4 - (width * channels) % 4) % 4;
            int stride = width * channels + rowPadding; // BMP 行必须是 4 字节对齐

            file.seekg(fileHeader.bfOffBits, std::ios::beg);
            if (!file)
            {
                // 用户报错
                std::cerr << "BMP 读取错误: 定位像素数据失败: " << filename << std::endl;
                // // 调试报错
                // std::cerr << "BMPHandler::read 错误: 在文件 " << filename << " 中定位到像素数据起始位置 (bfOffBits=" << fileHeader.bfOffBits << ") 失败." << std::endl;
                return Image();
            }

            std::vector<unsigned char> rowBuffer(stride);
            for (int y = 0; y < height; ++y)
            {
                int destY = topDown ? y : (height - 1 - y);
                // 使用 ptr() 获取行指针
                unsigned char *destRowPtr = img.ptr(destY);

                file.read(reinterpret_cast<char *>(rowBuffer.data()), stride);
                if (!file || file.gcount() != stride)
                {
                    // 用户报错
                    std::cerr << "BMP 读取错误: 读取像素数据行失败: " << filename << std::endl;
                    // // 调试报错
                    // std::cerr << "BMPHandler::read 错误: 从文件 " << filename << " 读取像素数据行 " << y << " 失败 (预期 " << stride << " 字节, 实际读取 " << file.gcount() << " 字节)." << std::endl;
                    return Image();
                }
                // 只复制实际图像数据，忽略填充字节
                memcpy(destRowPtr, rowBuffer.data(), width * channels);
            }

            return img;
        }

        // 实现 write 方法
        bool write(const std::string &filename, const Image &img, const std::vector<int> &params) const override
        {
            if (img.empty())
            {
                // 用户报错
                std::cerr << "BMP 写入错误: 输入图像为空，无法写入。" << std::endl;
                // // 调试报错
                // std::cerr << "BMPHandler::write 错误: 尝试写入的 Image 对象为空." << std::endl;
                return false;
            }

            if (img.get_type() != IMG_MAKETYPE(IMG_8U, 3) && img.get_type() != IMG_MAKETYPE(IMG_8U, 4))
            {
                // 用户报错
                std::cerr << "BMP 写入错误: 不支持的BMP图像类型 (仅支持 8UC3 或 8UC4 格式)。" << std::endl;
                // // 调试报错
                // std::cerr << "BMPHandler::write 错误: 尝试写入的 Image 对象类型 (" << img.get_type_string() << ") 不支持 BMP 格式。必须是 8UC3 或 8UC4." << std::endl;
                return false;
            }

            std::ofstream file(filename, std::ios::binary);
            if (!file.is_open())
            {
                // 用户报错
                std::cerr << "BMP 写入错误: 无法打开文件进行写入: " << filename << std::endl;
                // // 调试报错
                // std::cerr << "BMPHandler::write 错误: 无法以二进制模式打开文件 '" << filename << "' 进行写入." << std::endl;
                return false;
            }

            // 使用 rows() 和 cols() 方法
            int width = img.get_cols();
            int height = img.get_rows();
            int channels = img.get_channels();
            int bitCount = channels * 8;

            int rowPadding = (4 - (width * channels) % 4) % 4;
            int stride = width * channels + rowPadding;
            uint32_t imageSize = static_cast<uint32_t>(stride) * height;

            BMPFileHeader fileHeader = {}; // 使用初始化列表清零
            fileHeader.bfType = 0x4D42;    // 'BM'
            fileHeader.bfSize = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + imageSize;
            fileHeader.bfReserved1 = 0;
            fileHeader.bfReserved2 = 0;
            fileHeader.bfOffBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);

            BMPInfoHeader infoHeader = {}; // 使用初始化列表清零
            infoHeader.biSize = sizeof(BMPInfoHeader);
            infoHeader.biWidth = width;
            // BMP 规范中，正的 biHeight 表示图像数据从下到上存储 (Bottom-Up DIB)
            // 写入时我们通常按从下到上的顺序写入行，所以 biHeight 为正值
            infoHeader.biHeight = height;
            infoHeader.biPlanes = 1;
            infoHeader.biBitCount = bitCount;
            infoHeader.biCompression = 0; // BI_RGB (未压缩)
            infoHeader.biSizeImage = imageSize;
            infoHeader.biXPelsPerMeter = 0; // 通常设为 0
            infoHeader.biYPelsPerMeter = 0; // 通常设为 0
            infoHeader.biClrUsed = 0;       // 对于非调色板图像为 0
            infoHeader.biClrImportant = 0;  // 通常为 0

            file.write(reinterpret_cast<const char *>(&fileHeader), sizeof(BMPFileHeader));
            file.write(reinterpret_cast<const char *>(&infoHeader), sizeof(BMPInfoHeader));
            if (!file)
            {
                // 用户报错
                std::cerr << "BMP 写入错误: 写入文件头或信息头失败: " << filename << std::endl;
                // // 调试报错
                // std::cerr << "BMPHandler::write 错误: 向文件 '" << filename << "' 写入 BMP 文件头或信息头时发生错误." << std::endl;
                return false;
            }

            // 准备一个带填充的行缓冲区
            std::vector<unsigned char> rowBuffer(stride, 0); // 初始化为 0 以填充

            // BMP 数据通常从下到上存储，所以我们从图像的最后一行开始读取并写入
            for (int y = height - 1; y >= 0; --y)
            {
                // 使用 ptr() 获取常量行指针
                const unsigned char *srcRowPtr = img.ptr(y);

                // 将图像数据复制到缓冲区
                memcpy(rowBuffer.data(), srcRowPtr, width * channels);
                // 填充字节已经在 rowBuffer 初始化时设为 0，无需额外处理

                file.write(reinterpret_cast<const char *>(rowBuffer.data()), stride);
                if (!file)
                {
                    // 用户报错
                    std::cerr << "BMP 写入错误: 写入像素数据行失败: " << filename << std::endl;
                    // // 调试报错
                    // std::cerr << "BMPHandler::write 错误: 向文件 '" << filename << "' 写入像素数据行 " << (height - 1 - y) << " (源图像行 " << y << ") 时发生错误." << std::endl;
                    return false;
                }
            }

            // 检查最终写入状态
            if (!file.good())
            {
                 // 用户报错
                std::cerr << "BMP 写入错误: 文件写入操作未完全成功: " << filename << std::endl;
                // // 调试报错
                // std::cerr << "BMPHandler::write 错误: 文件 '" << filename << "' 在写入完成后状态不是 good()." << std::endl;
                return false;
            }

            return true; // 成功写入
        }

        // 实现 getSupportedExtensions 方法
        std::vector<std::string> getSupportedExtensions() const override
        {
            return {"bmp"};
        }
    };

    ////////////// 图像 I/O 处理器工厂 //////////////
    ImageIOFactory &ImageIOFactory::getInstance()
    {

        static ImageIOFactory instance;
        static bool handlers_registered = false;

        if (!handlers_registered)
        {
            try
            {
                instance.registerHandler(std::make_unique<BMPHandler>());
                // 在这里可以添加注册其他处理器的代码
                // instance.registerHandler(std::make_unique<PNGHandler>());
                // instance.registerHandler(std::make_unique<JPEGHandler>());
                handlers_registered = true;
            }
            catch (const std::exception &e)
            {
                // 这个错误比较内部，主要给开发者看
                std::cerr << "图像库初始化错误: 注册图像处理器时出错: " << e.what() << std::endl;
                // // 调试报错
                // std::cerr << "ImageIOFactory::getInstance 错误: 在自动注册处理器期间捕获到异常: " << e.what() << std::endl;

                // 即使注册失败，也标记为已尝试注册，防止无限循环
                handlers_registered = true;
                // 这里可以考虑是否应该抛出异常，让应用程序知道初始化失败
                // throw std::runtime_error("图像库初始化失败: 无法注册必要的图像处理器.");
            }
        }

        return instance;
    }

    std::string ImageIOFactory::getFileExtensionLower(const std::string &filename)
    {
        size_t dot_pos = filename.find_last_of('.');
        // 如果找不到点，或者点是第一个字符（隐藏文件），则没有扩展名
        if (dot_pos == std::string::npos || dot_pos == 0 || dot_pos == filename.length() - 1)
        {
            return "";
        }
        std::string ext = filename.substr(dot_pos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c)
                       { return std::tolower(c); });
        return ext;
    }

    void ImageIOFactory::registerHandler(std::unique_ptr<ImageIOHandler> handler)
    {
        if (!handler)
        {
            // // 调试报错 (内部逻辑错误)
            // std::cerr << "ImageIOFactory::registerHandler 警告: 尝试注册空的处理器指针。" << std::endl;
            return; // 忽略空指针
        }

        std::vector<std::string> extensions = handler->getSupportedExtensions();
        if (extensions.empty())
        {
            // // 调试报错 (处理器实现问题)
            // std::cerr << "ImageIOFactory::registerHandler 警告: 尝试注册一个不支持任何文件扩展名的处理器。" << std::endl;
            return; // 忽略没有扩展名的处理器
        }

        // 获取裸指针以存入映射，所有权仍在 unique_ptr 中
        ImageIOHandler *raw_handler_ptr = handler.get();

        for (const std::string &ext_const : extensions)
        {
            // 转换为小写以进行不区分大小写的匹配
            std::string ext = ext_const;
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c)
                           { return std::tolower(c); });

            if (m_handlers_by_ext.count(ext))
            {
                // 这是一个严重配置错误，应该抛出异常
                // // 调试报错
                // std::cerr << "ImageIOFactory::registerHandler 错误: 扩展名 '" << ext << "' 的处理器注册冲突。已存在一个处理器。" << std::endl;
                throw std::runtime_error("图像库配置错误: 文件扩展名 '" + ext + "' 存在重复的处理器注册。");
            }
            m_handlers_by_ext[ext] = raw_handler_ptr;
        }

        // 将处理器的所有权转移给工厂
        m_registered_handlers.push_back(std::move(handler));
    }

    ImageIOHandler *ImageIOFactory::getHandler(const std::string &filename) const
    {
        std::string ext = getFileExtensionLower(filename);
        if (ext.empty())
        {
            // 用户报错 (如果 imread/imwrite 调用)
            // std::cerr << "图像处理错误: 文件 '" << filename << "' 没有可识别的文件扩展名。" << std::endl;
            // // 调试报错 (或者仅日志记录)
            // std::cerr << "ImageIOFactory::getHandler: 文件 '" << filename << "' 没有扩展名，无法确定处理器。" << std::endl;
            return nullptr; // 无法确定处理器
        }

        auto it = m_handlers_by_ext.find(ext);
        if (it != m_handlers_by_ext.end())
        {
            return it->second; // 找到处理器
        }
        else
        {
            // 用户报错 (如果 imread/imwrite 调用)
            // std::cerr << "图像处理错误: 不支持文件扩展名 '" << ext << "' (来自文件 '" << filename << "')." << std::endl;
            // // 调试报错 (或日志)
            // std::cerr << "ImageIOFactory::getHandler: 未找到支持扩展名 '" << ext << "' (来自文件 '" << filename << "') 的注册处理器。" << std::endl;
            return nullptr; // 未找到支持该扩展名的处理器
        }
    }

    ////////////// 顶层图像 I/O 函数 //////////////
    Image imread(const std::string &filename)
    {
        ImageIOFactory &factory = ImageIOFactory::getInstance();
        ImageIOHandler *handler = factory.getHandler(filename);

        if (handler)
        {
            try
            {
                return handler->read(filename);
            }
            catch (const std::exception &e)
            {
                // 用户报错
                std::cerr << "读取图像错误: 处理文件 '" << filename << "' 时发生异常: " << e.what() << std::endl;
                // // 调试报错 (可能与 handler->read 内部的调试信息重复，但提供调用上下文)
                // std::cerr << "imread 错误: 调用处理器读取文件 '" << filename << "' 时捕获到 std::exception: " << e.what() << std::endl;
                return Image(); // 返回空图像表示失败
            }
            catch (...)
            {
                // 用户报错
                std::cerr << "读取图像错误: 处理文件 '" << filename << "' 时发生未知错误。" << std::endl;
                // // 调试报错
                // std::cerr << "imread 错误: 调用处理器读取文件 '" << filename << "' 时捕获到未知类型的异常。" << std::endl;
                return Image(); // 返回空图像表示失败
            }
        }
        else
        {
            // 用户报错
            std::cerr << "读取图像错误: 无法找到支持文件 '" << filename << "' 格式的处理器。" << std::endl;
            // // 调试报错 (可能已在 getHandler 中记录)
            // std::cerr << "imread 错误: 未能获取文件 '" << filename << "' 的处理器。" << std::endl;
            return Image(); // 返回空图像表示失败
        }
    }

    bool imwrite(const std::string &filename, const Image &img, const std::vector<int> &params)
    {
        if (img.empty())
        {
             // 用户报错
            std::cerr << "写入图像错误: 尝试写入的图像数据为空。" << std::endl;
            // // 调试报错
            // std::cerr << "imwrite 错误: 传递给 imwrite 的 Image 对象为空，无法写入文件 '" << filename << "'。" << std::endl;
            return false; // 不能写空图像
        }

        ImageIOFactory &factory = ImageIOFactory::getInstance();
        ImageIOHandler *handler = factory.getHandler(filename);

        if (handler)
        {
            try
            {
                return handler->write(filename, img, params);
            }
            catch (const std::exception &e)
            {
                // 用户报错
                std::cerr << "写入图像错误: 处理文件 '" << filename << "' 时发生异常: " << e.what() << std::endl;
                // // 调试报错
                // std::cerr << "imwrite 错误: 调用处理器写入文件 '" << filename << "' 时捕获到 std::exception: " << e.what() << std::endl;
                return false; // 写入失败
            }
            catch (...)
            {
                 // 用户报错
                std::cerr << "写入图像错误: 处理文件 '" << filename << "' 时发生未知错误。" << std::endl;
                // // 调试报错
                // std::cerr << "imwrite 错误: 调用处理器写入文件 '" << filename << "' 时捕获到未知类型的异常。" << std::endl;
                return false; // 写入失败
            }
        }
        else
        {
            // 用户报错
            std::cerr << "写入图像错误: 无法找到支持文件 '" << filename << "' 格式的处理器。" << std::endl;
            // // 调试报错 (可能已在 getHandler 中记录)
            // std::cerr << "imwrite 错误: 未能获取文件 '" << filename << "' 的处理器。" << std::endl;
            return false; // 写入失败
        }
    }
} // namespace img