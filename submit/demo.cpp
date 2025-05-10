#include <iostream>
#include <string>
#include "imglib.h"

void display_menu();
void image_test();
void bad_test();

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
            std::cout << "选择了 内部处理测试 测试。" << std::endl;
            image_test();
            break;
        }
        case 2:
        {
            std::cout << "选择了 异常 测试。" << std::endl;
            std::cout << "请输入读入图像路径" << std::endl;
            bad_test();
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
    std::cout << "1. 内部处理测试\n";
    std::cout << "2. 异常 测试\n";
    std::cout << "0. 退出\n";
    std::cout << "请输入选择: ";
}

void image_test()
{
    std::cout << "开始内部处理测试..." << std::endl;
    int rows = 100;
    int cols = 100;
    img::Image img(rows, cols, IMG_8UC3); // 创建一个100x100的图像

    // 创建渐变色图像
    for (int y = 0; y < img.get_rows(); ++y)
    {
        for (int x = 0; x < img.get_cols(); ++x)
        {
            unsigned char *pixel = img.at<unsigned char>(y, x);
            // 数据在内存中按BGR排列
            pixel[0] = 0; // B - 蓝色通道
            pixel[1] = static_cast<unsigned char>((static_cast<float>(y) / img.get_rows()) * 255); // G - 绿色通道随y变化
            pixel[2] = static_cast<unsigned char>((static_cast<float>(x) / img.get_cols()) * 255); // R - 红色通道随x变化
        }
    }
    img.show_info(); // 显示图像信息
    img::Image img1 = img.convert_to(IMG_32FC3); // 注意这里调用的是移动构造函数而不是拷贝构造函数,所以引用数不变
    std::cout << "(移动构造引用数不变)当前img1的引用次数"<<img1.get_refcount() << std::endl;
    img::Image img2 = img1; // 引用数增加
    std::cout << "(赋值构造)当前img1的引用次数"<<img1.get_refcount() << std::endl;
    img::Image img3(img1); // 引用数增加
    std::cout << "(拷贝构造)当前img1的引用次数"<<img1.get_refcount() << std::endl;

    std::cout << "8U类型初始值" << static_cast<int>(*img.at<unsigned char>(0, 0)) << std::endl;
    std::cout << "32F类型初始值" << (*img1.at<float>(0, 0)) << std::endl;
    img+= 350; 
    img1+=350;
    std::cout << "8U类型截断" <<static_cast<int>(*img.at<unsigned char>(0, 0)) << std::endl;
    std::cout << "32F类型不截断" <<*img1.at<float>(0, 0) << std::endl;
    img1/=2;
    try
    {
        std::cout<< "测试除零报错"<<std::endl;
        img1/=0;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    

    img1+=20;
    //得到了一个我感觉还挺好看的彩虹颜色图片
    img = img1.convert_to(IMG_8UC3); //赋值运算符是软拷贝
    try
    {
        img::imwrite("gradient.bmp", img); // 保存图像
        std::cout << "图像已保存为 gradient.bmp" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }

    std::cout << "内部处理测试完成。" << std::endl;
}

// ...existing code...
void bad_test()
{
    std::cout << "开始异常测试..." << std::endl;
    std::string non_existent_file = "non_existent_image.bmp";
    std::string output_bad_file = "output_bad.bmp";

    // 1. 尝试使用无效类型创建图像
    std::cout << "\n--- 测试1: 使用无效类型创建图像 ---" << std::endl;
    try
    {
        img::Image img_invalid_type(10, 10, -5); // 无效类型
    }
    catch (const std::exception &e)
    {
        std::cerr << "捕获到异常: " << e.what() << '\n';
    }

    // 2. 尝试使用零尺寸创建图像
    std::cout << "\n--- 测试2: 使用零尺寸创建图像 ---" << std::endl;
    try
    {
        img::Image img_zero_size(0, 10, IMG_8UC1);
    }
    catch (const std::exception &e)
    {
        std::cerr << "捕获到异常: " << e.what() << '\n';
    }

    // 3. 访问空图像的像素数据
    std::cout << "\n--- 测试3: 访问空图像的像素数据 ---" << std::endl;
    try
    {
        img::Image empty_img;
        empty_img.at<unsigned char>(0, 0);
    }
    catch (const std::exception &e)
    {
        std::cerr << "捕获到异常: " << e.what() << '\n';
    }

    // 4. 访问图像的越界像素
    std::cout << "\n--- 测试4: 访问图像的越界像素 ---" << std::endl;
    try
    {
        img::Image img_small(5, 5, IMG_8UC1);
        img_small.at<unsigned char>(10, 10); // 越界
    }
    catch (const std::exception &e)
    {
        std::cerr << "捕获到异常: " << e.what() << '\n';
    }

    // 5. 图像除以零
    std::cout << "\n--- 测试5: 图像除以零 ---" << std::endl;
    try
    {
        img::Image img_div(10, 10, IMG_32FC1);
        img_div /= 0.0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "捕获到异常: " << e.what() << '\n';
    }

    // 6. 不兼容图像之间的操作 (例如，不同尺寸相加)
    std::cout << "\n--- 测试6: 不兼容图像相加 ---" << std::endl;
    try
    {
        img::Image img_a(10, 10, IMG_8UC1);
        img::Image img_b(5, 5, IMG_8UC1); // 不同尺寸
        img_a += img_b;
    }
    catch (const std::exception &e)
    {
        std::cerr << "捕获到异常: " << e.what() << '\n';
    }

    // 7. 不兼容图像之间的操作 (例如，不同类型相加)
    std::cout << "\n--- 测试7: 不同类型图像相加 ---" << std::endl;
    try
    {
        img::Image img_c(10, 10, IMG_8UC1);
        img::Image img_d(10, 10, IMG_32FC1); // 不同类型
        img_c += img_d;
    }
    catch (const std::exception &e)
    {
        std::cerr << "捕获到异常: " << e.what() << '\n';
    }


    // 8. 创建无效的ROI (超出边界)
    std::cout << "\n--- 测试8: 创建无效ROI (超出边界) ---" << std::endl;
    try
    {
        img::Image img_roi_base(20, 20, IMG_8UC1);
        img_roi_base.roi(10, 10, 15, 15); // ROI 超出原始图像边界
    }
    catch (const std::exception &e)
    {
        std::cerr << "捕获到异常: " << e.what() << '\n';
    }

    // 9. 转换到通道数不匹配的类型
    std::cout << "\n--- 测试9: 转换到通道数不匹配的类型 ---" << std::endl;
    try
    {
        img::Image img_convert_src(10, 10, IMG_8UC1);
        img_convert_src.convert_to(IMG_8UC3); // 1通道转3通道 (当前库不支持)
    }
    catch (const std::exception &e)
    {
        std::cerr << "捕获到异常: " << e.what() << '\n';
    }

    // 10. 读取不存在的文件
    std::cout << "\n--- 测试10: 读取不存在的文件 ---" << std::endl;
    try
    {
        std::string input_path;
        std::cout << "请输入要读取的图像路径: ";
        std::cin >> input_path; // 等待用户输入路径
        img::Image read_img = img::imread(input_path);
        if(read_img.empty() && !input_path.empty()){ // imread 在失败时会打印错误并返回空图像
             std::cerr << "捕获到异常: imread 返回了空图像，可能由于文件问题或处理器问题。" << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "捕获到异常: " << e.what() << '\n';
    }

    // 11. 写入空图像
    std::cout << "\n--- 测试11: 写入空图像 ---" << std::endl;
    try
    {
        img::Image empty_img_to_write;
        bool success = img::imwrite(output_bad_file, empty_img_to_write);
        if(!success){ // imwrite 在失败时会打印错误并返回 false
             std::cerr << "捕获到异常: imwrite 返回 false，写入空图像失败。" << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "捕获到异常: " << e.what() << '\n';
    }
    
    // 12. 克隆空图像
    std::cout << "\n--- 测试12: 克隆空图像 ---" << std::endl;
    try
    {
        img::Image empty_img_to_clone;
        img::Image cloned_empty = empty_img_to_clone.clone();
    }
    catch (const std::exception &e)
    {
        std::cerr << "捕获到异常: " << e.what() << '\n';
    }

    std::cout << "\n异常测试完成。" << std::endl;
}
// ...existing code...