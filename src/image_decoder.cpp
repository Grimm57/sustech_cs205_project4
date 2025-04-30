#include "imglib.h"
#include <iostream>

// BMPDecoder 实现
bool BMPDecoder::checkFormat(const std::string &filename) const {
    return filename.size() > 4 && filename.substr(filename.size() - 4) == ".bmp";
}

Mat BMPDecoder::decode(const std::string &filename, int flags) const {
    std::cout << "Decoding BMP: " << filename << std::endl;
    // 简单返回空矩阵
    return Mat();
}

// PNGDecoder 实现
bool PNGDecoder::checkFormat(const std::string &filename) const {
    return filename.size() > 4 && filename.substr(filename.size() - 4) == ".png";
}

Mat PNGDecoder::decode(const std::string &filename, int flags) const {
    std::cout << "Decoding PNG: " << filename << std::endl;
    return Mat();
}

// JPEGDecoder 实现
bool JPEGDecoder::checkFormat(const std::string &filename) const {
    return filename.size() > 4 && filename.substr(filename.size() - 4) == ".jpg";
}

Mat JPEGDecoder::decode(const std::string &filename, int flags) const {
    std::cout << "Decoding JPEG: " << filename << std::endl;
    return Mat();
}

// 查找解码器
ImageDecoder *findDecoder(const std::string &filename) {
    static BMPDecoder bmpDecoder;
    static PNGDecoder pngDecoder;
    static JPEGDecoder jpegDecoder;

    if (bmpDecoder.checkFormat(filename)) return &bmpDecoder;
    if (pngDecoder.checkFormat(filename)) return &pngDecoder;
    if (jpegDecoder.checkFormat(filename)) return &jpegDecoder;

    return nullptr;
}