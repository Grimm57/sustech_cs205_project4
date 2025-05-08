#include <iostream>
#include <string>
#include "../include/imglib.h"

void test_image_constructors();
void test_image_error_handling();
void test_image_operator_overloads();

void test_io_bmp_read();
void test_io_bmp_write();

void test_processing_adjust_brightness();
void test_processing_blend_images();
void test_processing_crop_image();
void test_processing_rotate_image();

void display_menu();

int main()
{
    while (true)
    {
        display_menu();
        int choice;
        std::cin >> choice;

        if (choice == 0)
        {
            std::cout << "退出程序。" << std::endl;
            break;
        }

        switch (choice)
        {
        case 1:
        {
            std::cout << "选择了 Image 类测试。" << std::endl;
            std::cout << "1. 构造函数测试\n2. 错误处理测试\n3. 操作符重载测试\n请选择: ";
            int sub_choice;
            std::cin >> sub_choice;
            switch (sub_choice)
            {
            case 1:
                test_image_constructors();
                break;
            case 2:
                test_image_error_handling();
                break;
            case 3:
                test_image_operator_overloads();
                break;
            default:
                std::cout << "无效的选择。" << std::endl;
            }
            break;
        }
        case 2:
        {
            std::cout << "选择了 IO 测试。" << std::endl;
            std::cout << "1. BMP 图像读取测试\n2. BMP 图像写入测试\n请选择: ";
            int sub_choice;
            std::cin >> sub_choice;
            switch (sub_choice)
            {
            case 1:
                test_io_bmp_read();
                break;
            case 2:
                test_io_bmp_write();
                break;
            default:
                std::cout << "无效的选择。" << std::endl;
            }
            break;
        }
        case 3:
        {
            std::cout << "选择了图像处理测试。" << std::endl;
            std::cout << "1. 亮度调整测试\n2. 融合图像测试\n3. 裁剪图像测试\n4. 旋转图像测试\n请选择: ";
            int sub_choice;
            std::cin >> sub_choice;
            switch (sub_choice)
            {
            case 1:
                test_processing_adjust_brightness();
                break;
            case 2:
                test_processing_blend_images();
                break;
            case 3:
                test_processing_crop_image();
                break;
            case 4:
                test_processing_rotate_image();
                break;
            default:
                std::cout << "无效的选择。" << std::endl;
            }
            break;
        }
        default:
            std::cout << "无效的选择。" << std::endl;
        }
    }
    return 0;
}

void display_menu()
{
    std::cout << "\n========== 测试菜单 ==========\n";
    std::cout << "1. Image 类测试\n";
    std::cout << "2. IO 测试\n";
    std::cout << "3. 图像处理测试\n";
    std::cout << "0. 退出\n";
    std::cout << "请输入选择: ";
}

void test_image_constructors()
{
    std::cout << "开始测试 Image 类的构造函数..." << std::endl;

    // 测试默认构造函数
    img::Image img1;
    std::cout << "默认构造函数创建的图像信息：" << img1 << std::endl;

    // 测试带参数的构造函数
    img::Image img2(100, 200, IMG_8UC3);
    std::cout << "带参数的构造函数创建的图像信息：" << img2 << std::endl;

    // 测试软拷贝构造函数
    img::Image img3(img2);
    std::cout << "软拷贝构造函数创建的图像信息：" << img3 << std::endl;

    // 测试移动构造函数
    img::Image img4(std::move(img3));
    std::cout << "移动构造函数创建的图像信息：" << img4 << std::endl;
    std::cout << "被移动的图像 img3 的信息：" << img3 << std::endl;

    // 测试深拷贝
    img::Image img5 = img2.clone();
    std::cout << "深拷贝创建的图像信息：" << img5 << std::endl;

    // 测试不同类型的构造函数
    img::Image img6(50, 50, IMG_16UC1);
    std::cout << "构造 16 位单通道图像的信息：" << img6 << std::endl;

    img::Image img7(50, 50, IMG_32FC3);
    std::cout << "构造 32 位浮点三通道图像的信息：" << img7 << std::endl;

    std::cout << "Image 类构造函数测试完成。" << std::endl;
}

void test_image_error_handling()
{
    std::cout << "开始测试 Image 类的错误处理..." << std::endl;
    img::Image img1(-100, 200, IMG_8UC3);
    
    std::cout << "Image 类错误处理测试完成。" << std::endl;
}

void test_image_operator_overloads()
{
    std::cout << "开始测试 Image 类的操作符重载..." << std::endl;
}

void test_io_bmp_read()
{
    std::cout << "开始测试 BMP 图像读取..." << std::endl;
}

void test_io_bmp_write()
{
    std::cout << "开始测试 BMP 图像写入..." << std::endl;
}

void test_processing_adjust_brightness()
{
    std::cout << "开始测试亮度调整..." << std::endl;
}

void test_processing_blend_images()
{
    std::cout << "开始测试图像融合..." << std::endl;
}

void test_processing_crop_image()
{
    std::cout << "开始测试图像裁剪..." << std::endl;
}

void test_processing_rotate_image()
{
    std::cout << "开始测试图像旋转..." << std::endl;
}