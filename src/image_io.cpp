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
    // 内置对于BMP图像的支持
#pragma pack(push, 1) // 确保结构体成员紧密排列，没有编译器插入的填充字节
        typedef struct
        {
            uint16_t bfType;      // 文件类型标识，必须为 0x4D42 ('BM')
            uint32_t bfSize;      // BMP 文件总大小 (字节)
            uint16_t bfReserved1; // 保留，必须为 0
            uint16_t bfReserved2; // 保留，必须为 0
            uint32_t bfOffBits;   // 从文件头到实际像素数据的偏移量 (字节)
        } BMPFileHeader;

        typedef struct
        {
            uint32_t biSize;         // 此结构体的大小 (字节)，通常为 40
            int32_t biWidth;         // 图像宽度 (像素)
            int32_t biHeight;        // 图像高度 (像素)。正值表示从下到上存储，负值表示从上到下存储。
            uint16_t biPlanes;       // 目标设备的位平面数，必须为 1
            uint16_t biBitCount;     // 每个像素的位数 (1, 4, 8, 16, 24, 32)
            uint32_t biCompression;  // 压缩类型 (0: BI_RGB 不压缩, 1: BI_RLE8, 2: BI_RLE4, ...)
            uint32_t biSizeImage;    // 实际图像数据的大小 (字节)。对于未压缩图像，可以为 0。
            int32_t biXPelsPerMeter; // 水平分辨率 (像素/米)，通常不重要
            int32_t biYPelsPerMeter; // 垂直分辨率 (像素/米)，通常不重要
            uint32_t biClrUsed;      // 调色板中实际使用的颜色数。对于真彩色图像，为 0。
            uint32_t biClrImportant; // 调色板中重要的颜色数。为 0 表示所有颜色都重要。
        } BMPInfoHeader;
#pragma pack(pop) // 恢复之前的字节对齐设置

    ////////////// 工厂中支持的文件类型 //////////////
    class BMPHandler : public img::ImageIOHandler
    {        
    public:
        // 使用你指定的 h_read 和 h_write 方法名
        Image h_read(const std::string &filename) const override
        {
            std::ifstream file(filename, std::ios::binary);
            if (!file.is_open())
            {
                throw std::runtime_error("BMP读取错误: 无法打开文件 '" + filename + "'");
            }

            BMPFileHeader fileHeader = {}; // 初始化为0
            BMPInfoHeader infoHeader = {}; // 初始化为0

            // 读取文件头
            file.read(reinterpret_cast<char *>(&fileHeader), sizeof(BMPFileHeader));
            if (!file || file.gcount() != sizeof(BMPFileHeader))
            {
                throw std::runtime_error("BMP读取错误: 从文件 '" + filename + "' 读取文件头失败");
            }

            // 验证BMP文件标识 'BM' (0x4D42)
            if (fileHeader.bfType != 0x4D42)
            {
                throw std::runtime_error("BMP读取错误: 文件 '" + filename + "' 不是有效的BMP文件 (文件标识无效)");
            }

            // 读取信息头
            file.read(reinterpret_cast<char *>(&infoHeader), sizeof(BMPInfoHeader));
            if (!file || file.gcount() != sizeof(BMPInfoHeader))
            {
                throw std::runtime_error("BMP读取错误: 从文件 '" + filename + "' 读取信息头失败");
            }

            // 检查是否支持的BMP格式
            if (infoHeader.biBitCount != 24 && infoHeader.biBitCount != 32)
            {
                throw std::runtime_error("BMP读取错误: 文件 '" + filename + "' 包含不支持的BMP位深度。"
                                                                            " 仅支持24位和32位。检测到位深度: " +
                                         std::to_string(infoHeader.biBitCount) + "位");
            }
            if (infoHeader.biCompression != 0) // BI_RGB
            {
                throw std::runtime_error("BMP读取错误: 文件 '" + filename + "' 包含不支持的BMP压缩格式。"
                                                                            " 仅支持未压缩 (BI_RGB)。检测到压缩格式: " +
                                         std::to_string(infoHeader.biCompression));
            }

            // 验证图像尺寸
            if (infoHeader.biWidth <= 0 || infoHeader.biHeight == 0)
            {
                throw std::runtime_error("BMP读取错误: 文件 '" + filename + "' 包含无效的图像尺寸。"
                                                                            " 宽度: " +
                                         std::to_string(infoHeader.biWidth) + ", 高度: " + std::to_string(infoHeader.biHeight));
            }

            int width = infoHeader.biWidth;
            int actual_height = std::abs(infoHeader.biHeight); // 绝对高度
            bool topDown = (infoHeader.biHeight < 0);          // 判断图像是顶行优先还是底行优先
            int channels = infoHeader.biBitCount / 8;

            int imageType = IMG_MAKETYPE(IMG_8U, channels);
            try
            {
                Image img(actual_height, width, imageType); // 使用 (rows, cols, type) 构造

                // 计算每行的填充字节数，BMP行必须是4字节对齐
                int row_pitch_bytes = width * channels;
                int padding = (4 - (row_pitch_bytes % 4)) % 4;
                int stride = row_pitch_bytes + padding; // BMP文件中每行的实际字节数（包括填充）

                file.seekg(fileHeader.bfOffBits, std::ios::beg);
                if (!file)
                {
                    throw std::runtime_error("BMP读取错误: 在文件 '" + filename + "' 中定位像素数据失败 (偏移量: " + std::to_string(fileHeader.bfOffBits) + ")");
                }

                std::vector<unsigned char> rowBuffer(stride);
                for (int y_read_order = 0; y_read_order < actual_height; ++y_read_order)
                {
                    int destY = topDown ? y_read_order : (actual_height - 1 - y_read_order);
                    unsigned char *destRowPtr = img.get_rowptr(destY); // 假设 Image 类有 get_rowptr

                    file.read(reinterpret_cast<char *>(rowBuffer.data()), stride);
                    if (!file || file.gcount() != stride)
                    {
                        throw std::runtime_error("BMP读取错误: 从文件 '" + filename + "' 读取像素数据行 " + std::to_string(y_read_order) + " 失败");
                    }

                    // 将读取的行数据（不包括填充字节）复制到 Image 对象中
                    std::memcpy(destRowPtr, rowBuffer.data(), row_pitch_bytes);
                }
                // file.close() 会在 std::ifstream 析构时自动调用

                return img;
            }
            catch (const std::exception &e)
            {
                std::cerr << e.what() << ("BMP读取错误: 为文件 '" + filename + "' 创建内部Image对象失败") << '\n';
                return Image();//返回空图像
            }
        }

        bool h_write(const std::string &filename, const Image &img) const override
        {
            if (img.empty())
            {
                throw std::invalid_argument("BMP写入错误: 输入的Image对象为空。");
            }

            if (img.get_type() != IMG_MAKETYPE(IMG_8U, 3) && img.get_type() != IMG_MAKETYPE(IMG_8U, 4))
            {
                throw std::runtime_error("BMP写入错误: 不支持的Image类型。仅支持8位3通道或8位4通道的图像。");
            }

            std::ofstream file(filename, std::ios::binary);
            if (!file.is_open())
            {
                throw std::runtime_error("BMP写入错误: 无法打开文件 '" + filename + "' 进行写入。");
            }

            int width = img.get_cols();
            int height = img.get_rows();
            int channels = img.get_channels();
            uint16_t bitCount = static_cast<uint16_t>(channels * 8);

            int row_pitch_bytes = width * channels;
            int padding = (4 - (row_pitch_bytes % 4)) % 4;
            int stride = row_pitch_bytes + padding;
            uint32_t imageSize = static_cast<uint32_t>(stride) * height;

            BMPFileHeader fileHeader = {};
            fileHeader.bfType = 0x4D42; // 'BM'
            fileHeader.bfSize = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + imageSize;
            fileHeader.bfReserved1 = 0;
            fileHeader.bfReserved2 = 0;
            fileHeader.bfOffBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);

            BMPInfoHeader infoHeader = {};
            infoHeader.biSize = sizeof(BMPInfoHeader);
            infoHeader.biWidth = width;
            // 标准BMP通常是底行优先 (bottom-up DIB)，所以 biHeight 为正值
            infoHeader.biHeight = height;
            infoHeader.biPlanes = 1;
            infoHeader.biBitCount = bitCount;
            infoHeader.biCompression = 0;       
            infoHeader.biSizeImage = imageSize; // 对于未压缩图像，可以设为实际大小
            // 根据需要设置分辨率，这里设为0表示未指定，或者可以设为常用值如 2835 (72 DPI)
            infoHeader.biXPelsPerMeter = 0;
            infoHeader.biYPelsPerMeter = 0;
            infoHeader.biClrUsed = 0;      
            infoHeader.biClrImportant = 0; // 0表示所有颜色都重要

            file.write(reinterpret_cast<const char *>(&fileHeader), sizeof(BMPFileHeader));
            file.write(reinterpret_cast<const char *>(&infoHeader), sizeof(BMPInfoHeader));

            if (!file) // 检查写入头是否成功
            {
                throw std::runtime_error("BMP写入错误: 向文件 '" + filename + "' 写入文件头或信息头失败。");
            }

            std::vector<unsigned char> rowBuffer(stride, 0); // 初始化填充字节为0

            // BMP 文件通常是底行优先存储，所以我们从 Image 对象的最后一行开始写到文件的第一行像素数据
            for (int y_img = height - 1; y_img >= 0; --y_img)
            {
                const unsigned char *srcRowPtr = img.get_rowptr(y_img); // 获取图像的当前行

                // 将图像数据复制到行缓冲区
                std::memcpy(rowBuffer.data(), srcRowPtr, row_pitch_bytes);
                // 填充字节已在 rowBuffer 初始化时设为 0

                file.write(reinterpret_cast<const char *>(rowBuffer.data()), stride);
                if (!file) // 检查每行写入是否成功
                {
                    throw std::runtime_error("BMP写入错误: 向文件 '" + filename + "' 写入像素数据行失败。");
                }
            }

            file.flush();
            if (!file.good())
            {
                throw std::runtime_error("BMP写入错误: 文件流在写入文件 '" + filename + "' 完成后状态不佳。");
            }
            return true;
        }

        std::vector<std::string> get_supported_extensions() const override
        {
            return {"bmp"};
        }
    };

    ////////////// 图像 I/O 处理器工厂 //////////////
    std::string ImageIOFactory::get_file_extension_lower(const std::string &filename)
    {
        if (filename.empty())
        {
            return ""; // 空文件名没有扩展名
        }

        // 从后向前查找最后一个 '.'
        int dot_position = -1; // 初始化为未找到
        for (int i = filename.length() - 1; i >= 0; --i)
        {
            if (filename[i] == '.')
            {
                dot_position = i;
                break;
            }
        }
        if (dot_position == -1 || dot_position == 0 || dot_position == (int)filename.length() - 1)
        {
            return ""; // 无效的扩展名格式
        }

        // 提取点后面的子字符串作为扩展名
        std::string extension = "";
        for (size_t i = dot_position + 1; i < filename.length(); ++i)
        {
            extension += filename[i];
        }

        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        return extension;
    }

    void ImageIOFactory::register_handler(std::unique_ptr<ImageIOHandler> handler)
    {
        if (!handler)
        {
            std::cerr << "ImageIOFactory::register_handler 警告: 尝试注册空的处理器指针。" << std::endl;
            return;
        }
        std::vector<std::string> extensions = handler->get_supported_extensions();
        if (extensions.empty())
        {
            std::cerr << "ImageIOFactory::register_handler 警告: 尝试注册一个不支持任何文件扩展名的处理器。" << std::endl;
            return;
        }
        // 获取裸指针以存入映射，所有权仍在 unique_ptr 中
        ImageIOHandler *raw_handler_ptr = handler.get(); // 智能指针的成员函数,返回其指向的对象的原始指针

        for (const std::string &ext : extensions)
        {
            if (m_handlers_map.count(ext)) // 如果返回非0
            {
                // 这是一个严重配置错误，应该抛出异常
                throw std::runtime_error("图像库配置错误: 文件扩展名 '" + ext + "' 存在重复的处理器注册。");
            }
            m_handlers_map[ext] = raw_handler_ptr;
        }

        // 将处理器的所有权转移给工厂
        // 在工厂的m_registered_handlers向量末尾添加
        // handler是一个unique ptr
        // m_registered_handlers.push_back(handler);编译失败,因为不能创建副本
        // 调用move,是为了把现在这个unique ptr的所有权move到工厂的智能指针向量里头
        // 被move标记后编译器才明白是进行转移而不是拷贝
        m_registered_handlers.push_back(std::move(handler));
    }

    ImageIOHandler *ImageIOFactory::get_handler(const std::string &filename) const
    {
        std::string ext = get_file_extension_lower(filename);
        if (ext.empty())
        {
            return nullptr; // 无法确定处理器,在上层抛出错误
        }

        auto it = m_handlers_map.find(ext);
        if (it != m_handlers_map.end())
        {
            return it->second; // 找到处理器
        }
        else
        {
            return nullptr; // 未找到支持该扩展名的处理器
        }
    }

    ImageIOFactory &ImageIOFactory::get_instance()
    {
        static ImageIOFactory instance;
        static bool handlers_registered = false;

        if (!handlers_registered)
        {
            try
            {
                // 移交单一对象的所有权
                // std::make_unique<BMPHandler>()调用无参构造器返回了指向handler对象的一个unique ptr
                instance.register_handler(std::make_unique<BMPHandler>());
                handlers_registered = true;
            }
            catch (const std::exception &e)
            {
                // 错误比较内部，主要给开发者看
                std::cerr << "图像库初始化错误: 注册图像处理器时出错: " << e.what() << std::endl;
                handlers_registered = true; // 不要重复错误注册
                throw std::runtime_error("图像库初始化失败: 无法注册必要的图像处理器.");
            }
        }

        return instance;
    }

    ////////////// 顶层图像 I/O 函数 //////////////
    Image imread(const std::string &filename)
    {
        ImageIOFactory &factory = ImageIOFactory::get_instance();
        ImageIOHandler *handler = factory.get_handler(filename);

        if (handler)
        {
            try
            {
                return handler->h_read(filename);
            }
            catch (const std::exception &e)
            {
                std::cerr << "读取图像错误: 处理文件 '" << filename << "' 时发生异常: " << e.what() << std::endl;
                return Image(); // 返回空图像表示失败
            }
        }
        else
        {
            std::cerr << "读取图像错误: 无法找到支持文件 '" << filename << "' 格式的处理器。" << std::endl;
            return Image(); // 返回空图像表示失败
        }
    }

    bool imwrite(const std::string &filename, const Image &img)
    {
        if (img.empty())
        {
            std::cerr << "写入图像错误: 尝试写入的图像数据为空。" << std::endl;
            return false;
        }

        ImageIOFactory &factory = ImageIOFactory::get_instance();
        ImageIOHandler *handler = factory.get_handler(filename);

        if (handler)
        {
            try
            {
                return handler->h_write(filename, img);
            }
            catch (const std::exception &e)
            {
                std::cerr << "写入图像错误: 处理文件 '" << filename << "' 时发生异常: " << e.what() << std::endl;
                return false; // 写入失败
            }
        }
        else
        {
            std::cerr << "写入图像错误: 无法找到支持文件 '" << filename << "' 格式的处理器。" << std::endl;
            return false; // 写入失败
        }
    }
} // namespace img