#pragma once

#include <string>
#include <vector>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <iostream> // 用于打印输出
#include <TopAbs_ShapeEnum.hxx> // 形状类型枚举
#include <TopoDS_Shell.hxx>
#include <TopTools_ListOfShape.hxx>
#include <gp_Dir.hxx>

class OCCHandler {
public:
    OCCHandler();
    ~OCCHandler();

    // 加载STEP文件，返回是否成功，可选择是否移动到原点
    bool loadStepFile(const std::string& filename, bool moveToOrigin = false);

    // 获取当前模型
    TopoDS_Shape getShape() const;

    // TopoDS_Shape转vtkPolyData
    // TopoDS_Shape转vtkPolyData（无默认参数）
    vtkSmartPointer<vtkPolyData> shapeToPolyData() const;

    // TopoDS_Shape转vtkPolyData（带参数）
    vtkSmartPointer<vtkPolyData> shapeToPolyData(const TopoDS_Shape& shape) const;

    // 打印TopoDS_Shape结构（根据形状类型停止递归）
    void printShapeStructure(const TopoDS_Shape& shape = TopoDS_Shape(),
                            TopAbs_ShapeEnum stopAtType = TopAbs_SHAPE,
                            std::ostream& out = std::cout,
                            int indent = 0) const;

    // 移动模型到原点
    void moveShapeToOrigin();

    // 绕指定坐标轴旋转90度（轴：0=X轴，1=Y轴，2=Z轴）
    void rotate90(gp_Dir axis);

    // 提取形状中的所有shell
    TopTools_ListOfShape extractAllShells(const TopoDS_Shape& sourceShape = TopoDS_Shape());

    // 基于法向量方向提取shell并创建新形状
    // direction: 参考方向
    // angleTolerance: shell主法向量与参考方向的最大夹角（度）
    // returnExtracted: 如果为true则返回提取的shell，否则返回剩余的shell
    TopoDS_Shape extractShellsByNormal(const gp_Dir& direction, double angleTolerance = 5.0, bool returnExtracted = true);

    // 计算shell的主法向量方向
    gp_Dir calculateShellMainNormal(const TopoDS_Shell& shell) const;

    // 创建一个只包含指定shell的新形状
    TopoDS_Shape createShapeFromShells(const TopTools_ListOfShape& shells);

    // 提取形状中的所有面
    TopTools_ListOfShape extractAllFaces(const TopoDS_Shape& sourceShape = TopoDS_Shape());

    // 基于法向量方向提取面并创建新形状
    // direction: 参考方向
    // angleTolerance: 面法向量与参考方向的最大夹角（度）
    // returnExtracted: 如果为true则返回提取的面，否则返回剩余的面
    TopoDS_Shape extractFacesByNormal(const gp_Dir& direction, double angleTolerance = 5.0, bool returnExtracted = true);

private:
    TopoDS_Shape shape;

    // 获取形状类型的字符串表示
    std::string getShapeTypeString(const TopAbs_ShapeEnum& shapeType) const;


};
