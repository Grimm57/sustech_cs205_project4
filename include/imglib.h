#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <stdexcept>
#include <cassert>
#include <algorithm>
#include <cstring>
#include <vector>
#include <memory>
#include <unordered_map>

namespace img
{

#define IMG_8U 0
#define IMG_16U 1
#define IMG_32S 2
#define IMG_32F 3
#define IMG_64F 4

#define IMG_MAKETYPE(depth, cn) ((depth) + (((cn) - 1) << 3))
#define IMG_DEPTH(type) ((type) & 7)
#define IMG_CN(type) ((((type) >> 3) & 0x1F) + 1)

#define IMG_8UC1 IMG_MAKETYPE(IMG_8U, 1)
#define IMG_8UC3 IMG_MAKETYPE(IMG_8U, 3)
#define IMG_8UC4 IMG_MAKETYPE(IMG_8U, 4)

#define IMG_16UC1 IMG_MAKETYPE(IMG_16U, 1)
#define IMG_16UC3 IMG_MAKETYPE(IMG_16U, 3)
#define IMG_16UC4 IMG_MAKETYPE(IMG_16U, 4)

#define IMG_32SC1 IMG_MAKETYPE(IMG_32S, 1)
#define IMG_32SC3 IMG_MAKETYPE(IMG_32S, 3)
#define IMG_32SC4 IMG_MAKETYPE(IMG_32S, 4)

#define IMG_32FC1 IMG_MAKETYPE(IMG_32F, 1)
#define IMG_32FC3 IMG_MAKETYPE(IMG_32F, 3)
#define IMG_32FC4 IMG_MAKETYPE(IMG_32F, 4)

#define IMG_64FC1 IMG_MAKETYPE(IMG_64F, 1)
#define IMG_64FC3 IMG_MAKETYPE(IMG_64F, 3)
#define IMG_64FC4 IMG_MAKETYPE(IMG_64F, 4)

    /**
     * @brief 表示一个二维图像的核心类，其设计受到了 `cv::Mat` 的启发。
     *
     * 本类负责管理图像的像素数据、维度（行、列）、数据类型，并支持创建
     * 共享数据的视图（类似ROI，但本实现中未显式提供ROI构造函数，
     * 但设计支持共享数据）。它使用手动的引用计数来管理内存。
     *
     * (核心设计：为底层数据块维护一个引用计数。多个 Image 对象可以指向
     * 同一数据块，只有当最后一个引用消失时才释放内存。)
     */
    class Image
    {
    public:
        /** @brief 默认构造函数。创建一个空的 Image 对象。 */
        Image();

        /**
         * @brief 根据指定的维度和类型构造图像。
         * 会为图像数据和引用计数分配新的内存。
         * @param rows 图像的行数。
         * @param cols 图像的列数。
         * @param type 图像类型 (例如 IMG_8UC3, IMG_32FC1)。使用 IMG_MAKETYPE 宏创建。
         * @throws std::bad_alloc 如果内存分配失败。
         * @throws std::runtime_error 如果类型无效或尺寸为0。
         */
        Image(size_t rows, size_t cols, int type);

        /**
         * @brief 拷贝构造函数。
         * 创建一个新的 Image 对象头，与源对象 `other` 共享底层像素数据和引用计数。
         * 这是一种浅拷贝（仅拷贝头部信息和指针），效率高。会增加引用计数。
         * @param other 源 Image 对象。
         */
        Image(const Image &other);

        /**
         * @brief 移动构造函数。
         * 从源对象 `other` 接管资源（数据指针、维度、引用计数指针等），并将 `other` 置为空状态。
         * 不涉及数据拷贝和引用计数更改，效率高。
         * @param other 源 Image 对象 (将被修改为空)。
         */
        Image(Image &&other);

        /**
         * @brief 析构函数。
         * 减少底层数据的引用计数。如果引用计数降为零，则释放内存。
         */
        ~Image();

        /**
         * @brief 拷贝赋值运算符。
         * 使当前对象共享 `other` 的数据。如果当前对象之前拥有数据，会先减少其引用计数（可能释放）。
         * 然后增加 `other` 指向数据的引用计数。
         * @param other 源 Image 对象。
         * @return 对当前对象的引用。
         */
        Image &operator=(const Image &other);

        /**
         * @brief 移动赋值运算符。
         * 从 `other` 接管资源，并将 `other` 置为空状态。如果当前对象之前拥有数据，会先释放。
         * @param other 源 Image 对象 (将被修改为空)。
         * @return 对当前对象的引用。
         */
        Image &operator=(Image &&other);

        /**
         * @brief 创建图像的深拷贝（克隆）。
         * 分配新的内存（包括新的引用计数），并将当前图像的像素数据完整复制过去。
         * 返回的新 Image 对象拥有独立的数据副本，其引用计数初始化为1。
         * @return 一个包含独立数据副本的新 Image 对象。
         * @throws std::bad_alloc 如果内存分配失败。
         * @throws std::runtime_error 如果源图像为空。
         */
        Image clone() const;

        /**
         * @brief (重新)分配图像数据存储空间。
         * 如果 `rows`, `cols`, `type` 与当前对象相同，则不执行任何操作。
         * 否则，先调用 `release()` 释放当前可能持有的资源，然后分配新空间。
         * @param rows 新的行数。
         * @param cols 新的列数。
         * @param type 新的图像类型。
         * @throws std::bad_alloc 如果内存分配失败。
         * @throws std::runtime_error 如果类型无效或尺寸为0。
         */
        void create(size_t rows, size_t cols, int type);

        /**
         * @brief 显式释放图像数据。
         * 减少共享数据的引用计数。如果引用计数降为零，底层内存将被释放。
         * 然后将当前 Image 对象重置为空状态（维度为0，指针为空）。
         */
        void release();

        /**
         * @brief 检查图像是否为空（未分配数据或维度为零）。
         * @return 如果图像数据指针为空或行/列数为0，则返回 `true`，否则返回 `false`。
         */
        bool empty() const { return m_data_start == nullptr || m_rows == 0 || m_cols == 0; };

        /** @brief 返回图像的行数。 */
        size_t get_rows() const { return m_rows; }

        /** @brief 返回图像的列数。 */
        size_t get_cols() const { return m_cols; }

        /** @brief 返回图像类型 (深度和通道的组合)。 */
        int get_type() const { return m_type; }

        /**
         * @brief 返回图像每一行数据所占用的总字节数。
         * 注意：步长可能大于 `cols() * get_pixel_size()`（虽然在此简化实现中通常相等）。
         */
        size_t get_step() const { return m_step; }

        /** @brief 返回单个通道元素占用的字节数 (例如 IMG_8U 为 1, IMG_32F 为 4)。 */
        size_t get_channel_size() const { return m_channel_size; };

        /**
         * @brief 检查两个图像是否兼容进行逐元素操作。
         * 兼容条件：两者都非空，具有相同的行数、列数和类型。
         * @param img1 第一个图像。
         * @param img2 第二个图像。
         * @throws std::runtime_error 如果图像不兼容。
         */
        static void check_compatibility(const Image &img1, const Image &img2);

        /** @brief 返回图像的通道数 (例如，灰度图为1，BGR图为3)。 */
        int get_channels() const { return (m_type == -1) ? 0 : IMG_CN(m_type); };

        /** @brief 返回图像的深度类型 (例如 IMG_8U, IMG_32F)。 */
        int get_depth() const { return (m_type == -1) ? -1 : IMG_DEPTH(m_type); };

        /** @brief 返回单个像素占用的字节数 (等于 get_channels() * get_channel_size())。 */
        size_t get_pixel_size() const { return m_channel_size * get_channels(); };

        /** @brief 返回图像的总像素数 (rows * cols)。 */
        size_t get_total() const { return m_rows * m_cols; }

        /**
         * @brief 返回指向图像数据起始位置的指针 (第一行第一个像素)。
         * @return 指向图像有效数据起始位置的 `unsigned char*` 指针。如果图像为空，则为 `nullptr`。
         */
        unsigned char *data() { return m_data_start; }

        /**
         * @brief 返回指向图像数据起始位置的常量指针。
         * @return 指向图像有效数据起始位置的 `const unsigned char*` 指针。如果图像为空，则为 `nullptr`。
         */
        const unsigned char *data() const { return m_data_start; }

        /**
         * @brief 返回指向指定行的起始位置的指针。
         * @param r 行索引 (从0开始)。
         * @return 指向第 `r` 行起始像素的 `unsigned char*` 指针。
         * @throws std::out_of_range 如果行索引 `r` 无效 (小于0或大于等于 `rows()`)。
         */
        unsigned char *ptr(int r);

        /**
         * @brief 返回指向指定行的起始位置的常量指针。
         * @param r 行索引 (从0开始)。
         * @return 指向第 `r` 行起始像素的 `const unsigned char*` 指针。
         * @throws std::out_of_range 如果行索引 `r` 无效 (小于0或大于等于 `rows()`)。
         */
        const unsigned char *ptr(int r) const;

        /**
         * @brief 提供对指定位置像素的直接访问（读写）。
         * 这是一个模板函数，需要显式指定图像类型 `T` (例如 `unsigned char` for IMG_8UC1, `float` for IMG_32FC1, `Vec3b` for IMG_8UC3 - 需要定义 Vec3b 等结构体)。
         * 建议在使用前检查图像类型是否匹配 `T`。
         * @param row 像素的行索引 (从0开始)。
         * @param col 像素的列索引 (从0开始)。
         * @return 对位于 (row, col) 的像素的引用。
         * @throws std::out_of_range 如果索引越界。
         * @throws std::logic_error 如果图像为空。
         */
        template <typename T>
        T &at(size_t row, size_t col)
        {
            if (this->empty())
            {
                throw std::logic_error("Image::at - 试图访问空图像的数据。");
            }
            if (row >= m_rows || col >= m_cols)
            {
                throw std::out_of_range("Image::at - 访问索引超出图像边界。");
            }

            return *reinterpret_cast<T *>(m_data_start + row * m_step + col * get_pixel_size());
        };

        /**
         * @brief 提供对指定位置像素的直接访问（只读）。
         * @param row 像素的行索引 (从0开始)。
         * @param col 像素的列索引 (从0开始)。
         * @return 对位于 (row, col) 的像素的常量引用。
         * @throws std::out_of_range 如果索引越界。
         * @throws std::logic_error 如果图像为空。
         */
        template <typename T>
        const T &at(size_t row, size_t col) const
        {
            if (this->empty())
            {
                throw std::logic_error("Image::at (const) - 试图访问空图像的数据。");
            }
            if (row >= m_rows || col >= m_cols)
            {
                throw std::out_of_range("Image::at (const) - 访问索引超出图像边界。");
            }

            return *reinterpret_cast<const T *>(m_data_start + row * m_step + col * get_pixel_size());
        };

        /** @brief 图像与标量的复合加法赋值 (逐像素)。 */
        Image &operator+=(double scalar);
        Image operator+(double scalar);
        friend Image operator+(double scalar, const Image &img);
        /** @brief 图像与标量的复合减法赋值 (逐像素)。 */
        Image &operator-=(double scalar);
        Image operator-(double scalar);
        /** @brief 图像与标量的复合乘法赋值 (逐像素)。 */
        Image &operator*=(double scalar);
        Image operator*(double scalar);
        friend Image operator*(double scalar, const Image &img);
        /** @brief 图像与标量的复合除法赋值 (逐像素)。 */
        Image &operator/=(double scalar);
        Image operator/(double scalar);

        /**
         * @brief 图像与图像的复合加法赋值 (逐元素)。
         * 要求两个图像具有相同的尺寸和类型。
         * @param other 要加到当前图像上的另一个图像。
         * @throws std::runtime_error 如果图像不兼容。
         */
        Image &operator+=(const Image &other);
        /**
         * @brief 图像与图像的复合减法赋值 (逐元素)。
         * 要求两个图像具有相同的尺寸和类型。
         * @param other 从当前图像减去的另一个图像。
         * @throws std::runtime_error 如果图像不兼容。
         */
        Image &operator-=(const Image &other);

        /** @brief 返回当前数据的引用计数（主要用于调试）。 */
        int get_refcount() const { return m_refcount ? *m_refcount : 0; }

        /**
         * @brief 创建一个表示当前图像感兴趣区域（ROI）的新 Image 对象（视图）。
         *
         * 这个新的 Image 对象与原始图像共享底层的像素数据和引用计数。
         * 对ROI视图的修改会直接反映在原始图像的对应区域。
         * ROI视图的 m_step 与原始图像的 m_step 相同。
         *
         * @param x ROI左上角在当前图像中的列索引（0-based）。
         * @param y ROI左上角在当前图像中的行索引（0-based）。
         * @param width ROI的宽度（列数）。
         * @param height ROI的高度（行数）。
         * @return 一个新的 Image 对象，它是原始图像指定区域的一个视图。
         * @throws std::out_of_range 如果ROI参数指定的区域超出了当前图像的边界，
         *                           或者 width/height 为0或导致访问非法内存。
         * @throws std::logic_error 如果当前图像为空 (this->empty())。
         */
        Image roi(size_t start_x, size_t start_y, size_t width, size_t height) const;

    private:
        /** @brief 内部辅助函数：分配内存块（包括引用计数区域） */
        void allocate(size_t rows, size_t cols, int type);

        size_t m_rows = 0;
        size_t m_cols = 0;
        char m_type = -1;
        unsigned char m_channel_size = 0;

        size_t m_step = 0;

        unsigned char *m_data_start = nullptr;
        unsigned char *m_datastorage = nullptr;

        int *m_refcount = nullptr;
    };

    /**
     * @brief 通过给每个像素通道增加一个固定值来调整图像亮度。
     * 此函数直接修改输入的图像 (`img`)。
     * 支持常见的图像类型。对于整数类型，结果会被截断到有效范围内。
     * @param img 要修改的图像 (输入/输出)。必须是非空图像。
     * @param value 要加到每个像素通道的值。
     * @throws std::runtime_error 如果图像为空或类型不支持。
     */
    void adjust_brightness(Image &img, double value);

    /**
     * @brief 使用加权平均 (alpha 混合) 将两张图像混合。
     * 公式: output = alpha * img1 + (1 - alpha) * img2
     * 要求 `img1` 和 `img2` 具有兼容的尺寸和类型。
     * 输出图像 `output` 会被自动创建或调整大小/类型以匹配输入。
     * @param img1 第一个输入图像 (const)。
     * @param img2 第二个输入图像 (const)。
     * @param output 输出图像 (结果将写入此处)。
     * @param alpha 第一个图像的权重，应在 [0.0, 1.0] 范围内。
     * @throws std::runtime_error 如果输入图像不兼容或类型不支持。
     */
    void blend_images(const Image &img1, const Image &img2, Image &output, double alpha);

    /**
     * @brief 旋转图像指定次数（每次90度顺时针）。
     * @param src 要旋转的源图像 (const)。
     * @param times 旋转次数 (0: 不旋转, 1: 90度, 2: 180度, 3: 270度)。次数会被模4处理。
     * @return 一个包含旋转后图像数据的新 Image 对象。
     * @throws std::runtime_error 如果源图像为空。
     * @throws std::bad_alloc 如果内存分配失败。
     */
    Image rotate_images(const Image &src, int times);

    /**
     * @brief 使用临近插值调整图像大小。
     * @param src 要调整大小的源图像 (const)。
     * @param new_rows 目标图像的行数。
     * @param new_cols 目标图像的列数。
     * @return 一个包含调整大小后图像数据的新 Image 对象。
     * @throws std::runtime_error 如果源图像为空或目标尺寸无效。
     * @throws std::bad_alloc 如果内存分配失败。
     */
    Image resize_images(const Image &src, size_t new_rows, size_t new_cols);

    /**
     * @brief 图像文件 I/O 处理器的抽象基类。
     *
     * 定义了所有特定格式图像处理器（如 PNG, JPEG, BMP）必须实现的通用接口。
     * 子类需要实现具体的读写逻辑和支持的扩展名列表。
     *
     * 设计思路：
     * 1. 定义通用读写操作。
     * 2. 定义获取支持扩展名的方法，供工厂识别。
     */
    class ImageIOHandler
    {
    public:
        /** @brief 虚析构函数，确保派生类正确析构。 */
        virtual ~ImageIOHandler() = default;

        /**
         * @brief 从指定文件读取图像数据。
         * @param filename 要读取的文件路径。
         * @return 包含图像数据的 Image 对象。如果读取失败（例如文件无效、格式不匹配），
         *         应返回一个空的 Image (img.empty() == true)。
         * @throws std::runtime_error 如果发生无法恢复的错误（例如无法打开文件）。
         */
        virtual Image read(const std::string &filename) const = 0;

        /**
         * @brief 将 Image 对象写入指定文件。
         * @param filename 要写入的文件路径。
         * @param img 要保存的 Image 对象。
         * @param params 特定于格式的编码参数 (可选)。例如 JPEG 质量。
         * @return 如果写入成功，返回 true；否则返回 false。
         * @throws std::runtime_error 如果发生无法恢复的错误或图像与格式要求不兼容。
         */
        virtual bool write(const std::string &filename, const Image &img, const std::vector<int> &params) const = 0;

        /**
         * @brief 获取此处理器支持的文件扩展名列表（不含点，小写）。
         * 工厂将使用此列表来确定何时使用这个处理器。
         * @return 一个包含支持的扩展名（例如 "png", "jpg", "jpeg", "bmp"）的字符串向量。
         */
        virtual std::vector<std::string> getSupportedExtensions() const = 0;
    };

    /**
     * @brief 图像 I/O 处理器工厂类 (通常实现为单例)。
     *
     * 负责注册、管理和分发不同图像格式的 ImageIOHandler。
     * 主要通过文件扩展名来查找合适的处理器。
     *
     * 设计思路：
     * 1. 提供注册处理器的机制。
     * 2. 提供根据文件名获取合适处理器的方法。
     * 3. 管理已注册处理器的生命周期 (使用智能指针)。
     * 4. 通常设计为单例，全局唯一访问点。
     */
    class ImageIOFactory
    {
    public:
        /**
         * @brief 获取工厂的唯一实例 (单例模式)。
         * @return 对 ImageIOFactory 单例的引用。
         */
        static ImageIOFactory &getInstance();

        ImageIOFactory(const ImageIOFactory &) = delete;
        ImageIOFactory &operator=(const ImageIOFactory &) = delete;
        ImageIOFactory(ImageIOFactory &&) = delete;
        ImageIOFactory &operator=(ImageIOFactory &&) = delete;

        /**
         * @brief 注册一个新的图像 I/O 处理器。
         * 工厂将取得 handler 的所有权。
         * 它会读取处理器支持的扩展名，并将其添加到内部映射中。
         * @param handler 指向要注册的处理器实例的 unique_ptr。该处理器必须实现 ImageIOHandler 接口。
         * @throws std::runtime_error 如果注册的扩展名与现有处理器冲突。
         */
        void registerHandler(std::unique_ptr<ImageIOHandler> handler);

        /**
         * @brief 根据文件名（主要是其扩展名）获取合适的处理器。
         * @param filename 文件名或文件路径。
         * @return 指向合适的 ImageIOHandler 的指针；如果找不到支持该文件扩展名的处理器，则返回 nullptr。
         *         注意：返回的指针由工厂拥有，调用者不应删除它。生命周期由工厂管理。
         */
        ImageIOHandler *getHandler(const std::string &filename) const;

    private:
        /** @brief 私有构造函数，用于单例模式。 */
        ImageIOFactory() = default;
        /** @brief 私有析构函数。unique_ptr 会自动管理内存。 */
        ~ImageIOFactory() = default;

        /** @brief 存储所有已注册的处理器，负责它们的所有权。 */
        std::vector<std::unique_ptr<ImageIOHandler>> m_registered_handlers;

        /** @brief 按小写文件扩展名映射到对应的处理器（原始指针，由 m_registered_handlers 拥有）。 */
        std::unordered_map<std::string, ImageIOHandler *> m_handlers_by_ext;

        /** @brief 内部辅助函数：从文件名提取小写扩展名。 */
        static std::string getFileExtensionLower(const std::string &filename);
    };

    /**
     * @brief 从文件加载图像 (使用工厂模式)。
     *
     * 该函数通过 ImageIOFactory 查找适合文件扩展名的处理器来加载图像。
     * 它封装了获取工厂实例、查找处理器、调用处理器 read 方法的逻辑。
     *
     * @param filename 要加载的图像文件的路径。
     * @return 加载成功时返回包含图像数据的 `Image` 对象；
     *         如果文件不存在、无法打开、找不到合适的处理器或加载失败，则返回一个空的 `Image` 对象 (`empty()` 为 true)。
     */
    Image imread(const std::string &filename);

    /**
     * @brief 将图像保存到文件 (使用工厂模式)。
     *
     * 该函数通过 ImageIOFactory 查找适合文件扩展名的处理器来保存图像。
     * 它封装了获取工厂实例、查找处理器、调用处理器 write 方法的逻辑。
     *
     * @param filename 要保存图像的文件路径。文件扩展名决定了使用的处理器和输出格式。
     * @param img 要保存的 `Image` 对象 (const 引用)。图像不应为空，且其类型应与目标格式兼容。
     * @param params 可选的、特定于格式的编码参数 (例如 {JPEG_QUALITY, 95})。
     * @return 如果保存成功，则返回 `true`；
     *         如果找不到合适的处理器、图像为空、与格式不兼容或写入文件失败，则返回 `false`。
     */
    bool imwrite(const std::string &filename, const Image &img, const std::vector<int> &params = std::vector<int>());
}
