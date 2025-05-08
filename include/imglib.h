#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <iostream>

namespace img
{
#define IMG_8U 0
#define IMG_16U 1
#define IMG_32S 2
#define IMG_32F 3
#define IMG_64F 4

// 保证宏里头不会传入负数
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
     * @brief 表示图像的核心类
     */
    class Image
    {
    public:
        /////////构造函数系列/////////
        /// 构造函数///
        Image();
        Image(size_t rows, size_t cols, int type);
        Image(const Image &other); // 软拷贝
        Image(Image &&other);      // 移动构造函数

        /////////析构函数/////////
        ~Image();

        /////////赋值运算符/////////
        Image &operator=(const Image &other); // 软拷贝
        Image &operator=(Image &&other);      // 移动赋值

        ////////////辅助函数/////////
        void create(size_t rows, size_t cols, int type);    // 用于重置内存空间
        void release();                                     // 把图像置空,在必要时释放内存
        Image clone() const;                                // 深拷贝

        void show_info(std::ostream &os = std::cout) const; // 默认输出到 std::cout
        friend std::ostream &operator<< (std::ostream &os, const Image &img);

        ///////////获取图像信息函数/////////
        /** @brief 检查图像是否为空（未分配数据或维度为零）。*/
        bool empty() const { return m_data_start == nullptr || m_rows == 0 || m_cols == 0; };
        /** @brief 返回图像的行数。 */
        size_t get_rows() const { return m_rows; }
        /** @brief 返回图像的列数。 */
        size_t get_cols() const { return m_cols; }
        /** @brief 返回图像类型 (深度和通道的组合)。 */
        int get_type() const { return m_type; }
        /** @brief 返回图像每一行数据所占用的总字节数。*/
        size_t get_step() const { return m_step; }
        /** @brief 返回单个通道元素占用的字节数 (例如 IMG_8U 为 1, IMG_32F 为 4)。 */
        size_t get_channel_size() const { return m_channel_size; };
        /** @brief 返回图像的通道数 (例如，灰度图为1，BGR图为3)。 */
        int get_channels() const { return (m_type == -1) ? 0 : IMG_CN(m_type); };
        /** @brief 返回图像的深度类型 (例如 IMG_8U, IMG_32F)。 */
        int get_depth() const { return (m_type == -1) ? -1 : IMG_DEPTH(m_type); };
        /** @brief 返回单个像素占用的字节数 (等于 get_channels() * get_channel_size())。 */
        size_t get_pixel_size() const { return m_channel_size * get_channels(); };
        /** @brief 返回图像的总像素数 (rows * cols)。 */
        size_t get_total() const { return m_rows * m_cols; }
        /** @brief 返回当前数据的引用计数（主要用于调试）。 */
        int get_refcount() const { return m_refcount ? *m_refcount : 0; }
        /**
         * @brief 返回指向图像数据起始位置的指针 (注意是指向第一行第一个像素,而不是ref_count)。
         * @return 指向图像有效数据起始位置的 `unsigned char*` 指针。如果图像为空，则为 `nullptr`。
         */
        unsigned char *data() { return m_data_start; }
        const unsigned char *data() const { return m_data_start; }
        /**
         * @brief 返回指向指定行的指针。
         * @param r 行索引 (从0开始)。
         * @return 指向第 `r` 行的 `unsigned char*` 指针。如果图像为空，则为 `nullptr`。
         */
        unsigned char *ptr(int r);
        const unsigned char *ptr(int r) const;
        /**
         * @brief 提供对指定位置像素的第一个通道的指针。
         * !!!!!这个函数不会检查图像类型是否与指定类型相匹配!!!!!
         * !!!!!用户需要负责选择正确的类型!!!!!
         * @param row 像素的行索引 (从0开始)。
         * @param col 像素的列索引 (从0开始)。
         * @return 对位于 (row, col) 的像素的引用。
         * @throw std::logic_error 如果图像为空。
         * @throw std::out_of_range 如果访问索引超出图像边界。
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

        ///////////运算符重载///////////
        Image &operator+=(double scalar);
        Image operator+(double scalar);
        friend Image operator+(double scalar, const Image &img);
        Image &operator-=(double scalar);
        Image operator-(double scalar);
        Image &operator*=(double scalar);
        Image operator*(double scalar);
        friend Image operator*(double scalar, const Image &img);
        Image &operator/=(double scalar);
        Image operator/(double scalar);

        static void check_compatibility(const Image &img1, const Image &img2);
        Image &operator+=(const Image &other);
        Image &operator-=(const Image &other);
        ////////////选择兴趣区域/////////
        Image roi(size_t start_x, size_t start_y, size_t width, size_t height) const;

        ////////////转换图像类型,并且进行简单的线性变化/////////
        Image convert_to(int new_type, double scale = 1.0, double shift = 0.0) const;

    private:
        void allocate(size_t rows, size_t cols, int type);
        size_t m_rows;         // 像素行数
        size_t m_cols;         // 像素列数
        int m_type;            // 图像类型 (深度和通道的组合)
        size_t m_channel_size; // 单个通道元素占用的字节数 (例如 IMG_8U 为 1, IMG_32F 为 4)
        size_t m_step;         // 每一行数据所占用的总字节数

        unsigned char *m_data_start;  // 指向图像有效数据的起始位置
        unsigned char *m_datastorage; // 指向实际分配的内存块的起始位置 (包含引用计数)
        int *m_refcount;              // 指向存储引用计数的位置
    };

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
         * @throws std::runtime_error 如果发生无法恢复的错误（例如无法打开文件）。 // 已存在，保持
         * @throw std::runtime_error 如果文件无法打开，或读取过程中发生严重I/O错误。
         * @throw std::bad_alloc 如果为图像数据分配内存失败。
         * @throw std::runtime_error 如果文件格式无效或已损坏，无法解析。
         */
        virtual Image read(const std::string &filename) const = 0;

        /**
         * @brief 将 Image 对象写入指定文件。
         * @param filename 要写入的文件路径。
         * @param img 要保存的 Image 对象。
         * @param params 特定于格式的编码参数 (可选)。例如 JPEG 质量。
         * @return 如果写入成功，返回 true；否则返回 false。
         * @throws std::runtime_error 如果发生无法恢复的错误或图像与格式要求不兼容。 // 已存在，保持
         * @throw std::logic_error 如果输入图像为空。
         * @throw std::runtime_error 如果文件无法创建/写入，或写入过程中发生严重I/O错误。
         * @throw std::invalid_argument 如果图像类型与目标文件格式不兼容，或编码参数无效。
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
         * @throws std::runtime_error 如果注册的扩展名与现有处理器冲突。 // 已存在，保持
         * @throw std::invalid_argument 如果 handler 为 nullptr。
         * @throw std::runtime_error 如果处理器未报告任何支持的扩展名。
         */
        void registerHandler(std::unique_ptr<ImageIOHandler> handler);

        /**
         * @brief 根据文件名（主要是其扩展名）获取合适的处理器。
         * @param filename 文件名或文件路径。
         * @return 指向合适的 ImageIOHandler 的指针；如果找不到支持该文件扩展名的处理器，则返回 nullptr。
         *         注意：返回的指针由工厂拥有，调用者不应删除它。生命周期由工厂管理。
         * @throw std::invalid_argument 如果文件名为空或无法提取有效扩展名 (可选行为, 也可仅返回nullptr)。
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

        /**
         * @brief 内部辅助函数：从文件名提取小写扩展名。
         * @throw std::invalid_argument 如果文件名过短或不含扩展名分隔符 (可选行为)。
         */
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
     * @throw std::runtime_error 如果在获取处理器或读取图像过程中发生严重且不可恢复的错误 (例如，I/O处理器内部的严重错误，或内存分配失败)。
     * @throw std::bad_alloc 如果图像数据内存分配失败 (通常由底层的 ImageIOHandler::read 抛出并可能被此函数重新抛出)。
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
     * @throw std::logic_error 如果输入图像为空。
     * @throw std::runtime_error 如果在获取处理器或写入图像过程中发生严重且不可恢复的错误 (例如，I/O处理器内部的严重错误)。
     * @throw std::invalid_argument 如果图像类型与目标文件格式不兼容或编码参数无效 (通常由底层的 ImageIOHandler::write 抛出并可能被此函数重新抛出)。
     */
    bool imwrite(const std::string &filename, const Image &img, const std::vector<int> params = std::vector<int>());

    /**
     * @brief 通过给每个像素通道增加一个固定值来调整图像亮度。
     * 此函数直接修改输入的图像 (`img`)。
     * 支持常见的图像类型。对于整数类型，结果会被截断到有效范围内。
     * @param img 要修改的图像 (输入/输出)。必须是非空图像。
     * @param value 要加到每个像素通道的值。
     * @throws std::runtime_error 如果图像为空或类型不支持。 // 已存在，保持
     * @throw std::logic_error 如果图像为空。
     * @throw std::runtime_error 如果图像类型不支持亮度调整操作。
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
     * @throws std::runtime_error 如果输入图像不兼容或类型不支持。 // 已存在，保持
     * @throw std::logic_error 如果任一输入图像为空。
     * @throw std::runtime_error 如果输入图像的尺寸或类型不兼容。
     * @throw std::invalid_argument 如果 alpha 值不在 [0.0, 1.0] 范围内。
     * @throw std::bad_alloc 如果为输出图像分配内存失败。
     * @throw std::runtime_error 如果图像类型不支持混合操作。
     */
    void blend_images(const Image &img1, const Image &img2, Image &output, double alpha);

    /**
     * @brief 旋转图像指定次数（每次90度顺时针）。
     * @param src 要旋转的源图像 (const)。
     * @param times 旋转次数 (0: 不旋转, 1: 90度, 2: 180度, 3: 270度)。次数会被模4处理。
     * @return 一个包含旋转后图像数据的新 Image 对象。
     * @throws std::runtime_error 如果源图像为空。 // 已存在，保持
     * @throws std::bad_alloc 如果内存分配失败。 // 已存在，保持
     * @throw std::logic_error 如果源图像为空。
     */
    Image rotate_images(const Image &src, int times);

    /**
     * @brief 使用临近插值调整图像大小。
     * @param src 要调整大小的源图像 (const)。
     * @param new_rows 目标图像的行数。
     * @param new_cols 目标图像的列数。
     * @return 一个包含调整大小后图像数据的新 Image 对象。
     * @throws std::runtime_error 如果源图像为空或目标尺寸无效。 // 已存在，保持
     * @throws std::bad_alloc 如果内存分配失败。 // 已存在，保持
     * @throw std::logic_error 如果源图像为空。
     * @throw std::invalid_argument 如果目标行数或列数为零。
     */
    Image resize_images(const Image &src, size_t new_rows, size_t new_cols);

}