#include <iostream>
#include "../include/imglib.h" // 包含你的图像库头文件

int main(int argc, char *argv[]) {
    // 在这里编写你的演示代码，使用 imglib 库的功能
    std::cout << "Image Library Demo" << std::endl;

    img::Image my_image = img::imread("example.bmp"); // 读取图像
    printf("引用计数: %d\n", my_image.get_refcount());
    if (my_image.empty()) {
        std::cerr << "错误：无法加载图像 'example.bmp' 或图像为空。" << std::endl;
        return 1; // 返回非零值表示错误
    }
    img::Image image_1 = my_image.clone(); // 克隆图像
    img::Image image_2 = my_image; // 浅拷贝
    image_2.at<unsigned char>(0, 0) = 0; // 修改像素值
    printf("引用计数: %d\n", my_image.get_refcount());
    img::adjust_brightness(my_image,-100); // 调整亮度
    img::Image resized_image = img::resize_images(my_image, 200, 200); // 调整大小
    img::imwrite("output.bmp", my_image);
    img::imwrite("resized_output.bmp", resized_image);
    // img::Image[] images = {my_image, resized_image}; // 创建图像数组
    img::Image image_3;
    printf("%d",image_3.get_refcount()); // 获取图像
    return 0; // main 函数必须返回 int
}