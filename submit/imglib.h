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
// 辅助宏，用于构建带函数名的错误信息
#define IMG_ERROR_PREFIX(function_name) (function_name + " - ")

#define IMG_8U 0
#define IMG_16U 1
#define IMG_32S 2
#define IMG_32F 3
#define IMG_64F 4

// 注意保证宏里头不要传入非法类型
#define IMG_MAKETYPE(depth, cn) ((depth) + (((cn) - 1) << 3)) // IMG_MAKETYPE只会在开头使用,就不修改成函数了
    // #define IMG_DEPTH(type) ((type) & 7)
    inline int IMG_DEPTH(int type)
    {
        return type >= 0 ? type & 7 : -1;
    }
    // #define IMG_CN(type) ((((type) >> 3) & 0x1F) + 1)
    inline int IMG_CN(int type)
    {
        return type >= 0 ? (((type >> 3) & 0x1F) + 1) : 0;
    }
#define IMG_8UC1 IMG_MAKETYPE(IMG_8U, 1) // 0000 0000
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

    //////////////////////////////////////////数据存储类//////////////////////////////////////////
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
        void create(size_t rows, size_t cols, int type); // 用于重置内存空间
        void release();                                  // 把图像置空,在必要时释放内存
        Image clone() const;                             // 深拷贝

        void show_info(std::ostream &os = std::cout) const; // 默认输出到 std::cout
        friend std::ostream &operator<<(std::ostream &os, const Image &img);

        ///////////获取图像信息函数/////////
        /** @brief 检查图像是否为空（未分配数据或维度为零）。*/
        bool empty() const { return m_datastorage == nullptr; };
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

        int get_channels() const { return IMG_CN(m_type); };
        /** @brief 返回图像的深度类型 (例如 IMG_8U, IMG_32F)。 */
        int get_depth() const { return IMG_DEPTH(m_type); };

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
        unsigned char *get_rowptr(int r);
        const unsigned char *get_rowptr(int r) const;
        /**
         * @brief 提供对指定位置像素的第一个通道的指针。
         * !!!!!这个函数不会检查图像类型是否与指定类型相匹配!!!!!
         * !!!!!用户需要负责选择正确的类型!!!!!
         * @param row 像素的行索引 (从0开始)。
         * @param col 像素的列索引 (从0开始)。
         * @return 指向位于 (row, col) 的像素第一个通道的指针。
         * @throw std::logic_error 如果图像为空。
         * @throw std::out_of_range 如果访问索引超出图像边界。
         */
        template <typename T>
        T *at(size_t row, size_t col)
        {
            if (this->empty())
            {
                throw std::logic_error("Image::at - 试图访问空图像的数据。");
            }
            if (row >= m_rows || col >= m_cols)
            {
                throw std::out_of_range("Image::at - 访问索引超出图像边界。");
            }

            return reinterpret_cast<T *>(m_data_start + row * m_step + col * get_pixel_size());
        };
        template <typename T>
        const T *at(size_t row, size_t col) const
        {
            if (this->empty())
            {
                throw std::logic_error("Image::at (const) - 试图访问空图像的数据。");
            }
            if (row >= m_rows || col >= m_cols)
            {
                throw std::out_of_range("Image::at (const) - 访问索引超出图像边界。");
            }

            return reinterpret_cast<const T *>(m_data_start + row * m_step + col * get_pixel_size());
        };
        
        ///////////运算符重载///////////
        Image &operator+=(double scalar);
        Image operator+(double scalar) const;
        friend Image operator+(double scalar, const Image &img);
        Image &operator-=(double scalar);
        Image operator-(double scalar) const;
        Image &operator*=(double scalar);
        Image operator*(double scalar) const;
        friend Image operator*(double scalar, const Image &img);
        Image &operator/=(double scalar);
        Image operator/(double scalar) const;
        // 这里要求operation_name是为了打印报错信息
        static void check_compatibility(const Image &img1, const Image &img2, const std::string &operation_name);
        Image &operator+=(const Image &other);
        Image &operator-=(const Image &other);

        Image operator+(const Image &other) const;
        Image operator-(const Image &other) const;
        ////////////选择兴趣区域/////////
        Image roi(size_t start_x, size_t start_y, size_t width, size_t height) const;

        ////////////转换图像类型/////////
        // 注意只支持相同通道数的转换
        // 例如 IMG_8UC3 转换为 IMG_32FC3 是可以的,但是 IMG_8UC1 转换为 IMG_32FC3 是不可以的
        Image convert_to(int new_type) const;

    private:
        void allocate(size_t rows, size_t cols, int type);
        size_t m_rows;         // 像素行数
        size_t m_cols;         // 像素列数
        int m_type;            // 图像类型 (深度和通道的组合)
        size_t m_channel_size; // 单个通道元素占用的字节数 (例如 IMG_8U 为 1, IMG_32F 为 4)
        size_t m_step;         // 每一行数据所占用的总字节数

        unsigned char *m_data_start;  // 指向图像在堆上有效像素数据的起始位置
        unsigned char *m_datastorage; // 指向实际在堆上分配的内存块的起始位置 (包含引用计数)
        int *m_refcount;              // 指向存储引用计数的位置
    };

    //////////////////////////////////////////IO处理器//////////////////////////////////////////
    //抽象类,所有图像类型的处理继承于此,抽象类的实现放在image_io.cpp中
    class ImageIOHandler
    {
    public:
        /** @brief 虚析构函数，确保派生类正确析构。 */
        virtual ~ImageIOHandler() = default;
        //如果析构函数不是虚的，只有基类的析构函数会被调用，派生类的析构函数不会被调用，这可能导致资源泄漏
        //基类本身没有数据,使用默认即可

        /**
         * @brief 从指定文件读取图像数据。
         * @param filename 要读取的文件路径。
         */
        virtual Image h_read(const std::string &filename) const = 0;
        // =0 代表该函数是一个纯虚函数
        //包含纯虚函数的类为抽象类,不能被实例化
        //如果派生类没有提供所有纯虚函数实现, 派生类也为抽象类

        /**
         * @brief 将 Image 对象写入指定文件。
         * @param filename 要写入的文件路径。
         * @param img 要保存的 Image 对象。
         */
        virtual bool h_write(const std::string &filename, const Image &img) const = 0;

        /**
         * @brief 获取此处理器支持的文件扩展名列表（不含点，小写）。
         * 工厂将使用此列表来确定何时使用这个处理器。
         * @return 一个包含支持的扩展名（例如 "png", "jpg", "jpeg", "bmp"）的字符串向量。
         */
        virtual std::vector<std::string> get_supported_extensions() const = 0;
        //返回字符串的列表
    };

    //图像IO的工厂,采用单例模式,用于创建和管理不同的ImageIOHandler对象
    class ImageIOFactory
    {
    public:
        /**
         * @brief 获取工厂实例的唯一访问点
         * @return 对 ImageIOFactory 单例的引用。
         */
        static ImageIOFactory &get_instance();

        //防止创建ImageIOFactory对象
        ImageIOFactory(const ImageIOFactory &) = delete;
        ImageIOFactory &operator=(const ImageIOFactory &) = delete;
        ImageIOFactory(ImageIOFactory &&) = delete;
        ImageIOFactory &operator=(ImageIOFactory &&) = delete;

        //unique_ptr 可以转移所有权,但是保证对于指向对象的唯一拥有(也就是不能复制)
        //之后实现特定的ImageIOHandler派生类的时候
        //使用make_unique 来创建unique_ptr,然后通过registerHandler的方式把这个handler对象的所有权完全转交给
        //工厂类来进行管理
        void register_handler(std::unique_ptr<ImageIOHandler> handler);
        ImageIOHandler *get_handler(const std::string &filename) const;

    private:
        ImageIOFactory() = default;
        ~ImageIOFactory() = default;

        //这是一个unique_ptr的可变长度数组,在getInstance方法中会把注册后的handler全部存入这个数组
        //在程序结束ImageIOFactory被销毁的时候,会依次调用每一个unique_ptr的析构函数来释放对应的handler的资源
        std::vector<std::unique_ptr<ImageIOHandler>> m_registered_handlers;
        //使用哈希表来优化getHandler的效率,现在是负优化,但是可能之后处理器会变多
        //在析构的时候只会释放指针的内存,而不会释放指针指向内容,指针是用于观察而不是拥有
        std::unordered_map<std::string, ImageIOHandler *> m_handlers_map;
        //用来匹配handler
        static std::string get_file_extension_lower(const std::string &filename);
    };
    //////////////顶层IO读写//////////////
    Image imread(const std::string &filename);
    //简单的状态码,库主要还是使用抛出错误
    bool imwrite(const std::string &filename, const Image &img);



    //////////////示例图像处理函数//////////////
 
    void adjust_brightness(Image &img, double value);
    void blend_images(const Image &img1, const Image &img2, Image &output, double alpha);

}