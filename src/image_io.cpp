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

    // --- BMPHandler 实现 ---
    class BMPHandler : public img::ImageIOHandler
    {
        // --- BMP 文件头结构体 (复用 C 代码定义) ---
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
        // 实现 read 方法
        Image read(const std::string &filename) const override
        {
            std::ifstream file(filename, std::ios::binary);
            if (!file.is_open())
            {
                std::cerr << "BMPHandler Error: Cannot open file: " << filename << std::endl;
                return Image();//如果没有正确的读取会返回空图片
            }

            BMPFileHeader fileHeader = {};
            BMPInfoHeader infoHeader = {};

            file.read(reinterpret_cast<char *>(&fileHeader), sizeof(BMPFileHeader));
            
            if (!file || file.gcount() != sizeof(BMPFileHeader))
            {
                std::cerr << "BMPHandler Error: Failed to read file header from " << filename << std::endl;
                return Image();
            }

            if (fileHeader.bfType != 0x4D42) // 'BM'
            {
                std::cerr << "BMPHandler Error: Not a valid BMP file: " << filename << std::endl;
                return Image();
            }

            file.read(reinterpret_cast<char *>(&infoHeader), sizeof(BMPInfoHeader));
            if (!file || file.gcount() != sizeof(BMPInfoHeader))
            {
                std::cerr << "BMPHandler Error: Failed to read info header from " << filename << std::endl;
                return Image();
            }

            if ((infoHeader.biBitCount != 24 && infoHeader.biBitCount != 32) || infoHeader.biCompression != 0)
            {
                std::cerr << "BMPHandler Error: Unsupported BMP format (must be 24/32 bit, uncompressed) in " << filename << std::endl;
                return Image();
            }
            if (infoHeader.biWidth <= 0 || infoHeader.biHeight == 0)
            {
                std::cerr << "BMPHandler Error: Invalid image dimensions in " << filename << std::endl;
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
                std::cerr << "BMPHandler Error: Failed to create Image object." << std::endl;
                return Image();
            }

            int rowPadding = (4 - (width * channels) % 4) % 4;
            int stride = width * channels + rowPadding;

            file.seekg(fileHeader.bfOffBits, std::ios::beg);
            if (!file)
            {
                std::cerr << "BMPHandler Error: Failed to seek to pixel data in " << filename << std::endl;
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
                    std::cerr << "BMPHandler Error: Failed to read pixel data row " << y << " from " << filename << std::endl;
                    return Image();
                }

                memcpy(destRowPtr, rowBuffer.data(), width * channels);
            }

            return img;
        }

        // 实现 write 方法
        bool write(const std::string &filename, const Image &img, const std::vector<int> &params) const override
        {
            if (img.empty())
            {
                std::cerr << "BMPHandler Error: Input image is empty." << std::endl;
                return false;
            }

            if (img.get_type() != IMG_MAKETYPE(IMG_8U, 3) && img.get_type() != IMG_MAKETYPE(IMG_8U, 4))
            {
                std::cerr << "BMPHandler Error: Unsupported image type for BMP writing. Must be 8UC3 or 8UC4." << std::endl;
                return false;
            }

            std::ofstream file(filename, std::ios::binary);
            if (!file.is_open())
            {
                std::cerr << "BMPHandler Error: Cannot open file for writing: " << filename << std::endl;
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

            BMPFileHeader fileHeader;
            fileHeader.bfType = 0x4D42;
            fileHeader.bfSize = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + imageSize;
            fileHeader.bfReserved1 = 0;
            fileHeader.bfReserved2 = 0;
            fileHeader.bfOffBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);

            BMPInfoHeader infoHeader;
            infoHeader.biSize = sizeof(BMPInfoHeader);
            infoHeader.biWidth = width;
            infoHeader.biHeight = height; // 正值表示从下到上
            infoHeader.biPlanes = 1;
            infoHeader.biBitCount = bitCount;
            infoHeader.biCompression = 0;
            infoHeader.biSizeImage = imageSize;
            infoHeader.biXPelsPerMeter = 0;
            infoHeader.biYPelsPerMeter = 0;
            infoHeader.biClrUsed = 0;
            infoHeader.biClrImportant = 0;

            file.write(reinterpret_cast<const char *>(&fileHeader), sizeof(BMPFileHeader));
            file.write(reinterpret_cast<const char *>(&infoHeader), sizeof(BMPInfoHeader));
            if (!file)
            {
                std::cerr << "BMPHandler Error: Failed to write headers to " << filename << std::endl;
                return false;
            }

            std::vector<unsigned char> rowBuffer(stride, 0);

            for (int y = height - 1; y >= 0; --y)
            {
                // 使用 ptr() 获取常量行指针
                const unsigned char *srcRowPtr = img.ptr(y);

                memcpy(rowBuffer.data(), srcRowPtr, width * channels);

                file.write(reinterpret_cast<const char *>(rowBuffer.data()), stride);
                if (!file)
                {
                    std::cerr << "BMPHandler Error: Failed to write pixel data row " << y << " to " << filename << std::endl;
                    return false;
                }
            }

            return file.good();
        }

        // 实现 getSupportedExtensions 方法
        std::vector<std::string> getSupportedExtensions() const override
        {
            return {"bmp"};
        }
    };

    ImageIOFactory &ImageIOFactory::getInstance()
    {

        static ImageIOFactory instance;
        static bool handlers_registered = false;

        if (!handlers_registered)
        {
            try
            {
                instance.registerHandler(std::make_unique<BMPHandler>());
                handlers_registered = true;
            }
            catch (const std::exception &e)
            {

                std::cerr << "Error during handler auto-registration: " << e.what() << std::endl;
                handlers_registered = true;
            }
        }

        return instance;
    }

    std::string ImageIOFactory::getFileExtensionLower(const std::string &filename)
    {
        size_t dot_pos = filename.find_last_of('.');
        if (dot_pos == std::string::npos || dot_pos == 0)
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
            return;
        }

        std::vector<std::string> extensions = handler->getSupportedExtensions();
        if (extensions.empty())
        {
            return;
        }

        ImageIOHandler *raw_handler_ptr = handler.get();

        for (const std::string &ext_const : extensions)
        {
            std::string ext = ext_const;
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c)
                           { return std::tolower(c); });

            if (m_handlers_by_ext.count(ext))
            {

                throw std::runtime_error("ImageIOFactory: Handler registration conflict for extension '" + ext + "'.");
            }
            m_handlers_by_ext[ext] = raw_handler_ptr;
        }

        m_registered_handlers.push_back(std::move(handler));
    }

    ImageIOHandler *ImageIOFactory::getHandler(const std::string &filename) const
    {
        std::string ext = getFileExtensionLower(filename);
        if (ext.empty())
        {

            return nullptr;
        }

        auto it = m_handlers_by_ext.find(ext);
        if (it != m_handlers_by_ext.end())
        {
            return it->second;
        }
        else
        {

            return nullptr;
        }
    }

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
                std::cerr << "Error reading image '" << filename << "': " << e.what() << std::endl;
                return Image();
            }
            catch (...)
            {
                std::cerr << "Unknown error reading image '" << filename << "'" << std::endl;
                return Image();
            }
        }
        else
        {

            return Image();
        }
    }

    bool imwrite(const std::string &filename, const Image &img, const std::vector<int> &params)
    {
        if (img.empty())
        {
            return false;
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
                std::cerr << "Error writing image '" << filename << "': " << e.what() << std::endl;
                return false;
            }
            catch (...)
            {
                std::cerr << "Unknown error writing image '" << filename << "'" << std::endl;
                return false;
            }
        }
        else
        {

            return false;
        }
    }
}
