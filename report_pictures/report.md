# CS205 Advanced Programming Project3
## A Simple Image Processing Library

12310907 吴嘉淇

# 零、摘要:

![示意图](image0.png)

项目总体示意图：（使用开源软件绘制[draw.io](https://www.drawio.com/)）

在本次项目中，我实现了一个简单的图像处理库。库的整体实现主要由三个部分组成：

Image类（image.cpp）；

文件IO工厂（image_io.cpp）；

图像处理函数（processor.cpp）；

个人认为本次项目难度巨大,不过我有注意处理一些常见的内存问题以及用户交互可能导致的程序问题,并且进行了异常处理.

如果希望了解每一个函数的具体作用以及使用协议,可以查看库的帮助文档或是imglib.h文件中的注释.

在报告中，我讲分块讲述每一个部分的重难点的实现逻辑，遇到的困难以及最终解决方法.

# 一、Image类 —— 库的核心数据结构

Image类的实现参考了非常多openCV中ce::Mat类的特点.

以下是Image类的总体设计示意图：


