#include "FaceProcessor.h"
#include <BRep_Tool.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <TopoDS.hxx>
#include <TopExp_Explorer.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <gp_Pln.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkPolyLine.h>
#include <vtkPolyData.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkUnsignedCharArray.h>
#include <vtkTriangle.h>
#include <BRepTools_WireExplorer.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <GeomLProp_SLProps.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <BRepTools.hxx>

// 构造函数
FaceProcessor::FaceProcessor() : pathSpacing(10.0), offsetDistance(5.0), pointDensity(1.0), minPathLength(20.0) {
}

// 析构函数
FaceProcessor::~FaceProcessor() {
}

// 设置要处理的形状
void FaceProcessor::setShape(const TopoDS_Shape& shape) {
    inputFaces = shape;
    clearPaths();
}

// 设置切割参数
void FaceProcessor::setCuttingParameters(gp_Dir cutdirection ,double spacing, double offset, double density) {
    faceDirection = cutdirection; // 设置切割方向
    pathSpacing = spacing;
    offsetDistance = offset;
    if (density <= 0.0) {
        std::cerr << "警告：点密度必须大于0，设置为默认值1.0" << std::endl;
        pointDensity = 1.0;
    } else {
        pointDensity = density;
    }
}

// 设置最小路径长度
void FaceProcessor::setMinPathLength(double minLength) {
    if (minLength < 0.0) {
        std::cerr << "警告：最小路径长度不能为负数，设置为0.0" << std::endl;
        minPathLength = 0.0;
    } else {
        minPathLength = minLength;
    }
}

// 自动检测并调整单位
void FaceProcessor::autoDetectAndAdjustUnits() {
    if (generatedPaths.empty()) {
        std::cout << "没有路径可用于单位检测" << std::endl;
        return;
    }

    // 计算所有路径的长度统计
    std::vector<double> lengths;
    for (const auto& path : generatedPaths) {
        double length = calculatePathLength(path);
        if (length > 0) {
            lengths.push_back(length);
        }
    }

    if (lengths.empty()) {
        std::cout << "没有有效长度的路径用于单位检测" << std::endl;
        return;
    }

    // 计算统计信息
    std::sort(lengths.begin(), lengths.end());
    double minLength = lengths.front();
    double maxLength = lengths.back();
    double medianLength = lengths[lengths.size() / 2];

    std::cout << "\n=== 单位自动检测 ===" << std::endl;
    std::cout << "路径长度统计:" << std::endl;
    std::cout << "- 最短: " << std::fixed << std::setprecision(6) << minLength << std::endl;
    std::cout << "- 最长: " << std::fixed << std::setprecision(6) << maxLength << std::endl;
    std::cout << "- 中位数: " << std::fixed << std::setprecision(6) << medianLength << std::endl;

    // 基于长度范围推测单位
    std::string detectedUnit = "未知";
    double suggestedThreshold = minPathLength;

    if (medianLength > 1000) {
        detectedUnit = "毫米 (mm)";
        suggestedThreshold = 20.0;  // 20mm
    } else if (medianLength > 10) {
        detectedUnit = "厘米 (cm)";
        suggestedThreshold = 2.0;   // 2cm = 20mm
    } else if (medianLength > 0.1) {
        detectedUnit = "分米 (dm) 或英寸 (inch)";
        suggestedThreshold = 0.2;   // 0.2dm = 20mm
    } else if (medianLength > 0.001) {
        detectedUnit = "米 (m)";
        suggestedThreshold = 0.02;  // 0.02m = 20mm
    } else {
        detectedUnit = "千米 (km) 或其他大单位";
        suggestedThreshold = 0.00002; // 0.00002km = 20mm
    }

    std::cout << "推测单位: " << detectedUnit << std::endl;
    std::cout << "建议的最小路径长度阈值: " << suggestedThreshold << std::endl;

    // 询问是否自动调整
    std::cout << "当前阈值: " << minPathLength << std::endl;
    if (std::abs(minPathLength - suggestedThreshold) > suggestedThreshold * 0.1) {
        std::cout << "建议将阈值调整为: " << suggestedThreshold << std::endl;
        // 自动调整（在实际应用中可能需要用户确认）
        minPathLength = suggestedThreshold;
        std::cout << "已自动调整最小路径长度阈值为: " << minPathLength << std::endl;
    } else {
        std::cout << "当前阈值合理，无需调整" << std::endl;
    }
}

// 生成切割平面（只为可见面生成）
bool FaceProcessor::generateCuttingPlanes() {
    if (visibleFaces.empty()) {
        std::cerr << "No visible faces available for cutting plane generation. Call analyzeFaceVisibility() first." << std::endl;
        return false;
    }

    // 清空之前的切割平面
    cuttingPlanes.clear();

    std::cout << "Generating cutting planes for " << visibleFaces.size() << " visible faces..." << std::endl;

    // 创建包含所有可见面的复合形状
    TopoDS_Compound visibleCompound;
    BRep_Builder builder;
    builder.MakeCompound(visibleCompound);

    for (const auto& face : visibleFaces) {
        builder.Add(visibleCompound, face);
    }

    // 计算可见面的整体包围盒
    Bnd_Box boundingBox;
    BRepBndLib::Add(visibleCompound, boundingBox);

    if (boundingBox.IsVoid()) {
        std::cerr << "无法计算形状的包围盒" << std::endl;
        return false;
    }

    double xMin, yMin, zMin, xMax, yMax, zMax;
    boundingBox.Get(xMin, yMin, zMin, xMax, yMax, zMax);

    // 获取法向量作为 z 方向
    gp_Dir zDir = faceDirection;

    // 计算最长边方向
    double xLen = xMax - xMin;
    double yLen = yMax - yMin;
    double zLen = zMax - zMin;

    gp_Dir longestEdgeDir;
    if (xLen >= yLen && xLen >= zLen) {
        longestEdgeDir = gp_Dir(1, 0, 0);
    } else if (yLen >= xLen && yLen >= zLen) {
        longestEdgeDir = gp_Dir(0, 1, 0);
    } else {
        longestEdgeDir = gp_Dir(0, 0, 1);
    }

    // 将最长边方向设置为 y 方向，并确保它与 z 方向（法向量）垂直
    gp_Dir yDir;
    if (fabs(zDir.Dot(longestEdgeDir)) > 0.1) {
        // 如果最长边方向与法向量不垂直，则创建一个垂直于法向量的临时方向
        gp_Dir tempDir(1, 0, 0);
        if (fabs(zDir.Dot(tempDir)) > 0.9) {
            tempDir = gp_Dir(0, 1, 0);
        }
        // 用叉积计算垂直于法向量的方向
        yDir = zDir.Crossed(tempDir);
    } else {
        // 如果最长边方向已经与法向量垂直，直接使用它
        yDir = longestEdgeDir;
    }

    // 使用 z 和 y 方向的叉积计算 x 方向（切割平面的方向）
    gp_Dir xDir = zDir.Crossed(yDir);

    // 计算中心点
    gp_Pnt center((xMin + xMax)/2, (yMin + yMax)/2, (zMin + zMax)/2);

    // 计算在 x 方向上的最大尺寸
    // 计算对角线向量
    gp_Vec diag(xMax - xMin, yMax - yMin, zMax - zMin);
    // 计算对角线在 x 方向上的投影长度
    double xProjection = fabs(diag.Dot(gp_Vec(xDir.X(), xDir.Y(), xDir.Z())));
    double length = xProjection;

    // 计算切割平面的起始位置（从包围盒一端开始）
    double startPos = -length / 2;
    double endPos = length / 2;

    // 生成切割平面，沿 x 方向（xDir）插入
    for (double pos = startPos; pos <= endPos; pos += pathSpacing) {
        // 沿 x 方向偏移中心点
        gp_Vec offset(xDir.X() * pos, xDir.Y() * pos, xDir.Z() * pos);
        gp_Pnt planeOrigin = center.Translated(offset);

        // 创建平面（点和法向量为 x 方向）
        gp_Pln cuttingPlane(planeOrigin, xDir);
        cuttingPlanes.push_back(cuttingPlane);
    }

    std::cout << "生成了 " << cuttingPlanes.size() << " 个切割平面" << std::endl;
    return !cuttingPlanes.empty();

}

// 生成路径（只为可见面生成路径）
bool FaceProcessor::generatePaths() {
    if (visibleFaces.empty()) {
        std::cerr << "No visible faces available for path generation. Call analyzeFaceVisibility() first." << std::endl;
        return false;
    }

    // 首先生成切割平面
    if (cuttingPlanes.empty() && !generateCuttingPlanes()) {
        return false;
    }

    // 清空之前的路径
    clearPaths();

    std::cout << "Generating paths for " << visibleFaces.size() << " visible faces..." << std::endl;

    int pathCount = 0;

    // 创建包含所有可见面的复合形状
    TopoDS_Compound visibleCompound;
    BRep_Builder builder;
    builder.MakeCompound(visibleCompound);

    for (const auto& face : visibleFaces) {
        builder.Add(visibleCompound, face);
    }

    // 对每个切割平面
    for (size_t i = 0; i < cuttingPlanes.size(); i++) {
        // 创建切割平面
        TopoDS_Face planeFace = BRepBuilderAPI_MakeFace(cuttingPlanes[i]).Face();

        // 计算与可见面的交线
        BRepAlgoAPI_Section section(visibleCompound, planeFace, Standard_False);
        section.Build();

        if (!section.IsDone() || section.Shape().IsNull()) {
            continue;
        }

        // 对每条交线（每个Edge）单独生成一条路径
        for (TopExp_Explorer edgeExplorer(section.Shape(), TopAbs_EDGE); edgeExplorer.More(); edgeExplorer.Next()) {
            TopoDS_Edge edge = TopoDS::Edge(edgeExplorer.Current());

            // 获取边上的参数范围
            double start, end;
            Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, start, end);

            if (curve.IsNull()) {
                continue;
            }

            // 沿边创建点，使用pointDensity参数来控制点的密度
            double curveLength = (end - start);
            int numPoints = std::max(10, int(curveLength * pointDensity));
            std::vector<PathPoint> intersectionPoints;

            for (int j = 0; j <= numPoints; j++) {
                double t = start + (end - start) * j / numPoints;
                gp_Pnt point;
                curve->D0(t, point);

                // 获取面在该点的法向量
                gp_Dir faceNormal = faceDirection;

                // 创建路径点
                PathPoint pathPoint(point, faceNormal);
                intersectionPoints.push_back(pathPoint);
            }

            if (!intersectionPoints.empty()) {
                // 创建路径
                SprayPath path;
                createPathFromIntersection(intersectionPoints, offsetDistance, path);

                // 计算路径长度并进行筛选
                double pathLength = calculatePathLength(path);

                // 添加调试信息
                if (pathCount < 10) {  // 只显示前10条路径的调试信息
                    std::cout << "路径 " << pathCount << ": 点数=" << path.points.size()
                              << ", 长度=" << std::fixed << std::setprecision(2) << pathLength
                              << "mm, 阈值=" << minPathLength << "mm";
                }

                // 只保留长度大于等于最小长度的路径
                if (pathLength >= minPathLength) {
                    if (pathCount < 10) {
                        std::cout << " -> 保留" << std::endl;
                    }

                    // 设置路径索引和宽度
                    path.pathIndex = pathCount++;
                    path.width = pathSpacing;
                    path.planeIndex = i;  // 记录所属的切割平面索引
                    path.isConnected = false;  // 初始化为未连接状态

                    // 添加到路径列表
                    generatedPaths.push_back(path);
                } else {
                    if (pathCount < 10) {
                        std::cout << " -> 过滤" << std::endl;
                    }
                    pathCount++;  // 仍然增加计数器以保持调试信息的连续性
                }
            }
        }
    }

    std::cout << "生成了 " << generatedPaths.size() << " 条路径（已过滤长度小于 "
              << minPathLength << "mm 的短路径）" << std::endl;

    // 输出一些统计信息
    if (!generatedPaths.empty()) {
        double totalLength = 0.0;
        double minLength = std::numeric_limits<double>::max();
        double maxLength = 0.0;

        for (const auto& path : generatedPaths) {
            double length = calculatePathLength(path);
            totalLength += length;
            minLength = std::min(minLength, length);
            maxLength = std::max(maxLength, length);
        }

        std::cout << "路径长度统计: 总长=" << std::fixed << std::setprecision(1) << totalLength
                  << "mm, 平均=" << (totalLength/generatedPaths.size())
                  << "mm, 最短=" << minLength
                  << "mm, 最长=" << maxLength << "mm" << std::endl;

        // 如果发现路径长度异常，自动检测单位
        if (minLength < 1.0 || maxLength > 10000.0) {
            std::cout << "检测到异常的路径长度，启动单位自动检测..." << std::endl;
            autoDetectAndAdjustUnits();
        }
    }
    return !generatedPaths.empty();
}

// 清除所有路径
void FaceProcessor::clearPaths() {
    generatedPaths.clear();
    connectionPaths.clear();
    integratedTrajectories.clear();
    pathVisibility.clear();
    surfaceLayers.clear();
}

// 从交线创建路径
void FaceProcessor::createPathFromIntersection(const std::vector<PathPoint>& intersectionPoints,
                                            double offsetDistance, SprayPath& path) {
    if (intersectionPoints.empty()) {
        return;
    }

    // 清空路径点
    path.points.clear();

    // 对每个交点，创建偏移的路径点
    for (const auto& point : intersectionPoints) {
        // 沿面法向量方向偏移点
        gp_Vec offsetVec(point.normal.X(), point.normal.Y(), point.normal.Z());
        offsetVec *= offsetDistance;

        gp_Pnt offsetPoint = point.position.Translated(offsetVec);

        // 添加到路径
        path.points.push_back(PathPoint(offsetPoint, point.normal));
    }
}

// 计算路径长度
double FaceProcessor::calculatePathLength(const SprayPath& path) const {
    if (path.points.size() < 2) {
        return 0.0;
    }

    double totalLength = 0.0;
    double maxSegment = 0.0;
    double minSegment = std::numeric_limits<double>::max();

    for (size_t i = 1; i < path.points.size(); i++) {
        const gp_Pnt& p1 = path.points[i-1].position;
        const gp_Pnt& p2 = path.points[i].position;
        double segmentLength = p1.Distance(p2);
        totalLength += segmentLength;

        maxSegment = std::max(maxSegment, segmentLength);
        minSegment = std::min(minSegment, segmentLength);
    }

    // 对于前几条路径，输出详细的调试信息
    static int debugCount = 0;
    if (debugCount < 5 && path.points.size() > 1) {
        std::cout << "  详细信息: 段数=" << (path.points.size()-1)
                  << ", 最长段=" << std::fixed << std::setprecision(3) << maxSegment
                  << ", 最短段=" << std::fixed << std::setprecision(3) << minSegment
                  << ", 平均段=" << std::fixed << std::setprecision(3) << (totalLength/(path.points.size()-1))
                  << std::endl;
        debugCount++;
    }

    return totalLength;
}

// 检测单位并设置最优阈值
void FaceProcessor::detectAndSetOptimalThreshold() {
    if (generatedPaths.empty()) {
        std::cout << "No paths available for unit detection. Generate paths first." << std::endl;
        return;
    }

    // 创建测试路径来检测单位
    SprayPath testPath;
    testPath.points.push_back(PathPoint(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
    testPath.points.push_back(PathPoint(gp_Pnt(10, 0, 0), gp_Dir(0, 0, 1)));

    double testLength = calculatePathLength(testPath);
    std::cout << "Unit detection: 10-unit test path length = " << testLength << std::endl;

    // 根据测试长度判断单位并设置阈值
    if (testLength > 9.99 && testLength < 10.01) {
        std::cout << "Detected units: millimeters (mm)" << std::endl;
        setMinPathLength(20.0);
        std::cout << "Set threshold to: 20.0 mm" << std::endl;
    } else if (testLength > 0.0099 && testLength < 0.0101) {
        std::cout << "Detected units: meters (m)" << std::endl;
        setMinPathLength(0.02);
        std::cout << "Set threshold to: 0.02 m (20mm)" << std::endl;
    } else if (testLength > 0.99 && testLength < 1.01) {
        std::cout << "Detected units: centimeters (cm)" << std::endl;
        setMinPathLength(2.0);
        std::cout << "Set threshold to: 2.0 cm (20mm)" << std::endl;
    } else if (testLength > 0.39 && testLength < 0.40) {
        std::cout << "Detected units: inches (in)" << std::endl;
        setMinPathLength(0.787);
        std::cout << "Set threshold to: 0.787 in (20mm)" << std::endl;
    } else {
        std::cout << "Unknown units detected. Test length: " << testLength << std::endl;
        std::cout << "Please manually set appropriate threshold." << std::endl;
    }
}

// 检测单位
std::string FaceProcessor::detectUnits() const {
    SprayPath testPath;
    testPath.points.push_back(PathPoint(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
    testPath.points.push_back(PathPoint(gp_Pnt(10, 0, 0), gp_Dir(0, 0, 1)));

    double testLength = calculatePathLength(testPath);

    if (testLength > 9.99 && testLength < 10.01) {
        return "millimeters";
    } else if (testLength > 0.0099 && testLength < 0.0101) {
        return "meters";
    } else if (testLength > 0.99 && testLength < 1.01) {
        return "centimeters";
    } else if (testLength > 0.39 && testLength < 0.40) {
        return "inches";
    } else {
        return "unknown";
    }
}

// 打印路径长度统计信息
void FaceProcessor::printPathLengthStatistics() const {
    if (generatedPaths.empty()) {
        std::cout << "No paths available for statistics." << std::endl;
        return;
    }

    std::vector<double> lengths;
    for (const auto& path : generatedPaths) {
        double length = calculatePathLength(path);
        lengths.push_back(length);
    }

    if (lengths.empty()) return;

    // 计算统计信息
    double minLength = *std::min_element(lengths.begin(), lengths.end());
    double maxLength = *std::max_element(lengths.begin(), lengths.end());
    double totalLength = 0.0;
    for (double len : lengths) {
        totalLength += len;
    }
    double avgLength = totalLength / lengths.size();

    std::cout << "\n=== Path Length Statistics ===" << std::endl;
    std::cout << "Total paths: " << lengths.size() << std::endl;
    std::cout << "Min length: " << std::fixed << std::setprecision(6) << minLength << std::endl;
    std::cout << "Max length: " << std::fixed << std::setprecision(6) << maxLength << std::endl;
    std::cout << "Average length: " << std::fixed << std::setprecision(6) << avgLength << std::endl;
    std::cout << "Total length: " << std::fixed << std::setprecision(6) << totalLength << std::endl;
    std::cout << "Current threshold: " << minPathLength << std::endl;

    // 计算会被过滤的路径数量
    int filteredCount = 0;
    for (double len : lengths) {
        if (len < minPathLength) {
            filteredCount++;
        }
    }

    std::cout << "Paths that would be filtered: " << filteredCount
              << " (" << (100.0 * filteredCount / lengths.size()) << "%)" << std::endl;

    std::string units = detectUnits();
    std::cout << "Detected units: " << units << std::endl;
}

// 分析面的可见性（应该首先调用）
bool FaceProcessor::analyzeFaceVisibility() {
    if (inputFaces.IsNull()) {
        std::cerr << "No input faces available for visibility analysis" << std::endl;
        return false;
    }

    std::cout << "Starting face-level visibility analysis..." << std::endl;

    // 1. 从输入形状中提取所有面
    extractFacesFromShape();

    if (faceVisibility.empty()) {
        std::cerr << "No faces found in input shape" << std::endl;
        return false;
    }

    // 2. 计算每个面的深度
    calculateFaceDepths();

    // 3. 检测面之间的遮挡关系（包括部分遮挡）
    detectPartialFaceOcclusions();

    // 4. 提取可见面（包括部分可见面）
    visibleFaces.clear();
    int fullyVisibleCount = 0;
    int partiallyVisibleCount = 0;
    int occludedCount = 0;

    for (const auto& faceInfo : faceVisibility) {
        if (faceInfo.isVisible) {
            // 保留所有可见面（包括部分可见）
            visibleFaces.push_back(faceInfo.face);

            if (faceInfo.isPartiallyVisible) {
                partiallyVisibleCount++;
            } else {
                fullyVisibleCount++;
            }
        } else {
            occludedCount++;
        }
    }

    std::cout << "Face visibility analysis completed:" << std::endl;
    std::cout << "  Total faces: " << faceVisibility.size() << std::endl;
    std::cout << "  Fully visible faces: " << fullyVisibleCount << std::endl;
    std::cout << "  Partially visible faces: " << partiallyVisibleCount << std::endl;
    std::cout << "  Total visible faces: " << (fullyVisibleCount + partiallyVisibleCount) << std::endl;
    std::cout << "  Occluded faces: " << occludedCount << std::endl;

    return (fullyVisibleCount + partiallyVisibleCount) > 0;
}

// 获取可见面
std::vector<TopoDS_Face> FaceProcessor::getVisibleFaces() const {
    return visibleFaces;
}

// 从输入形状中提取所有面
void FaceProcessor::extractFacesFromShape() {
    faceVisibility.clear();

    int faceIndex = 0;
    for (TopExp_Explorer faceExplorer(inputFaces, TopAbs_FACE); faceExplorer.More(); faceExplorer.Next()) {
        TopoDS_Face face = TopoDS::Face(faceExplorer.Current());

        FaceVisibilityInfo faceInfo;
        faceInfo.face = face;
        faceInfo.faceIndex = faceIndex++;
        faceInfo.centerPoint = calculateFaceCenter(face);
        faceInfo.normal = calculateFaceNormal(face);
        faceInfo.area = calculateFaceArea(face);
        faceInfo.isVisible = true; // 初始假设都可见
        faceInfo.isPartiallyVisible = false; // 初始假设不是部分可见
        faceInfo.visibilityRatio = 1.0; // 初始完全可见
        faceInfo.depth = 0.0; // 将在calculateFaceDepths中计算

        faceVisibility.push_back(faceInfo);
    }

    std::cout << "Extracted " << faceVisibility.size() << " faces from input shape" << std::endl;
}

// 计算面的深度
void FaceProcessor::calculateFaceDepths() {
    // 使用Z+方向作为深度方向（与面提取方向一致）
    gp_Dir depthDirection(0, 0, 1);

    for (auto& faceInfo : faceVisibility) {
        // 计算面中心点在深度方向上的投影
        gp_Vec centerVec(faceInfo.centerPoint.XYZ());
        faceInfo.depth = centerVec.Dot(gp_Vec(depthDirection.XYZ()));
    }

    std::cout << "Calculated depths for " << faceVisibility.size() << " faces" << std::endl;
}

// 检测面之间的遮挡关系
void FaceProcessor::detectFaceOcclusions() {
    const double DEPTH_THRESHOLD = pathSpacing * 0.1; // 深度差阈值

    int occludedCount = 0;

    for (size_t i = 0; i < faceVisibility.size(); i++) {
        if (!faceVisibility[i].isVisible) continue;

        // 检查是否被其他面遮挡
        for (size_t j = 0; j < faceVisibility.size(); j++) {
            if (i == j || !faceVisibility[j].isVisible) continue;

            // 在Z+方向上，Z值更大的面遮挡Z值更小的面
            if (faceVisibility[j].depth <= faceVisibility[i].depth + DEPTH_THRESHOLD) continue;

            // 检查是否存在遮挡
            if (isFaceOccluded(i, j)) {
                faceVisibility[i].isVisible = false;
                occludedCount++;
                break; // 一旦被遮挡就不需要继续检查
            }
        }
    }

    std::cout << "Face occlusion detection completed: " << occludedCount << " faces occluded" << std::endl;
}

// 检测部分面遮挡关系（改进版本）
void FaceProcessor::detectPartialFaceOcclusions() {
    const double DEPTH_THRESHOLD = pathSpacing * 0.1; // 深度差阈值
    const double MIN_VISIBILITY_RATIO = 0.1; // 最小可见性比例阈值

    int fullyOccludedCount = 0;
    int partiallyOccludedCount = 0;

    for (size_t i = 0; i < faceVisibility.size(); i++) {
        if (!faceVisibility[i].isVisible) continue;

        double totalVisibilityRatio = 1.0;
        std::vector<int> occluders;

        // 检查是否被其他面遮挡
        for (size_t j = 0; j < faceVisibility.size(); j++) {
            if (i == j || !faceVisibility[j].isVisible) continue;

            // 在Z+方向上，Z值更大的面遮挡Z值更小的面
            if (faceVisibility[j].depth <= faceVisibility[i].depth + DEPTH_THRESHOLD) continue;

            // 计算遮挡比例
            double occlusionRatio = calculateFaceVisibilityRatio(i, j);
            if (occlusionRatio > 0.0) {
                occluders.push_back(j);
                totalVisibilityRatio *= (1.0 - occlusionRatio);
            }
        }

        // 更新面的可见性信息
        faceVisibility[i].visibilityRatio = totalVisibilityRatio;
        faceVisibility[i].occludingFaces = occluders;

        if (totalVisibilityRatio < MIN_VISIBILITY_RATIO) {
            // 几乎完全被遮挡，标记为不可见
            faceVisibility[i].isVisible = false;
            faceVisibility[i].isPartiallyVisible = false;
            fullyOccludedCount++;
        } else if (totalVisibilityRatio < 0.8) {
            // 部分被遮挡，但仍然保留
            faceVisibility[i].isVisible = true;
            faceVisibility[i].isPartiallyVisible = true;
            partiallyOccludedCount++;
        } else {
            // 基本完全可见
            faceVisibility[i].isVisible = true;
            faceVisibility[i].isPartiallyVisible = false;
        }
    }

    std::cout << "Improved face occlusion detection completed:" << std::endl;
    std::cout << "  Fully occluded faces: " << fullyOccludedCount << std::endl;
    std::cout << "  Partially occluded faces: " << partiallyOccludedCount << std::endl;
    std::cout << "  Fully visible faces: " << (faceVisibility.size() - fullyOccludedCount - partiallyOccludedCount) << std::endl;
}

// 计算面的可见性比例
double FaceProcessor::calculateFaceVisibilityRatio(int faceIndex, int candidateOccluderIndex) {
    const FaceVisibilityInfo& face = faceVisibility[faceIndex];
    const FaceVisibilityInfo& occluder = faceVisibility[candidateOccluderIndex];

    // 计算两个面中心点在XY平面上的距离
    double distance = sqrt(pow(face.centerPoint.X() - occluder.centerPoint.X(), 2) +
                          pow(face.centerPoint.Y() - occluder.centerPoint.Y(), 2));

    // 基于面积计算影响半径
    double faceRadius = sqrt(face.area / M_PI);
    double occluderRadius = sqrt(occluder.area / M_PI);

    // 如果距离大于两个半径之和，没有遮挡
    if (distance >= faceRadius + occluderRadius) {
        return 0.0;
    }

    // 如果遮挡面完全覆盖被遮挡面
    if (distance + faceRadius <= occluderRadius) {
        return 1.0; // 完全遮挡
    }

    // 如果被遮挡面完全覆盖遮挡面
    if (distance + occluderRadius <= faceRadius) {
        return pow(occluderRadius / faceRadius, 2); // 按面积比例计算遮挡
    }

    // 部分重叠的情况 - 使用简化的重叠面积计算
    double overlapRatio = 1.0 - (distance / (faceRadius + occluderRadius));
    overlapRatio = std::max(0.0, std::min(1.0, overlapRatio));

    // 考虑面积差异的影响
    double areaRatio = std::min(occluder.area / face.area, 1.0);

    return overlapRatio * areaRatio;
}

// 判断是否应该保留部分可见面
bool FaceProcessor::shouldKeepPartiallyVisibleFace(const FaceVisibilityInfo& faceInfo) {
    // 如果可见性比例太低，不保留
    if (faceInfo.visibilityRatio < 0.1) {
        return false;
    }

    // 如果面积太小且可见性比例不高，不保留
    if (faceInfo.area < 100.0 && faceInfo.visibilityRatio < 0.3) {
        return false;
    }

    // 其他情况保留
    return true;
}

// 检查面是否被遮挡
bool FaceProcessor::isFaceOccluded(int faceIndex, int candidateOccluderIndex) {
    const FaceVisibilityInfo& face = faceVisibility[faceIndex];
    const FaceVisibilityInfo& occluder = faceVisibility[candidateOccluderIndex];

    // 简化的遮挡检测：检查面中心点的XY投影是否重叠
    double distance = sqrt(pow(face.centerPoint.X() - occluder.centerPoint.X(), 2) +
                          pow(face.centerPoint.Y() - occluder.centerPoint.Y(), 2));

    // 如果两个面的中心点在XY平面上的距离小于某个阈值，认为存在遮挡
    double overlapThreshold = sqrt(face.area + occluder.area) * 0.1; // 基于面积的阈值

    return distance < overlapThreshold;
}

// 计算面的中心点
gp_Pnt FaceProcessor::calculateFaceCenter(const TopoDS_Face& face) {
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    return props.CentreOfMass();
}

// 计算面的法向量
gp_Dir FaceProcessor::calculateFaceNormal(const TopoDS_Face& face) {
    // 获取面的中心点处的法向量
    Handle(Geom_Surface) surface = BRep_Tool::Surface(face);

    // 获取参数范围
    Standard_Real uMin, uMax, vMin, vMax;
    BRepTools::UVBounds(face, uMin, uMax, vMin, vMax);

    // 计算中心参数
    Standard_Real uMid = (uMin + uMax) * 0.5;
    Standard_Real vMid = (vMin + vMax) * 0.5;

    // 计算法向量
    gp_Pnt point;
    gp_Vec du, dv;
    surface->D1(uMid, vMid, point, du, dv);

    gp_Vec normal = du.Crossed(dv);
    if (normal.Magnitude() > 1e-10) {
        normal.Normalize();

        // 确保法向量指向正确方向（与面的方向一致）
        if (face.Orientation() == TopAbs_REVERSED) {
            normal.Reverse();
        }

        return gp_Dir(normal);
    }

    // 如果计算失败，返回默认方向
    return gp_Dir(0, 0, 1);
}

// 计算面的面积
double FaceProcessor::calculateFaceArea(const TopoDS_Face& face) {
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    return props.Mass(); // 对于面，Mass()返回面积
}

// 整合轨迹 - 将多条分散的路径整合为连续的喷涂轨迹
bool FaceProcessor::integrateTrajectories() {
    if (generatedPaths.empty()) {
        std::cerr << "没有可用的路径进行整合" << std::endl;
        return false;
    }

    // 清空之前的整合结果
    integratedTrajectories.clear();
    connectionPaths.clear();

    // 按切割平面分组路径
    groupPathsByPlane();

    std::cout << "开始整合 " << generatedPaths.size() << " 条路径..." << std::endl;
    return !integratedTrajectories.empty();
}

// 获取生成的路径
const std::vector<SprayPath>& FaceProcessor::getPaths() const {
    return generatedPaths;
}

// 获取整合后的轨迹
const std::vector<IntegratedTrajectory>& FaceProcessor::getIntegratedTrajectories() const {
    return integratedTrajectories;
}

// 获取表面层级信息
const std::vector<SurfaceLayer>& FaceProcessor::getSurfaceLayers() const {
    return surfaceLayers;
}

// 将路径转换为VTK PolyData用于可视化
vtkSmartPointer<vtkPolyData> FaceProcessor::pathsToPolyData(bool onlySprayPaths) const {
    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto cells = vtkSmartPointer<vtkCellArray>::New();

    // 创建一个数组用于存储路径索引（可用于颜色映射）
    auto pathIdArray = vtkSmartPointer<vtkDoubleArray>::New();
    pathIdArray->SetName("PathIndex");

    // 创建一个数组用于存储是否为喷涂点
    auto sprayPointArray = vtkSmartPointer<vtkDoubleArray>::New();
    sprayPointArray->SetName("IsSprayPoint");

    // 创建一个数组用于存储法向量
    auto normalArray = vtkSmartPointer<vtkDoubleArray>::New();
    normalArray->SetNumberOfComponents(3);
    normalArray->SetName("Normals");

    // 添加颜色数组，使所有路径都可见
    auto colorArray = vtkSmartPointer<vtkUnsignedCharArray>::New();
    colorArray->SetNumberOfComponents(3);
    colorArray->SetName("Colors");

    int pointIndex = 0;

    // 遍历每一条喷涂路径
    for (const auto& path : generatedPaths) {
        std::vector<vtkIdType> pointIds;

        // 检查是否有足够的点创建路径
        if (path.points.size() < 2) {
            continue;  // 跳过少于2个点的路径
        }

        // 处理路径中的点
        for (size_t i = 0; i < path.points.size(); ++i) {
            // 如果只显示喷涂路径且当前点不是喷涂点，则跳过
            if (onlySprayPaths && !path.points[i].isSprayPoint) {
                continue;
            }

            const auto& point = path.points[i];
            points->InsertNextPoint(point.position.X(), point.position.Y(), point.position.Z());
            pointIds.push_back(pointIndex);

            // 存储法向量
            double normal[3] = {point.normal.X(), point.normal.Y(), point.normal.Z()};
            normalArray->InsertNextTuple(normal);

            // 存储路径索引
            pathIdArray->InsertNextValue(path.pathIndex);

            // 存储是否为喷涂点
            sprayPointArray->InsertNextValue(point.isSprayPoint ? 1.0 : 0.0);

            // 为每个点添加颜色 - 根据是否为喷涂点区分
            if (point.isSprayPoint) {
                // 喷涂点使用统一的绿色
                colorArray->InsertNextTuple3(0, 255, 0);  // 纯绿色
            } else {
                // 非喷涂点使用橙色
                colorArray->InsertNextTuple3(255, 165, 0);  // 橙色
            }

            pointIndex++;
        }

        // 关键修改：为每条路径单独创建一个polyLine，并立即添加到cells中
        // 这样不同路径之间就不会连接在一起
        if (pointIds.size() >= 2) {
            vtkSmartPointer<vtkPolyLine> polyLine = vtkSmartPointer<vtkPolyLine>::New();
            polyLine->GetPointIds()->SetNumberOfIds(pointIds.size());
            for (size_t i = 0; i < pointIds.size(); ++i) {
                polyLine->GetPointIds()->SetId(i, pointIds[i]);
            }
            cells->InsertNextCell(polyLine);
        }
    }

    polyData->SetPoints(points);
    polyData->SetLines(cells);
    polyData->GetPointData()->AddArray(pathIdArray);
    polyData->GetPointData()->AddArray(sprayPointArray);
    polyData->GetPointData()->SetNormals(normalArray);
    polyData->GetPointData()->SetScalars(colorArray); // 设置颜色为标量数据，这样VTK会使用这些颜色渲染

    return polyData;
}

// 将切割平面转换为VTK PolyData用于可视化
vtkSmartPointer<vtkPolyData> FaceProcessor::cuttingPlanesToPolyData() const {
    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto cells = vtkSmartPointer<vtkCellArray>::New();

    // 创建一个数组用于存储平面索引
    auto planeIdArray = vtkSmartPointer<vtkDoubleArray>::New();
    planeIdArray->SetName("PlaneIndex");

    // 创建一个颜色数组
    auto colorArray = vtkSmartPointer<vtkUnsignedCharArray>::New();
    colorArray->SetNumberOfComponents(3);
    colorArray->SetName("Colors");

    // 如果没有切割平面，返回空的PolyData
    if (cuttingPlanes.empty()) {
        return polyData;
    }

    // 计算边界盒以确定平面的大小
    Bnd_Box boundingBox;
    BRepBndLib::Add(inputFaces, boundingBox);

    if (boundingBox.IsVoid()) {
        std::cerr << "无法计算形状的包围盒" << std::endl;
        return polyData;
    }

    double xMin, yMin, zMin, xMax, yMax, zMax;
    boundingBox.Get(xMin, yMin, zMin, xMax, yMax, zMax);

    // 为每个切割平面创建一个矩形
    for (size_t i = 0; i < cuttingPlanes.size(); i++) {
        const gp_Pln& plane = cuttingPlanes[i];

        // 获取平面原点和法向量
        gp_Pnt origin = plane.Location();
        gp_Dir normal = plane.Axis().Direction();

        // 创建平面上的两个正交向量
        gp_Dir xDir, yDir;
        if (std::abs(normal.X()) < 0.707 && std::abs(normal.Y()) < 0.707) {
            xDir = gp_Dir(1, 0, 0).Crossed(normal);
        } else {
            xDir = gp_Dir(0, 0, 1).Crossed(normal);
        }
        yDir = normal.Crossed(xDir);

        // 计算包围盒8个顶点在切割面本地坐标系下的投影范围
        double minX = 1e100, maxX = -1e100, minY = 1e100, maxY = -1e100;
        for (int corner = 0; corner < 8; ++corner) {
            double px = (corner & 1) ? xMax : xMin;
            double py = (corner & 2) ? yMax : yMin;
            double pz = (corner & 4) ? zMax : zMin;
            gp_Pnt p(px, py, pz);
            gp_Vec vec(origin, p);
            double projX = vec.Dot(gp_Vec(xDir));
            double projY = vec.Dot(gp_Vec(yDir));
            if (projX < minX) minX = projX;
            if (projX > maxX) maxX = projX;
            if (projY < minY) minY = projY;
            if (projY > maxY) maxY = projY;
        }
        // 稍微放大一点，避免边界重合
        double scale = 1.05;
        minX *= scale; maxX *= scale; minY *= scale; maxY *= scale;

        // 平面的四个角点
        gp_Pnt p1 = origin.Translated(gp_Vec(xDir) * minX + gp_Vec(yDir) * minY);
        gp_Pnt p2 = origin.Translated(gp_Vec(xDir) * maxX + gp_Vec(yDir) * minY);
        gp_Pnt p3 = origin.Translated(gp_Vec(xDir) * maxX + gp_Vec(yDir) * maxY);
        gp_Pnt p4 = origin.Translated(gp_Vec(xDir) * minX + gp_Vec(yDir) * maxY);

        // 添加四个点
        vtkIdType pid1 = points->InsertNextPoint(p1.X(), p1.Y(), p1.Z());
        vtkIdType pid2 = points->InsertNextPoint(p2.X(), p2.Y(), p2.Z());
        vtkIdType pid3 = points->InsertNextPoint(p3.X(), p3.Y(), p3.Z());
        vtkIdType pid4 = points->InsertNextPoint(p4.X(), p4.Y(), p4.Z());

        // 创建两个三角形组成一个矩形
        vtkSmartPointer<vtkTriangle> triangle1 = vtkSmartPointer<vtkTriangle>::New();
        triangle1->GetPointIds()->SetId(0, pid1);
        triangle1->GetPointIds()->SetId(1, pid2);
        triangle1->GetPointIds()->SetId(2, pid3);

        vtkSmartPointer<vtkTriangle> triangle2 = vtkSmartPointer<vtkTriangle>::New();
        triangle2->GetPointIds()->SetId(0, pid1);
        triangle2->GetPointIds()->SetId(1, pid3);
        triangle2->GetPointIds()->SetId(2, pid4);

        cells->InsertNextCell(triangle1);
        cells->InsertNextCell(triangle2);

        // 为所有点添加平面索引和颜色
        for (int j = 0; j < 4; j++) {
            planeIdArray->InsertNextValue(i);

            // 使用HSV颜色空间为每个平面生成不同的颜色
            float hue = (i * 20) % 360; // 每个平面的色调间隔20度
            float saturation = 0.7f;
            float value = 0.7f; // 稍微暗一点，让平面半透明时更易于区分

            // HSV转RGB
            float h = hue / 60.0f;
            int hi = (int)floor(h);
            float f = h - hi;
            float p = value * (1.0f - saturation);
            float q = value * (1.0f - saturation * f);
            float t = value * (1.0f - saturation * (1.0f - f));

            float r, g, b;
            switch (hi) {
                case 0: r = value; g = t; b = p; break;
                case 1: r = q; g = value; b = p; break;
                case 2: r = p; g = value; b = t; break;
                case 3: r = p; g = q; b = value; break;
                case 4: r = t; g = p; b = value; break;
                default: r = value; g = p; b = q; break;
            }

            unsigned char rgb[3] = {
                (unsigned char)(r * 255),
                (unsigned char)(g * 255),
                (unsigned char)(b * 255)
            };
            colorArray->InsertNextTuple3(rgb[0], rgb[1], rgb[2]);
        }
    }

    polyData->SetPoints(points);
    polyData->SetPolys(cells);
    polyData->GetPointData()->AddArray(planeIdArray);
    polyData->GetPointData()->SetScalars(colorArray);

    return polyData;
}

// 按切割平面分组路径
void FaceProcessor::groupPathsByPlane() {
    // 创建平面到路径的映射
    std::map<int, std::vector<int>> planeToPathsMap;

    // 将路径按所属平面分组
    for (size_t i = 0; i < generatedPaths.size(); i++) {
        int planeIndex = generatedPaths[i].planeIndex;
        planeToPathsMap[planeIndex].push_back(i);
    }

    int trajectoryIndex = 0;

    // 为每个平面的路径组创建整合轨迹
    for (auto& planePaths : planeToPathsMap) {
        std::vector<int>& pathIndices = planePaths.second;

        if (pathIndices.empty()) continue;

        // 对当前平面的路径进行排序
        sortPathsInPlane(pathIndices);

        // 连接相邻路径创建整合轨迹
        IntegratedTrajectory trajectory;
        trajectory.trajectoryIndex = trajectoryIndex++;
        trajectory.totalLength = 0.0;

        connectAdjacentPaths(pathIndices, trajectory);

        if (!trajectory.points.empty()) {
            integratedTrajectories.push_back(trajectory);
        }
    }

    std::cout << "整合完成，生成了 " << integratedTrajectories.size() << " 条整合轨迹" << std::endl;
}

// 对平面内的路径进行排序，使相邻路径尽可能接近
void FaceProcessor::sortPathsInPlane(std::vector<int>& pathIndices) {
    if (pathIndices.size() <= 1) return;

    // 使用贪心算法进行路径排序，每次选择距离当前路径最近的未访问路径
    std::vector<int> sortedIndices;
    std::vector<bool> visited(pathIndices.size(), false);

    // 从第一条路径开始
    sortedIndices.push_back(pathIndices[0]);
    visited[0] = true;

    // 依次选择最近的路径
    for (size_t i = 1; i < pathIndices.size(); i++) {
        int currentPathIndex = sortedIndices.back();
        const SprayPath& currentPath = generatedPaths[currentPathIndex];

        double minDistance = std::numeric_limits<double>::max();
        int nearestIndex = -1;

        // 找到距离当前路径最近的未访问路径
        for (size_t j = 0; j < pathIndices.size(); j++) {
            if (visited[j]) continue;

            const SprayPath& candidatePath = generatedPaths[pathIndices[j]];
            double distance = calculatePathDistance(currentPath, candidatePath);

            if (distance < minDistance) {
                minDistance = distance;
                nearestIndex = j;
            }
        }

        if (nearestIndex != -1) {
            sortedIndices.push_back(pathIndices[nearestIndex]);
            visited[nearestIndex] = true;
        }
    }

    // 更新路径索引顺序
    pathIndices = sortedIndices;
}

// 连接相邻路径创建整合轨迹
void FaceProcessor::connectAdjacentPaths(const std::vector<int>& pathIndices, IntegratedTrajectory& trajectory) {
    if (pathIndices.empty()) return;

    trajectory.points.clear();
    trajectory.pathSegments.clear();
    trajectory.totalLength = 0.0;

    for (size_t i = 0; i < pathIndices.size(); i++) {
        int pathIndex = pathIndices[i];
        SprayPath& currentPath = generatedPaths[pathIndex];

        // 记录当前路径段的起始点索引
        trajectory.pathSegments.push_back(trajectory.points.size());

        // 如果不是第一条路径，需要检查是否需要反转方向
        if (i > 0) {
            int prevPathIndex = pathIndices[i - 1];
            const SprayPath& prevPath = generatedPaths[prevPathIndex];

            if (shouldReversePath(prevPath, currentPath)) {
                // 反转当前路径
                std::reverse(currentPath.points.begin(), currentPath.points.end());
            }

            // 创建连接路径
            ConnectionPath connection = createConnectionPath(prevPath, currentPath);
            if (!connection.points.empty()) {
                // 添加连接路径点（标记为非喷涂点）
                for (auto& point : connection.points) {
                    point.isSprayPoint = false;
                    trajectory.points.push_back(point);
                }
                connectionPaths.push_back(connection);
            }
        }

        // 添加当前路径的所有点
        for (const auto& point : currentPath.points) {
            trajectory.points.push_back(point);
        }

        // 标记路径为已连接
        currentPath.isConnected = true;
    }

    // 优化轨迹方向
    optimizeTrajectoryDirection(trajectory);

    // 计算总长度
    for (size_t i = 1; i < trajectory.points.size(); i++) {
        const gp_Pnt& p1 = trajectory.points[i-1].position;
        const gp_Pnt& p2 = trajectory.points[i].position;
        trajectory.totalLength += p1.Distance(p2);
    }
}

// 创建两条路径之间的连接路径
ConnectionPath FaceProcessor::createConnectionPath(const SprayPath& fromPath, const SprayPath& toPath) {
    ConnectionPath connection;
    connection.fromPathIndex = fromPath.pathIndex;
    connection.toPathIndex = toPath.pathIndex;
    connection.isTransition = true;

    if (fromPath.points.empty() || toPath.points.empty()) {
        return connection;
    }

    // 获取起点和终点
    const PathPoint& startPoint = fromPath.points.back();  // 前一条路径的终点
    const PathPoint& endPoint = toPath.points.front();     // 下一条路径的起点

    // 计算连接距离
    double distance = startPoint.position.Distance(endPoint.position);

    // 如果距离很小，不需要连接路径
    if (distance < pathSpacing * 0.1) {
        return connection;
    }

    // 创建简单的直线连接
    int numConnectionPoints = std::max(2, int(distance / (pathSpacing * 0.5)));

    for (int i = 0; i <= numConnectionPoints; i++) {
        double t = double(i) / numConnectionPoints;

        // 线性插值位置
        gp_Pnt pos = startPoint.position.Translated(
            gp_Vec(startPoint.position, endPoint.position).Multiplied(t)
        );

        // 插值法向量
        gp_Vec normalVec = gp_Vec(startPoint.normal.X(), startPoint.normal.Y(), startPoint.normal.Z()).Multiplied(1-t) +
                          gp_Vec(endPoint.normal.X(), endPoint.normal.Y(), endPoint.normal.Z()).Multiplied(t);
        normalVec.Normalize();
        gp_Dir normal(normalVec.X(), normalVec.Y(), normalVec.Z());

        // 连接路径点标记为非喷涂点
        connection.points.push_back(PathPoint(pos, normal, false));
    }

    return connection;
}

// 计算两条路径之间的距离
double FaceProcessor::calculatePathDistance(const SprayPath& path1, const SprayPath& path2) {
    if (path1.points.empty() || path2.points.empty()) {
        return std::numeric_limits<double>::max();
    }

    // 计算路径端点之间的最小距离
    double minDistance = std::numeric_limits<double>::max();

    // 检查path1的两个端点到path2的两个端点的距离
    const gp_Pnt& p1_start = path1.points.front().position;
    const gp_Pnt& p1_end = path1.points.back().position;
    const gp_Pnt& p2_start = path2.points.front().position;
    const gp_Pnt& p2_end = path2.points.back().position;

    minDistance = std::min(minDistance, p1_start.Distance(p2_start));
    minDistance = std::min(minDistance, p1_start.Distance(p2_end));
    minDistance = std::min(minDistance, p1_end.Distance(p2_start));
    minDistance = std::min(minDistance, p1_end.Distance(p2_end));

    return minDistance;
}

// 判断是否需要反转路径方向
bool FaceProcessor::shouldReversePath(const SprayPath& currentPath, const SprayPath& nextPath) {
    if (currentPath.points.empty() || nextPath.points.empty()) {
        return false;
    }

    // 计算当前路径终点到下一路径两端的距离
    const gp_Pnt& currentEnd = currentPath.points.back().position;
    const gp_Pnt& nextStart = nextPath.points.front().position;
    const gp_Pnt& nextEnd = nextPath.points.back().position;

    double distToStart = currentEnd.Distance(nextStart);
    double distToEnd = currentEnd.Distance(nextEnd);

    // 如果到终点的距离更近，则需要反转下一条路径
    return distToEnd < distToStart;
}

// 优化轨迹方向
void FaceProcessor::optimizeTrajectoryDirection(IntegratedTrajectory& trajectory) {
    // 这里可以添加更复杂的优化逻辑
    // 目前保持简单实现
    if (trajectory.points.size() < 2) return;

    // 可以根据需要添加轨迹方向优化算法
    // 例如：最小化总的移动距离、考虑喷涂方向等
}

// 将整合后的轨迹转换为VTK PolyData用于可视化
vtkSmartPointer<vtkPolyData> FaceProcessor::integratedTrajectoriesToPolyData() const {
    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto cells = vtkSmartPointer<vtkCellArray>::New();

    // 创建数组用于存储轨迹信息
    auto trajectoryIdArray = vtkSmartPointer<vtkDoubleArray>::New();
    trajectoryIdArray->SetName("TrajectoryIndex");

    auto sprayPointArray = vtkSmartPointer<vtkDoubleArray>::New();
    sprayPointArray->SetName("IsSprayPoint");

    auto normalArray = vtkSmartPointer<vtkDoubleArray>::New();
    normalArray->SetNumberOfComponents(3);
    normalArray->SetName("Normals");

    auto colorArray = vtkSmartPointer<vtkUnsignedCharArray>::New();
    colorArray->SetNumberOfComponents(3);
    colorArray->SetName("Colors");

    int pointIndex = 0;

    // 遍历每条整合轨迹
    for (const auto& trajectory : integratedTrajectories) {
        std::vector<vtkIdType> pointIds;

        if (trajectory.points.size() < 2) {
            continue;
        }

        // 处理轨迹中的点
        for (size_t i = 0; i < trajectory.points.size(); ++i) {
            const auto& point = trajectory.points[i];
            points->InsertNextPoint(point.position.X(), point.position.Y(), point.position.Z());
            pointIds.push_back(pointIndex);

            // 存储法向量
            double normal[3] = {point.normal.X(), point.normal.Y(), point.normal.Z()};
            normalArray->InsertNextTuple(normal);

            // 存储轨迹索引
            trajectoryIdArray->InsertNextValue(trajectory.trajectoryIndex);

            // 存储是否为喷涂点
            sprayPointArray->InsertNextValue(point.isSprayPoint ? 1.0 : 0.0);

            // 根据是否为喷涂点设置颜色
            if (point.isSprayPoint) {
                // 所有喷涂点使用统一的绿色
                colorArray->InsertNextTuple3(0, 255, 0);  // 纯绿色
            } else {
                // 非喷涂路径（连接/过渡段）使用橙色
                colorArray->InsertNextTuple3(255, 165, 0);  // 橙色
            }

            pointIndex++;
        }

        // 为整条轨迹创建一个连续的polyLine
        if (pointIds.size() >= 2) {
            vtkSmartPointer<vtkPolyLine> polyLine = vtkSmartPointer<vtkPolyLine>::New();
            polyLine->GetPointIds()->SetNumberOfIds(pointIds.size());
            for (size_t i = 0; i < pointIds.size(); ++i) {
                polyLine->GetPointIds()->SetId(i, pointIds[i]);
            }
            cells->InsertNextCell(polyLine);
        }
    }

    polyData->SetPoints(points);
    polyData->SetLines(cells);
    polyData->GetPointData()->AddArray(trajectoryIdArray);
    polyData->GetPointData()->AddArray(sprayPointArray);
    polyData->GetPointData()->SetNormals(normalArray);
    polyData->GetPointData()->SetScalars(colorArray);

    return polyData;
}

// 路径可见性分析 - 只保留最表层轨迹
bool FaceProcessor::analyzePathVisibility() {
    if (generatedPaths.empty()) {
        std::cerr << "没有可用的路径进行可见性分析" << std::endl;
        return false;
    }

    std::cout << "开始表面可见性分析..." << std::endl;

    // 1. 计算所有路径的深度
    calculatePathDepths();

    // 2. 检测遮挡关系
    detectOcclusions();

    // 3. 点级别可见性分析
    analyzePointLevelVisibility();

    // 4. 分割部分遮挡的路径
    segmentPartiallyOccludedPaths();

    // 5. 分类表面层级
    classifySurfaceLayers();

    // 6. 过滤可见路径
    filterVisiblePaths();

    // 7. 更新整合轨迹，只包含可见部分
    updateIntegratedTrajectoriesWithVisibility();

    std::cout << "可见性分析完成，保留了 " << surfaceLayers.size() << " 个表面层级" << std::endl;
    return true;
}

// 计算所有路径的深度
void FaceProcessor::calculatePathDepths() {
    pathVisibility.clear();
    pathVisibility.resize(generatedPaths.size());

    // 使用Z+方向作为深度方向（与面提取方向一致）
    gp_Dir depthDirection(0, 0, 1);  // Z+方向

    for (size_t i = 0; i < generatedPaths.size(); i++) {
        const SprayPath& path = generatedPaths[i];

        if (path.points.empty()) {
            pathVisibility[i].isVisible = false;
            pathVisibility[i].depth = 0.0;
            continue;
        }

        // 计算路径的平均Z坐标作为深度
        double totalZ = 0.0;
        int validPoints = 0;

        for (const auto& point : path.points) {
            totalZ += point.position.Z();
            validPoints++;
        }

        if (validPoints > 0) {
            pathVisibility[i].depth = totalZ / validPoints;  // 平均Z坐标
            pathVisibility[i].isVisible = true;  // 初始假设都可见
            pathVisibility[i].occludingPathIndex = -1;
            pathVisibility[i].occlusionRatio = 0.0;
        } else {
            pathVisibility[i].isVisible = false;
            pathVisibility[i].depth = 0.0;
        }
    }

    std::cout << "计算了 " << generatedPaths.size() << " 条路径的Z方向深度" << std::endl;
}

// 检测遮挡关系
void FaceProcessor::detectOcclusions() {
    const double DEPTH_THRESHOLD = pathSpacing * 0.1;  // 减小深度差阈值，更敏感
    const double OCCLUSION_THRESHOLD = 0.2;  // 降低遮挡比例阈值

    int occludedCount = 0;

    for (size_t i = 0; i < generatedPaths.size(); i++) {
        if (!pathVisibility[i].isVisible) continue;

        double maxOcclusionRatio = 0.0;
        int bestOccluderIndex = -1;

        // 检查是否被其他路径遮挡
        for (size_t j = 0; j < generatedPaths.size(); j++) {
            if (i == j || !pathVisibility[j].isVisible) continue;

            // 在Z+方向上，Z值更大的路径遮挡Z值更小的路径
            // 只有Z坐标更高的路径才能遮挡当前路径
            if (pathVisibility[j].depth <= pathVisibility[i].depth + DEPTH_THRESHOLD) continue;

            // 检查是否存在遮挡
            if (isPathOccluded(i, j)) {
                double occlusionRatio = calculateOcclusionRatio(generatedPaths[i], generatedPaths[j]);

                if (occlusionRatio > maxOcclusionRatio) {
                    maxOcclusionRatio = occlusionRatio;
                    bestOccluderIndex = j;
                }
            }
        }

        // 如果遮挡比例超过阈值，标记为被遮挡
        if (maxOcclusionRatio > OCCLUSION_THRESHOLD) {
            pathVisibility[i].isVisible = false;
            pathVisibility[i].occludingPathIndex = bestOccluderIndex;
            pathVisibility[i].occlusionRatio = maxOcclusionRatio;
            occludedCount++;
        }
    }

    std::cout << "检测到 " << occludedCount << " 条被Z+方向遮挡的路径" << std::endl;

    // 输出深度信息用于调试
    std::cout << "路径深度信息（Z坐标）：" << std::endl;
    for (size_t i = 0; i < std::min(generatedPaths.size(), size_t(10)); i++) {
        std::cout << "路径 " << i << ": Z=" << std::fixed << std::setprecision(2)
                  << pathVisibility[i].depth << ", 可见=" << (pathVisibility[i].isVisible ? "是" : "否");
        if (!pathVisibility[i].isVisible && pathVisibility[i].occludingPathIndex >= 0) {
            std::cout << ", 被路径 " << pathVisibility[i].occludingPathIndex << " 遮挡";
        }
        std::cout << std::endl;
    }
    if (generatedPaths.size() > 10) {
        std::cout << "... (显示前10条路径)" << std::endl;
    }
}

// 点级别可见性分析
void FaceProcessor::analyzePointLevelVisibility() {
    std::cout << "开始点级别可见性分析..." << std::endl;

    for (size_t i = 0; i < generatedPaths.size(); i++) {
        const SprayPath& path = generatedPaths[i];
        VisibilityInfo& visibility = pathVisibility[i];

        // 初始化点级别可见性
        visibility.pointVisibility.clear();
        visibility.pointVisibility.resize(path.points.size(), true);

        // 检查每个点是否被遮挡
        for (size_t pointIdx = 0; pointIdx < path.points.size(); pointIdx++) {
            const PathPoint& point = path.points[pointIdx];
            bool pointVisible = true;

            // 检查是否被其他路径遮挡
            for (size_t j = 0; j < generatedPaths.size(); j++) {
                if (i == j) continue;

                // 在Z+方向上，只有Z值更高的路径才能遮挡当前点
                if (pathVisibility[j].depth <= pathVisibility[i].depth + pathSpacing * 0.1) continue;

                if (isPointOccluded(point, i, j)) {
                    pointVisible = false;
                    break;
                }
            }

            visibility.pointVisibility[pointIdx] = pointVisible;
        }

        // 找到可见段
        visibility.visibleSegments = findVisibleSegments(visibility.pointVisibility);

        // 重新计算路径的整体可见性
        int visiblePoints = 0;
        for (bool visible : visibility.pointVisibility) {
            if (visible) visiblePoints++;
        }

        double visibilityRatio = double(visiblePoints) / path.points.size();
        // 只要有可见点就保留路径，后续会分割
        visibility.isVisible = (visiblePoints > 0);
        visibility.occlusionRatio = 1.0 - visibilityRatio;
    }

    std::cout << "点级别可见性分析完成" << std::endl;
}

// 检查单个点是否被遮挡
bool FaceProcessor::isPointOccluded(const PathPoint& point, int pathIndex, int candidateOccluderIndex) {
    const SprayPath& occluderPath = generatedPaths[candidateOccluderIndex];

    if (occluderPath.points.empty()) {
        return false;
    }

    // 检查两条路径是否在同一个切割平面上
    if (generatedPaths[pathIndex].planeIndex == occluderPath.planeIndex) {
        return false;
    }

    // 检查Z坐标：只有Z值更高的路径才能遮挡当前点
    double pointZ = point.position.Z();

    // 检查遮挡路径中是否有点在当前点的上方
    bool hasHigherPoint = false;
    for (const auto& occluderPoint : occluderPath.points) {
        if (occluderPoint.position.Z() > pointZ + pathSpacing * 0.05) {
            hasHigherPoint = true;
            break;
        }
    }

    // 如果遮挡路径没有更高的点，则不能遮挡
    if (!hasHigherPoint) {
        return false;
    }

    // 计算点在XY平面的投影
    double pointX = point.position.X();
    double pointY = point.position.Y();

    // 检查点是否在遮挡路径的XY投影范围内
    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();

    for (const auto& occluderPoint : occluderPath.points) {
        minX = std::min(minX, occluderPoint.position.X());
        maxX = std::max(maxX, occluderPoint.position.X());
        minY = std::min(minY, occluderPoint.position.Y());
        maxY = std::max(maxY, occluderPoint.position.Y());
    }

    // 减小容差，使检测更精确
    double tolerance = pathSpacing * 0.1;
    minX -= tolerance;
    maxX += tolerance;
    minY -= tolerance;
    maxY += tolerance;

    // 检查点是否在扩展的XY边界框内
    return (pointX >= minX && pointX <= maxX && pointY >= minY && pointY <= maxY);
}

// 找到可见段
std::vector<std::pair<int, int>> FaceProcessor::findVisibleSegments(const std::vector<bool>& pointVisibility) {
    std::vector<std::pair<int, int>> segments;

    if (pointVisibility.empty()) {
        return segments;
    }

    int segmentStart = -1;

    for (size_t i = 0; i < pointVisibility.size(); i++) {
        if (pointVisibility[i]) {
            // 可见点
            if (segmentStart == -1) {
                segmentStart = i; // 开始新段
            }
        } else {
            // 不可见点
            if (segmentStart != -1) {
                // 结束当前段
                segments.push_back({segmentStart, i - 1});
                segmentStart = -1;
            }
        }
    }

    // 处理最后一段
    if (segmentStart != -1) {
        segments.push_back({segmentStart, pointVisibility.size() - 1});
    }

    // 合并相邻的短段（可选优化）
    std::vector<std::pair<int, int>> mergedSegments;
    for (const auto& segment : segments) {
        if (mergedSegments.empty()) {
            mergedSegments.push_back(segment);
        } else {
            auto& lastSegment = mergedSegments.back();
            // 如果两段之间的间隙很小，考虑合并
            if (segment.first - lastSegment.second <= 2) {
                lastSegment.second = segment.second; // 合并段
            } else {
                mergedSegments.push_back(segment);
            }
        }
    }

    return mergedSegments;
}

// 分割部分遮挡的路径
void FaceProcessor::segmentPartiallyOccludedPaths() {
    std::cout << "开始分割部分遮挡的路径..." << std::endl;

    std::vector<SprayPath> newPaths;
    std::vector<VisibilityInfo> newVisibility;

    for (size_t i = 0; i < generatedPaths.size(); i++) {
        const SprayPath& originalPath = generatedPaths[i];
        const VisibilityInfo& visibility = pathVisibility[i];

        if (visibility.visibleSegments.empty()) {
            // 完全不可见，跳过
            continue;
        }

        if (visibility.visibleSegments.size() == 1 &&
            visibility.visibleSegments[0].first == 0 &&
            visibility.visibleSegments[0].second == originalPath.points.size() - 1) {
            // 完全可见，保持原样
            newPaths.push_back(originalPath);
            newVisibility.push_back(visibility);
            continue;
        }

        // 部分可见，需要分割
        for (const auto& segment : visibility.visibleSegments) {
            int startIdx = segment.first;
            int endIdx = segment.second;

            // 放宽段长度要求，保留更多可见部分
            if (endIdx - startIdx < 1) {
                continue; // 至少需要2个点（起点和终点）
            }

            // 创建新的路径段
            SprayPath newPath;
            newPath.pathIndex = newPaths.size();
            newPath.planeIndex = originalPath.planeIndex;
            newPath.width = originalPath.width;
            newPath.isConnected = false;

            // 复制可见段的点
            for (int j = startIdx; j <= endIdx; j++) {
                newPath.points.push_back(originalPath.points[j]);
            }

            // 检查分割后的路径段长度
            double segmentLength = calculatePathLength(newPath);

            // 只保留长度足够的路径段
            if (segmentLength >= minPathLength) {
                // 创建对应的可见性信息
                VisibilityInfo newVis;
                newVis.isVisible = true;
                newVis.depth = visibility.depth;
                newVis.occludingPathIndex = -1;
                newVis.occlusionRatio = 0.0;
                newVis.pointVisibility.resize(newPath.points.size(), true);
                newVis.visibleSegments.push_back({0, newPath.points.size() - 1});

                newPaths.push_back(newPath);
                newVisibility.push_back(newVis);
            }
            // 如果路径段太短，直接丢弃（不添加到newPaths中）
        }
    }

    // 替换原始路径
    generatedPaths = newPaths;
    pathVisibility = newVisibility;

    // 重新分配路径索引
    for (size_t i = 0; i < generatedPaths.size(); i++) {
        generatedPaths[i].pathIndex = i;
    }

    std::cout << "路径分割完成，生成了 " << generatedPaths.size() << " 个路径段" << std::endl;

    // 统计分割效果
    int maxPlaneIndex = -1;
    for (const auto& path : newPaths) {
        if (path.planeIndex > maxPlaneIndex) {
            maxPlaneIndex = path.planeIndex;
        }
    }
    int originalPathCount = maxPlaneIndex >= 0 ? maxPlaneIndex + 1 : 0;

    std::cout << "分割统计：保留了 " << newPaths.size() << " 个可见段";
    if (originalPathCount > 0) {
        std::cout << "，平均每个原始路径分割为 "
                  << std::fixed << std::setprecision(1)
                  << double(newPaths.size()) / originalPathCount << " 段";
    }
    std::cout << std::endl;

    // 验证分割结果
    validateSegmentationResults();
}

// 判断路径是否被遮挡
bool FaceProcessor::isPathOccluded(int pathIndex, int candidateOccluderIndex) {
    const SprayPath& path = generatedPaths[pathIndex];
    const SprayPath& occluder = generatedPaths[candidateOccluderIndex];

    if (path.points.empty() || occluder.points.empty()) {
        return false;
    }

    // 检查两条路径是否在同一个切割平面上（同一平面的路径不会相互遮挡）
    if (path.planeIndex == occluder.planeIndex) {
        return false;
    }

    // 在XY平面上进行投影比较（Z+方向遮挡检测）
    // 计算路径在XY平面的投影边界框

    // 计算被检测路径的XY投影边界框
    double path_minX = std::numeric_limits<double>::max();
    double path_maxX = std::numeric_limits<double>::lowest();
    double path_minY = std::numeric_limits<double>::max();
    double path_maxY = std::numeric_limits<double>::lowest();

    for (const auto& point : path.points) {
        path_minX = std::min(path_minX, point.position.X());
        path_maxX = std::max(path_maxX, point.position.X());
        path_minY = std::min(path_minY, point.position.Y());
        path_maxY = std::max(path_maxY, point.position.Y());
    }

    // 计算遮挡路径的XY投影边界框
    double occluder_minX = std::numeric_limits<double>::max();
    double occluder_maxX = std::numeric_limits<double>::lowest();
    double occluder_minY = std::numeric_limits<double>::max();
    double occluder_maxY = std::numeric_limits<double>::lowest();

    for (const auto& point : occluder.points) {
        occluder_minX = std::min(occluder_minX, point.position.X());
        occluder_maxX = std::max(occluder_maxX, point.position.X());
        occluder_minY = std::min(occluder_minY, point.position.Y());
        occluder_maxY = std::max(occluder_maxY, point.position.Y());
    }

    // 检查XY平面投影是否重叠
    bool xOverlap = (path_minX < occluder_maxX) && (path_maxX > occluder_minX);
    bool yOverlap = (path_minY < occluder_maxY) && (path_maxY > occluder_minY);

    return xOverlap && yOverlap;
}

// 计算遮挡比例
double FaceProcessor::calculateOcclusionRatio(const SprayPath& occludedPath, const SprayPath& occluderPath) {
    if (occludedPath.points.empty() || occluderPath.points.empty()) {
        return 0.0;
    }

    // 计算两条路径的重叠区域面积比例
    // 简化计算：使用边界框重叠面积

    // 计算被遮挡路径的边界框
    double occ_minX = std::numeric_limits<double>::max();
    double occ_maxX = std::numeric_limits<double>::lowest();
    double occ_minY = std::numeric_limits<double>::max();
    double occ_maxY = std::numeric_limits<double>::lowest();

    for (const auto& point : occludedPath.points) {
        occ_minX = std::min(occ_minX, point.position.X());
        occ_maxX = std::max(occ_maxX, point.position.X());
        occ_minY = std::min(occ_minY, point.position.Y());
        occ_maxY = std::max(occ_maxY, point.position.Y());
    }

    // 计算遮挡路径的边界框
    double occluder_minX = std::numeric_limits<double>::max();
    double occluder_maxX = std::numeric_limits<double>::lowest();
    double occluder_minY = std::numeric_limits<double>::max();
    double occluder_maxY = std::numeric_limits<double>::lowest();

    for (const auto& point : occluderPath.points) {
        occluder_minX = std::min(occluder_minX, point.position.X());
        occluder_maxX = std::max(occluder_maxX, point.position.X());
        occluder_minY = std::min(occluder_minY, point.position.Y());
        occluder_maxY = std::max(occluder_maxY, point.position.Y());
    }

    // 计算重叠区域
    double overlap_minX = std::max(occ_minX, occluder_minX);
    double overlap_maxX = std::min(occ_maxX, occluder_maxX);
    double overlap_minY = std::max(occ_minY, occluder_minY);
    double overlap_maxY = std::min(occ_maxY, occluder_maxY);

    if (overlap_minX >= overlap_maxX || overlap_minY >= overlap_maxY) {
        return 0.0;  // 没有重叠
    }

    // 计算面积
    double overlapArea = (overlap_maxX - overlap_minX) * (overlap_maxY - overlap_minY);
    double occludedArea = (occ_maxX - occ_minX) * (occ_maxY - occ_minY);

    if (occludedArea <= 0.0) {
        return 0.0;
    }

    return overlapArea / occludedArea;
}

// 分类表面层级
void FaceProcessor::classifySurfaceLayers() {
    surfaceLayers.clear();

    // 收集所有可见路径及其深度
    std::vector<std::pair<int, double>> visiblePaths;  // <路径索引, 深度>

    for (size_t i = 0; i < generatedPaths.size(); i++) {
        if (pathVisibility[i].isVisible) {
            visiblePaths.push_back({i, pathVisibility[i].depth});
        }
    }

    if (visiblePaths.empty()) {
        std::cout << "没有可见路径用于分层" << std::endl;
        return;
    }

    // 按深度排序（深度大的在前，即更靠近喷涂源）
    std::sort(visiblePaths.begin(), visiblePaths.end(),
              [](const std::pair<int, double>& a, const std::pair<int, double>& b) {
                  return a.second > b.second;
              });

    // 根据深度差异分层
    const double LAYER_THRESHOLD = pathSpacing * 0.8;  // 层级分离阈值

    int currentLayerIndex = 0;
    SurfaceLayer currentLayer;
    currentLayer.layerIndex = currentLayerIndex;
    currentLayer.pathIndices.clear();

    double currentLayerDepth = visiblePaths[0].second;

    for (const auto& pathInfo : visiblePaths) {
        int pathIndex = pathInfo.first;
        double depth = pathInfo.second;

        // 如果深度差异太大，创建新层级
        if (std::abs(depth - currentLayerDepth) > LAYER_THRESHOLD) {
            // 完成当前层级
            if (!currentLayer.pathIndices.empty()) {
                currentLayer.averageDepth = currentLayerDepth;
                surfaceLayers.push_back(currentLayer);
            }

            // 开始新层级
            currentLayerIndex++;
            currentLayer.layerIndex = currentLayerIndex;
            currentLayer.pathIndices.clear();
            currentLayerDepth = depth;
        }

        currentLayer.pathIndices.push_back(pathIndex);
    }

    // 添加最后一个层级
    if (!currentLayer.pathIndices.empty()) {
        currentLayer.averageDepth = currentLayerDepth;
        surfaceLayers.push_back(currentLayer);
    }

    std::cout << "分类为 " << surfaceLayers.size() << " 个表面层级" << std::endl;
    for (size_t i = 0; i < surfaceLayers.size(); i++) {
        std::cout << "层级 " << i << ": " << surfaceLayers[i].pathIndices.size()
                  << " 条路径，平均深度 " << surfaceLayers[i].averageDepth << std::endl;
    }
}

// 过滤可见路径
void FaceProcessor::filterVisiblePaths() {
    int originalCount = 0;
    int visibleCount = 0;
    int segmentCount = 0;

    for (size_t i = 0; i < generatedPaths.size(); i++) {
        originalCount++;
        if (pathVisibility[i].isVisible) {
            visibleCount++;

            // 检查是否为分割后的段
            if (pathVisibility[i].visibleSegments.size() > 0) {
                segmentCount++;
            }
        } else {
            // 将不可见路径的点标记为非喷涂点
            for (auto& point : generatedPaths[i].points) {
                point.isSprayPoint = false;
            }
        }
    }

    std::cout << "过滤结果: " << visibleCount << "/" << originalCount
              << " 条路径段可见，其中 " << segmentCount << " 条为分割后的可见段" << std::endl;
}

// 更新整合轨迹，只包含可见部分
void FaceProcessor::updateIntegratedTrajectoriesWithVisibility() {
    // 清空现有的整合轨迹
    integratedTrajectories.clear();

    // 只对最表层（第0层）的路径进行整合
    if (surfaceLayers.empty()) {
        std::cout << "没有表面层级，无法更新整合轨迹" << std::endl;
        return;
    }

    // 只处理最表层
    const SurfaceLayer& topLayer = surfaceLayers[0];
    std::vector<int> visiblePathIndices = topLayer.pathIndices;

    if (visiblePathIndices.empty()) {
        std::cout << "最表层没有可见路径" << std::endl;
        return;
    }

    std::cout << "为最表层的 " << visiblePathIndices.size() << " 条路径创建整合轨迹" << std::endl;

    // 按切割平面分组可见路径
    std::map<int, std::vector<int>> planeToVisiblePathsMap;

    for (int pathIndex : visiblePathIndices) {
        int planeIndex = generatedPaths[pathIndex].planeIndex;
        planeToVisiblePathsMap[planeIndex].push_back(pathIndex);
    }

    int trajectoryIndex = 0;

    // 为每个平面的可见路径组创建整合轨迹
    for (auto& planePaths : planeToVisiblePathsMap) {
        std::vector<int>& pathIndices = planePaths.second;

        if (pathIndices.empty()) continue;

        // 对当前平面的路径进行排序
        sortPathsInPlane(pathIndices);

        // 连接相邻路径创建整合轨迹
        IntegratedTrajectory trajectory;
        trajectory.trajectoryIndex = trajectoryIndex++;
        trajectory.totalLength = 0.0;

        connectAdjacentPaths(pathIndices, trajectory);

        if (!trajectory.points.empty()) {
            integratedTrajectories.push_back(trajectory);
        }
    }

    std::cout << "创建了 " << integratedTrajectories.size() << " 条表层整合轨迹" << std::endl;
}

// 验证分割结果
void FaceProcessor::validateSegmentationResults() {
    std::cout << "\n=== 分割结果验证 ===" << std::endl;

    int totalVisibleSegments = 0;
    int totalPoints = 0;
    int visiblePoints = 0;
    double totalLength = 0.0;
    double minLength = std::numeric_limits<double>::max();
    double maxLength = 0.0;

    std::map<int, int> planeSegmentCount;

    for (size_t i = 0; i < generatedPaths.size(); i++) {
        const SprayPath& path = generatedPaths[i];
        const VisibilityInfo& visibility = pathVisibility[i];

        if (visibility.isVisible) {
            totalVisibleSegments++;
            planeSegmentCount[path.planeIndex]++;

            // 计算路径长度统计
            double pathLength = calculatePathLength(path);
            totalLength += pathLength;
            minLength = std::min(minLength, pathLength);
            maxLength = std::max(maxLength, pathLength);

            for (const auto& point : path.points) {
                totalPoints++;
                if (point.isSprayPoint) {
                    visiblePoints++;
                }
            }
        }
    }

    std::cout << "可见段统计：" << std::endl;
    std::cout << "- 总可见段数：" << totalVisibleSegments << std::endl;
    std::cout << "- 总点数：" << totalPoints << std::endl;
    std::cout << "- 喷涂点数：" << visiblePoints << std::endl;

    if (totalPoints > 0) {
        double sprayRatio = double(visiblePoints) / totalPoints * 100.0;
        std::cout << "- 喷涂点比例：" << std::fixed << std::setprecision(1) << sprayRatio << "%" << std::endl;
    }

    // 路径长度统计
    if (totalVisibleSegments > 0) {
        double avgLength = totalLength / totalVisibleSegments;
        std::cout << "路径长度统计：" << std::endl;
        std::cout << "- 总长度：" << std::fixed << std::setprecision(1) << totalLength << "mm" << std::endl;
        std::cout << "- 平均长度：" << std::fixed << std::setprecision(1) << avgLength << "mm" << std::endl;
        std::cout << "- 最短路径：" << std::fixed << std::setprecision(1) << minLength << "mm" << std::endl;
        std::cout << "- 最长路径：" << std::fixed << std::setprecision(1) << maxLength << "mm" << std::endl;
        std::cout << "- 最小长度阈值：" << std::fixed << std::setprecision(1) << minPathLength << "mm" << std::endl;
    }

    std::cout << "各切割平面的可见段数：" << std::endl;
    for (const auto& pair : planeSegmentCount) {
        std::cout << "- 平面 " << pair.first << "：" << pair.second << " 段" << std::endl;
    }

    // 检查是否有过短的段
    int shortSegments = 0;
    for (const auto& path : generatedPaths) {
        if (path.points.size() < 3) {
            shortSegments++;
        }
    }

    if (shortSegments > 0) {
        std::cout << "警告：发现 " << shortSegments << " 个较短的路径段（少于3个点）" << std::endl;
    }

    std::cout << "=== 验证完成 ===" << std::endl;
}


