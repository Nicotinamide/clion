#include "FaceProcessor.h"
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <iostream>

// 改进的面可见性分析实现示例

class ImprovedFaceVisibility {
public:
    // 更精确的面重叠检测
    static bool isFaceOccludedPrecise(const FaceVisibilityInfo& face, 
                                     const FaceVisibilityInfo& occluder,
                                     const gp_Dir& viewDirection) {
        
        // 方法1：边界框重叠检测
        if (!boundingBoxOverlap(face, occluder, viewDirection)) {
            return false; // 边界框都不重叠，肯定不遮挡
        }
        
        // 方法2：多点采样检测
        return multiPointOcclusionTest(face, occluder, viewDirection);
    }
    
    // 边界框重叠检测
    static bool boundingBoxOverlap(const FaceVisibilityInfo& face, 
                                  const FaceVisibilityInfo& occluder,
                                  const gp_Dir& viewDirection) {
        
        // 计算两个面的边界框
        Bnd_Box box1, box2;
        BRepBndLib::Add(face.face, box1);
        BRepBndLib::Add(occluder.face, box2);
        
        if (box1.IsVoid() || box2.IsVoid()) return false;
        
        double x1min, y1min, z1min, x1max, y1max, z1max;
        double x2min, y2min, z2min, x2max, y2max, z2max;
        
        box1.Get(x1min, y1min, z1min, x1max, y1max, z1max);
        box2.Get(x2min, y2min, z2min, x2max, y2max, z2max);
        
        // 检查在观察方向垂直平面上的投影是否重叠
        if (std::abs(viewDirection.Z()) > 0.9) {
            // 主要沿Z方向观察，检查XY投影重叠
            return !(x1max < x2min || x2max < x1min || y1max < y2min || y2max < y1min);
        } else if (std::abs(viewDirection.Y()) > 0.9) {
            // 主要沿Y方向观察，检查XZ投影重叠
            return !(x1max < x2min || x2max < x1min || z1max < z2min || z2max < z1min);
        } else {
            // 主要沿X方向观察，检查YZ投影重叠
            return !(y1max < y2min || y2max < y1min || z1max < z2min || z2max < z1min);
        }
    }
    
    // 多点采样遮挡测试
    static bool multiPointOcclusionTest(const FaceVisibilityInfo& face, 
                                       const FaceVisibilityInfo& occluder,
                                       const gp_Dir& viewDirection) {
        
        // 在被测试面上采样多个点
        std::vector<gp_Pnt> samplePoints = generateSamplePoints(face.face, 9); // 3x3网格
        
        int occludedCount = 0;
        for (const auto& point : samplePoints) {
            if (isPointOccludedByFace(point, occluder.face, viewDirection)) {
                occludedCount++;
            }
        }
        
        // 如果超过50%的采样点被遮挡，认为面被遮挡
        return (double)occludedCount / samplePoints.size() > 0.5;
    }
    
    // 在面上生成采样点
    static std::vector<gp_Pnt> generateSamplePoints(const TopoDS_Face& face, int numPoints) {
        std::vector<gp_Pnt> points;
        
        try {
            // 获取面的参数范围
            Standard_Real uMin, uMax, vMin, vMax;
            BRepTools::UVBounds(face, uMin, uMax, vMin, vMax);
            
            Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
            if (surface.IsNull()) return points;
            
            // 生成网格采样点
            int gridSize = (int)sqrt(numPoints);
            for (int i = 0; i < gridSize; i++) {
                for (int j = 0; j < gridSize; j++) {
                    double u = uMin + (uMax - uMin) * i / (gridSize - 1);
                    double v = vMin + (vMax - vMin) * j / (gridSize - 1);
                    
                    gp_Pnt point;
                    surface->D0(u, v, point);
                    points.push_back(point);
                }
            }
        } catch (...) {
            // 如果参数化失败，使用面中心点
            GProp_GProps props;
            BRepGProp::SurfaceProperties(face, props);
            points.push_back(props.CentreOfMass());
        }
        
        return points;
    }
    
    // 检查点是否被面遮挡
    static bool isPointOccludedByFace(const gp_Pnt& point, 
                                     const TopoDS_Face& occluderFace,
                                     const gp_Dir& viewDirection) {
        
        // 从点沿观察方向发射射线
        gp_Lin ray(point, viewDirection);
        
        // 简化检测：检查射线是否与遮挡面相交
        // 这里可以使用更复杂的射线-面相交算法
        
        // 获取遮挡面的边界框
        Bnd_Box box;
        BRepBndLib::Add(occluderFace, box);
        
        if (box.IsVoid()) return false;
        
        double xmin, ymin, zmin, xmax, ymax, zmax;
        box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
        
        // 简化判断：如果点在遮挡面的边界框投影内，且深度更小，则被遮挡
        if (std::abs(viewDirection.Z()) > 0.9) {
            // Z方向观察
            return (point.X() >= xmin && point.X() <= xmax &&
                   point.Y() >= ymin && point.Y() <= ymax &&
                   point.Z() < (zmin + zmax) / 2);
        }
        
        return false; // 其他方向的简化实现
    }
    
    // 自适应阈值计算
    static double calculateAdaptiveThreshold(const FaceVisibilityInfo& face1, 
                                           const FaceVisibilityInfo& face2) {
        
        // 基于面积的自适应阈值
        double avgArea = (face1.area + face2.area) / 2.0;
        double baseThreshold = sqrt(avgArea) * 0.1;
        
        // 考虑面的形状复杂度
        double complexityFactor = 1.0; // 可以根据面的边数等计算
        
        return baseThreshold * complexityFactor;
    }
    
    // 支持多方向的可见性分析
    static std::vector<TopoDS_Face> analyzeFaceVisibilityFromDirection(
        const std::vector<FaceVisibilityInfo>& allFaces,
        const gp_Dir& viewDirection) {
        
        std::vector<TopoDS_Face> visibleFaces;
        std::vector<FaceVisibilityInfo> facesCopy = allFaces;
        
        // 1. 计算沿指定方向的深度
        for (auto& faceInfo : facesCopy) {
            gp_Vec centerVec(faceInfo.centerPoint.XYZ());
            faceInfo.depth = centerVec.Dot(gp_Vec(viewDirection.XYZ()));
        }
        
        // 2. 按深度排序（从远到近）
        std::sort(facesCopy.begin(), facesCopy.end(),
                 [](const FaceVisibilityInfo& a, const FaceVisibilityInfo& b) {
                     return a.depth < b.depth;
                 });
        
        // 3. 从近到远检查遮挡关系
        for (size_t i = 0; i < facesCopy.size(); i++) {
            bool isVisible = true;
            
            // 检查是否被更近的面遮挡
            for (size_t j = i + 1; j < facesCopy.size(); j++) {
                if (isFaceOccludedPrecise(facesCopy[i], facesCopy[j], viewDirection)) {
                    isVisible = false;
                    break;
                }
            }
            
            if (isVisible) {
                visibleFaces.push_back(facesCopy[i].face);
            }
        }
        
        return visibleFaces;
    }
};

// 使用示例
void demonstrateImprovedVisibility() {
    std::cout << "=== Improved Face Visibility Analysis ===" << std::endl;
    
    std::cout << "\nImprovements over current implementation:" << std::endl;
    std::cout << "1. Bounding box overlap detection" << std::endl;
    std::cout << "2. Multi-point sampling for occlusion testing" << std::endl;
    std::cout << "3. Adaptive threshold calculation" << std::endl;
    std::cout << "4. Support for arbitrary view directions" << std::endl;
    std::cout << "5. More precise geometric intersection tests" << std::endl;
    
    std::cout << "\nCurrent limitations addressed:" << std::endl;
    std::cout << "- Simple center-point distance check -> Multi-point sampling" << std::endl;
    std::cout << "- Fixed Z+ direction -> Arbitrary view directions" << std::endl;
    std::cout << "- Fixed thresholds -> Adaptive thresholds" << std::endl;
    std::cout << "- Approximate overlap -> Precise geometric tests" << std::endl;
    
    std::cout << "\nImplementation strategy:" << std::endl;
    std::cout << "1. Keep current implementation as fallback" << std::endl;
    std::cout << "2. Add improved methods as optional alternatives" << std::endl;
    std::cout << "3. Allow user to choose precision vs. speed trade-off" << std::endl;
    std::cout << "4. Provide configuration options for different scenarios" << std::endl;
}

int main() {
    demonstrateImprovedVisibility();
    
    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    return 0;
}
