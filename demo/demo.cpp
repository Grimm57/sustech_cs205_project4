#include "imglib.h"
#include <iostream>

int main() {
    // 测试 Mat 类
    std::cout << "Testing Mat class..." << std::endl;
    Mat mat(5, 5, CV_8UC1); // 创建一个 5x5 的单通道矩阵
    if (!mat.empty()) {
        std::cout << "Matrix created successfully: " << mat.rows() << "x" << mat.cols() << std::endl;
        std::cout << "Matrix type: " << mat.type() << ", Channels: " << mat.channels() << ", Depth: " << mat.depth() << std::endl;
    } else {
        std::cout << "Failed to create matrix." << std::endl;
    }

    // 测试解码器查找
    std::cout << "\nTesting decoder lookup..." << std::endl;
    ImageDecoder *decoder = findDecoder("example.png");
    if (decoder) {
        std::cout << "Decoder found for example.png" << std::endl;
    } else {
        std::cout << "No decoder found for example.png" << std::endl;
    }
    Mat test = imread("example.png",CV_8UC3); // 使用解码器读取图像
    if (!mat.empty()) {
        std::cout << "Image read successfully!" << std::endl;
    } else {
        std::cout << "Failed to read image." << std::endl;
    }
    // 测试解码功能
    std::cout << "\nTesting image decoding..." << std::endl;
    Mat decodedImage = decoder->decode("example.png");
    if (!decodedImage.empty()) {
        std::cout << "Image decoded successfully!" << std::endl;
    } else {
        std::cout << "Failed to decode image." << std::endl;
    }

    return 0;
}