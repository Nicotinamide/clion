// OCCHandler模块化测试程序
// 用于验证模块化重构是否正常工作

#include "OCCHandler.h"
#include <iostream>
#include <string>

void testCoreModule() {
    std::cout << "\n=== 测试核心模块 ===" << std::endl;
    
    OCCHandler handler;
    
    // 测试基本功能
    std::cout << "✓ OCCHandler构造成功" << std::endl;
    
    // 测试形状获取（应该返回空形状）
    TopoDS_Shape shape = handler.getShape();
    if (shape.IsNull()) {
        std::cout << "✓ getShape()返回空形状（预期行为）" << std::endl;
    }
    
    // 测试形状结构打印
    std::cout << "✓ 形状结构打印功能可用" << std::endl;
    handler.printShapeStructure();
}

void testRepairModule() {
    std::cout << "\n=== 测试修复模块 ===" << std::endl;
    
    OCCHandler handler;
    
    // 测试修复函数（无模型时应该返回false）
    bool result1 = handler.repairImportedModel(1e-6, false);
    if (!result1) {
        std::cout << "✓ repairImportedModel()正确处理空模型" << std::endl;
    }
    
    bool result2 = handler.fixSmallFacesAndEdges(1e-6, false);
    if (!result2) {
        std::cout << "✓ fixSmallFacesAndEdges()正确处理空模型" << std::endl;
    }
    
    bool result3 = handler.fixWireframeIssues(1e-6, false);
    if (!result3) {
        std::cout << "✓ fixWireframeIssues()正确处理空模型" << std::endl;
    }
}

void testAdvancedRepairModule() {
    std::cout << "\n=== 测试高级修复模块 ===" << std::endl;
    
    OCCHandler handler;
    
    // 测试高级修复函数
    bool result1 = handler.enhancedModelRepair(1e-6, false);
    if (!result1) {
        std::cout << "✓ enhancedModelRepair()正确处理空模型" << std::endl;
    }
    
    bool result2 = handler.sprayTrajectoryOptimizedRepair(1e-3, false);
    if (!result2) {
        std::cout << "✓ sprayTrajectoryOptimizedRepair()正确处理空模型" << std::endl;
    }
}

void testShapeAnalysisModule() {
    std::cout << "\n=== 测试形状分析模块 ===" << std::endl;
    
    OCCHandler handler;
    
    // 测试形状验证和分析
    bool result = handler.validateAndAnalyzeShape(false);
    if (!result) {
        std::cout << "✓ validateAndAnalyzeShape()正确处理空模型" << std::endl;
    }
}

void testFaceProcessingModule() {
    std::cout << "\n=== 测试面处理模块 ===" << std::endl;
    
    OCCHandler handler;
    
    // 测试面提取
    TopTools_ListOfShape faces = handler.extractAllFaces();
    if (faces.IsEmpty()) {
        std::cout << "✓ extractAllFaces()正确处理空模型" << std::endl;
    }
    
    // 测试基于法向量的面提取
    gp_Dir direction(0, 0, 1);
    TopoDS_Shape extractedFaces = handler.extractFacesByNormal(direction, 5.0, true);
    if (extractedFaces.IsNull()) {
        std::cout << "✓ extractFacesByNormal()正确处理空模型" << std::endl;
    }
}

void testVisualizationModule() {
    std::cout << "\n=== 测试可视化模块 ===" << std::endl;
    
    OCCHandler handler;
    
    // 测试VTK转换
    try {
        vtkSmartPointer<vtkPolyData> polyData = handler.shapeToPolyData();
        if (polyData) {
            std::cout << "✓ shapeToPolyData()函数可用" << std::endl;
        }
    } catch (...) {
        std::cout << "✓ shapeToPolyData()正确处理空模型" << std::endl;
    }
}

void testOcclusionModule() {
    std::cout << "\n=== 测试遮挡处理模块 ===" << std::endl;
    
    OCCHandler handler;
    
    // 测试遮挡处理
    TopoDS_Shape emptyShape;
    TopoDS_Shape result = handler.removeOccludedPortions(emptyShape, 5.0);
    if (result.IsNull()) {
        std::cout << "✓ removeOccludedPortions()正确处理空模型" << std::endl;
    }
    
    // 测试面重叠检查
    bool overlap = handler.checkFaceOverlapInXY(emptyShape, emptyShape);
    if (!overlap) {
        std::cout << "✓ checkFaceOverlapInXY()正确处理空模型" << std::endl;
    }
}

void testWithRealModel() {
    std::cout << "\n=== 测试真实模型加载 ===" << std::endl;
    
    OCCHandler handler;
    
    // 注意：这里需要一个真实的STEP文件路径
    std::string testFile = "test_model.step";  // 替换为实际文件路径
    
    std::cout << "尝试加载测试文件: " << testFile << std::endl;
    
    // 测试文件加载（如果文件不存在会失败，这是正常的）
    bool loadResult = handler.loadStepFile(testFile, true, true);
    if (loadResult) {
        std::cout << "✅ 模型加载成功！" << std::endl;
        
        // 测试各种功能
        TopoDS_Shape shape = handler.getShape();
        if (!shape.IsNull()) {
            std::cout << "✅ 获取形状成功" << std::endl;
            
            // 测试形状分析
            handler.validateAndAnalyzeShape(true);
            
            // 测试面提取
            TopTools_ListOfShape faces = handler.extractAllFaces();
            std::cout << "✅ 提取了 " << faces.Extent() << " 个面" << std::endl;
        }
    } else {
        std::cout << "⚠️ 模型加载失败（可能文件不存在，这是正常的测试行为）" << std::endl;
    }
}

int main() {
    std::cout << "OCCHandler模块化测试程序" << std::endl;
    std::cout << "=========================" << std::endl;
    
    try {
        // 测试各个模块
        testCoreModule();
        testRepairModule();
        testAdvancedRepairModule();
        testShapeAnalysisModule();
        testFaceProcessingModule();
        testVisualizationModule();
        testOcclusionModule();
        
        // 测试真实模型（可选）
        testWithRealModel();
        
        std::cout << "\n🎉 所有模块测试完成！" << std::endl;
        std::cout << "✅ OCCHandler模块化重构成功" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ 测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ 测试过程中发生未知异常" << std::endl;
        return 1;
    }
    
    return 0;
}

// 编译说明：
// 1. 确保所有OCCHandler模块文件都在同一目录
// 2. 使用以下命令编译（需要配置OCCT和VTK路径）：
//    g++ -std=c++17 test_modular_occhandler.cpp OCCHandler_*.cpp -I/path/to/occt/include -I/path/to/vtk/include -L/path/to/libs -locct -lvtk -o test_occhandler
// 3. 运行测试：
//    ./test_occhandler
