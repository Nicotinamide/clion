#pragma once

// Standard library includes first
#include <string>
#include <vector>
#include <map>
#include <iostream>

// OCCT includes
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
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

    // 加载STEP文件，返回是否成功，可选择是否移动到原点和是否自动修复
    bool loadStepFile(const std::string& filename, bool moveToOrigin = false, bool autoRepair = true);

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



    // 按Z高度对面进行分层
    std::map<double, TopTools_ListOfShape> groupFacesByHeight(const TopTools_ListOfShape& faces, double heightTolerance = 5.0) const;

    // 计算面的Z高度（中心点Z坐标）
    double calculateFaceHeight(const TopoDS_Face& face) const;

    // 使用缝合算法将面组合成shell
    TopoDS_Shape sewFacesToShells(const TopTools_ListOfShape& faces, double tolerance = 1.0) const;

    // 按高度分层并进行遮挡裁剪
    TopoDS_Shape removeOccludedPortions(const TopoDS_Shape& extractedFaces, double heightTolerance = 5.0);









    // 将一层的面合并为一个整体shell
    TopoDS_Shape mergeLayerToShell(const TopTools_ListOfShape& layerFaces, double layerHeight) const;

    // 层级整体裁剪：用上层整体裁剪下层整体
    TopoDS_Shape cutLayerWithLayer(const TopoDS_Shape& lowerLayerShell,
                                   const TopoDS_Shape& upperLayerShell,
                                   double upperHeight,
                                   double lowerHeight) const;

    // 将面投影到指定Z平面
    TopoDS_Shape projectFacesToPlane(const TopTools_ListOfShape& faces, double targetZ) const;

    // 将shell投影到指定Z平面
    TopoDS_Shape projectShellToPlane(const TopoDS_Shape& shell, double targetZ) const;

    // 从形状中提取所有面
    TopTools_ListOfShape extractFacesFromShape(const TopoDS_Shape& shape) const;

    // 检查两个层级shell是否在XY平面上重叠
    bool checkLayerOverlapInXY(const TopoDS_Shape& lowerShell, const TopoDS_Shape& upperShell) const;

    // 将形状移动到指定Z平面
    TopoDS_Shape moveShapeToPlane(const TopoDS_Shape& shape, double targetZ) const;

    // STEP导入后模型修复函数
    bool repairImportedModel(double tolerance = 1e-6, bool verbose = true);

    // 增强的模型修复函数（基于OCCT 7.9）
    bool enhancedModelRepair(double tolerance = 1e-6, bool verbose = true);

    // 针对喷涂轨迹优化的修复
    bool sprayTrajectoryOptimizedRepair(double tolerance = 1e-3, bool verbose = true);

    // 形状验证和分析
    bool validateAndAnalyzeShape(bool verbose = true);

    // 修复小面和小边
    bool fixSmallFacesAndEdges(double tolerance = 1e-6, bool verbose = true);

    // 修复线框问题
    bool fixWireframeIssues(double tolerance = 1e-6, bool verbose = true);

    // 检查两个面是否在XY平面上重叠
    bool checkFaceOverlapInXY(const TopoDS_Shape& face1, const TopoDS_Shape& face2) const;

    // 将面投影到指定Z平面
    TopoDS_Shape projectFaceToPlane(const TopoDS_Shape& face, double targetZ) const;

    // 专业级形状修复（包含详细分析和修复）
    bool professionalShapeHealing(double tolerance = 1e-6, bool verbose = true);

private:
    TopoDS_Shape shape;

    // 获取形状类型的字符串表示
    std::string getShapeTypeString(const TopAbs_ShapeEnum& shapeType) const;


};
