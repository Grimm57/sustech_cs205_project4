#include <iostream>
#include "../include/imglib.h" // 包含你的图像库头文件

int main(int argc, char *argv[]) {
    // 在这里编写你的演示代码，使用 imglib 库的功能
    std::cout << "Image Library Demo" << std::endl;

    img::Image my_image = img::imread("example.bmp");
    img::adjust_brightness(my_image,100); // 调整亮度
    img::Image resized_image = img::resize_images(my_image, 200, 200); // 调整大小
    img::imwrite("output.bmp", my_image);
    img::imwrite("resized_output.bmp", resized_image);

    return 0; // main 函数必须返回 int
}