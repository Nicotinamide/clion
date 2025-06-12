#pragma once

#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopTools_ListOfShape.hxx>
#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <vector>

// 路径点数据结构
struct PathPoint {
    gp_Pnt position;     // 点的位置
    gp_Dir normal;       // 点的法向量
    bool isSprayPoint;   // 是否为喷涂点

    PathPoint(const gp_Pnt& pos, const gp_Dir& norm, bool isSpray = true)
        : position(pos), normal(norm), isSprayPoint(isSpray) {}
};

// 路径数据结构
struct SprayPath {
    std::vector<PathPoint> points;  // 路径上的点
    double width;                   // 路径宽度
    int pathIndex;                  // 路径索引
    int planeIndex;                 // 所属切割平面索引
    bool isConnected;               // 是否已连接到其他路径
};

// 连接路径数据结构（用于路径间的连接）
struct ConnectionPath {
    std::vector<PathPoint> points;  // 连接路径上的点
    int fromPathIndex;              // 起始路径索引
    int toPathIndex;                // 目标路径索引
    bool isTransition;              // 是否为过渡路径（非喷涂）
};

// 整合后的喷涂轨迹
struct IntegratedTrajectory {
    std::vector<PathPoint> points;  // 整合后的所有点
    std::vector<int> pathSegments;  // 路径段分界点索引
    double totalLength;             // 总长度
    int trajectoryIndex;            // 轨迹索引
};

// 可见性分析结果
struct VisibilityInfo {
    bool isVisible;                 // 是否可见
    double depth;                   // 深度值（沿喷涂方向的距离）
    int occludingPathIndex;         // 遮挡路径的索引（-1表示无遮挡）
    double occlusionRatio;          // 遮挡比例（0.0-1.0）
    std::vector<bool> pointVisibility; // 每个点的可见性
    std::vector<std::pair<int, int>> visibleSegments; // 可见段的起止索引
};

// 表面层级信息
struct SurfaceLayer {
    std::vector<int> pathIndices;   // 该层包含的路径索引
    double averageDepth;            // 平均深度
    int layerIndex;                 // 层级索引（0为最表层）
};

class FaceProcessor {
public:
    FaceProcessor();
    ~FaceProcessor();

    // 设置要处理的形状
    void setShape(const TopoDS_Shape& shape);

    // 设置切割参数
    void setCuttingParameters(gp_Dir cutdirection,double pathSpacing, double offsetDistance = 5.0,
                              double pointDensity = 1.0);

    // 设置最小路径长度
    void setMinPathLength(double minLength);

    // 自动检测并调整单位
    void autoDetectAndAdjustUnits();

    // 生成切割平面
    bool generateCuttingPlanes();

    // 生成路径
    bool generatePaths();

    // 整合轨迹 - 将多条分散的路径整合为连续的喷涂轨迹
    bool integrateTrajectories();

    // 表面可见性分析 - 只保留最表层轨迹
    bool analyzeSurfaceVisibility();

    // 获取生成的路径
    const std::vector<SprayPath>& getPaths() const;

    // 获取整合后的轨迹
    const std::vector<IntegratedTrajectory>& getIntegratedTrajectories() const;

    // 获取表面层级信息
    const std::vector<SurfaceLayer>& getSurfaceLayers() const;

    // 将路径转换为VTK PolyData用于可视化
    vtkSmartPointer<vtkPolyData> pathsToPolyData(bool onlySprayPaths = true) const;

    // 将整合后的轨迹转换为VTK PolyData用于可视化
    vtkSmartPointer<vtkPolyData> integratedTrajectoriesToPolyData() const;

    // 将切割平面转换为VTK PolyData用于可视化
    vtkSmartPointer<vtkPolyData> cuttingPlanesToPolyData() const;

    // 清除所有路径
    void clearPaths();

    // 计算路径长度（公共接口，用于测试和调试）
    double calculatePathLength(const SprayPath& path) const;

    // 单位检测和调整工具
    void detectAndSetOptimalThreshold();
    std::string detectUnits() const;
    void printPathLengthStatistics() const;

private:
    TopoDS_Shape inputFaces;         // 输入形状
    TopoDS_Shape processedShape;     // 处理后的形状

    double pathSpacing;              // 路径间距
    double offsetDistance;           // 路径偏移距离
    double pointDensity;             // 路径点密度（每单位长度的点数）
    double minPathLength;            // 最小路径长度（mm）
    gp_Dir faceDirection;             // 表面法向量方向

    std::vector<gp_Pln> cuttingPlanes;  // 切割平面
    std::vector<SprayPath> generatedPaths; // 生成的路径
    std::vector<ConnectionPath> connectionPaths; // 连接路径
    std::vector<IntegratedTrajectory> integratedTrajectories; // 整合后的轨迹
    std::vector<VisibilityInfo> pathVisibility; // 路径可见性信息
    std::vector<SurfaceLayer> surfaceLayers; // 表面层级信息

    // 获取面的包围盒
    bool getFaceBoundingBox(const TopoDS_Face& face, double& xMin, double& yMin, double& zMin,
                          double& xMax, double& yMax, double& zMax) const;

    // 获取面的最长边方向
    gp_Dir getLongestEdgeDirection(const TopoDS_Face& face) const;

    // 生成切割平面
    void createCuttingPlanes(const TopoDS_Face& face, double spacing);

    // 计算面与切割平面的交线
    bool computeIntersectionCurves(const TopoDS_Face& face, const gp_Pln& plane,
                                  std::vector<PathPoint>& intersectionPoints);

    // 从交线创建路径
    void createPathFromIntersection(const std::vector<PathPoint>& intersectionPoints,
                                  double offsetDistance, SprayPath& path);

    // 轨迹整合相关方法
    void groupPathsByPlane();
    void sortPathsInPlane(std::vector<int>& pathIndices);
    void connectAdjacentPaths(const std::vector<int>& pathIndices, IntegratedTrajectory& trajectory);
    ConnectionPath createConnectionPath(const SprayPath& fromPath, const SprayPath& toPath);
    double calculatePathDistance(const SprayPath& path1, const SprayPath& path2);
    bool shouldReversePath(const SprayPath& currentPath, const SprayPath& nextPath);
    void optimizeTrajectoryDirection(IntegratedTrajectory& trajectory);

    // 表面可见性分析相关方法
    void calculatePathDepths();
    void detectOcclusions();
    void classifySurfaceLayers();
    bool isPathOccluded(int pathIndex, int candidateOccluderIndex);
    double calculateOcclusionRatio(const SprayPath& occludedPath, const SprayPath& occluderPath);
    void analyzePointLevelVisibility();
    void segmentPartiallyOccludedPaths();
    void filterVisiblePaths();
    void updateIntegratedTrajectoriesWithVisibility();

    // 点级别可见性分析
    bool isPointOccluded(const PathPoint& point, int pathIndex, int candidateOccluderIndex);
    std::vector<std::pair<int, int>> findVisibleSegments(const std::vector<bool>& pointVisibility);
    void splitPathByVisibility(int pathIndex);
    void validateSegmentationResults();
};
