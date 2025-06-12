#include "FaceProcessor.h"
#include <iostream>
#include <iomanip>

// Test path length calculation accuracy
void testPathLengthCalculation() {
    std::cout << "=== Path Length Calculation Test ===" << std::endl;
    
    // Create a simple test path
    SprayPath testPath;

    // Test 1: Simple straight line path (0,0,0) -> (10,0,0), should be 10 units
    testPath.points.clear();
    testPath.points.push_back(PathPoint(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
    testPath.points.push_back(PathPoint(gp_Pnt(10, 0, 0), gp_Dir(0, 0, 1)));

    FaceProcessor processor;
    double length1 = processor.calculatePathLength(testPath);
    std::cout << "Test 1 - Straight line (0,0,0)->(10,0,0): "
              << std::fixed << std::setprecision(3) << length1 << " units" << std::endl;
    
    // Test 2: Three-point path (0,0,0) -> (5,0,0) -> (10,0,0), should be 10 units
    testPath.points.clear();
    testPath.points.push_back(PathPoint(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
    testPath.points.push_back(PathPoint(gp_Pnt(5, 0, 0), gp_Dir(0, 0, 1)));
    testPath.points.push_back(PathPoint(gp_Pnt(10, 0, 0), gp_Dir(0, 0, 1)));

    double length2 = processor.calculatePathLength(testPath);
    std::cout << "Test 2 - Three-point path (0,0,0)->(5,0,0)->(10,0,0): "
              << std::fixed << std::setprecision(3) << length2 << " units" << std::endl;
    
    // 测试3: L形路径 (0,0,0) -> (10,0,0) -> (10,10,0)，应该是20个单位
    testPath.points.clear();
    testPath.points.push_back(PathPoint(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
    testPath.points.push_back(PathPoint(gp_Pnt(10, 0, 0), gp_Dir(0, 0, 1)));
    testPath.points.push_back(PathPoint(gp_Pnt(10, 10, 0), gp_Dir(0, 0, 1)));
    
    double length3 = processor.calculatePathLength(testPath);
    std::cout << "测试3 - L形路径 (0,0,0)->(10,0,0)->(10,10,0): " 
              << std::fixed << std::setprecision(3) << length3 << " 单位" << std::endl;
    
    // 测试4: 对角线路径 (0,0,0) -> (3,4,0)，应该是5个单位
    testPath.points.clear();
    testPath.points.push_back(PathPoint(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
    testPath.points.push_back(PathPoint(gp_Pnt(3, 4, 0), gp_Dir(0, 0, 1)));
    
    double length4 = processor.calculatePathLength(testPath);
    std::cout << "测试4 - 对角线路径 (0,0,0)->(3,4,0): " 
              << std::fixed << std::setprecision(3) << length4 << " 单位" << std::endl;
    
    // 测试5: 密集点路径，模拟实际情况
    testPath.points.clear();
    for (int i = 0; i <= 100; i++) {
        double x = i * 0.1;  // 每0.1单位一个点，总长度10单位
        testPath.points.push_back(PathPoint(gp_Pnt(x, 0, 0), gp_Dir(0, 0, 1)));
    }
    
    double length5 = processor.calculatePathLength(testPath);
    std::cout << "测试5 - 密集点路径 (101个点，0.1间距): " 
              << std::fixed << std::setprecision(3) << length5 << " 单位" << std::endl;
    
    std::cout << "\n=== 单位分析 ===" << std::endl;
    std::cout << "如果OCCT使用毫米作为单位，那么：" << std::endl;
    std::cout << "- 测试1应该显示 10.000 (10mm)" << std::endl;
    std::cout << "- 测试4应该显示 5.000 (5mm)" << std::endl;
    std::cout << "如果OCCT使用其他单位，需要相应转换。" << std::endl;
    
    std::cout << "\n=== 筛选测试 ===" << std::endl;
    processor.setMinPathLength(20.0);  // 设置20mm阈值
    
    // 测试短路径筛选
    std::cout << "设置最小长度阈值为 20.0" << std::endl;
    std::cout << "测试1 (长度" << length1 << "): " << (length1 >= 20.0 ? "保留" : "过滤") << std::endl;
    std::cout << "测试3 (长度" << length3 << "): " << (length3 >= 20.0 ? "保留" : "过滤") << std::endl;
    std::cout << "测试5 (长度" << length5 << "): " << (length5 >= 20.0 ? "保留" : "过滤") << std::endl;
}

// 分析可能的单位问题
void analyzeUnitIssues() {
    std::cout << "\n=== 单位问题分析 ===" << std::endl;
    
    std::cout << "可能的原因：" << std::endl;
    std::cout << "1. OCCT默认单位可能不是毫米" << std::endl;
    std::cout << "2. STEP文件的单位可能是米、英寸等" << std::endl;
    std::cout << "3. 路径点密度过高，导致每段很短" << std::endl;
    std::cout << "4. 几何精度设置问题" << std::endl;
    
    std::cout << "\n解决方案：" << std::endl;
    std::cout << "1. 检查STEP文件的单位设置" << std::endl;
    std::cout << "2. 调整最小路径长度阈值" << std::endl;
    std::cout << "3. 添加单位转换" << std::endl;
    std::cout << "4. 检查实际的路径长度输出" << std::endl;
}

// 建议的阈值设置
void suggestThresholds() {
    std::cout << "\n=== 建议的阈值设置 ===" << std::endl;
    
    std::cout << "根据不同单位的建议阈值：" << std::endl;
    std::cout << "- 如果单位是毫米: 20.0 (20mm)" << std::endl;
    std::cout << "- 如果单位是厘米: 2.0 (2cm = 20mm)" << std::endl;
    std::cout << "- 如果单位是米: 0.02 (0.02m = 20mm)" << std::endl;
    std::cout << "- 如果单位是英寸: 0.787 (0.787in ≈ 20mm)" << std::endl;
    
    std::cout << "\n动态调整建议：" << std::endl;
    std::cout << "1. 先运行一次，查看实际路径长度统计" << std::endl;
    std::cout << "2. 根据最短路径长度调整阈值" << std::endl;
    std::cout << "3. 如果最短路径是0.01，可能单位是米，阈值应设为0.02" << std::endl;
    std::cout << "4. 如果最短路径是10，可能单位是毫米，阈值保持20" << std::endl;
}

int main() {
    testPathLengthCalculation();
    analyzeUnitIssues();
    suggestThresholds();
    
    std::cout << "\n=== 使用建议 ===" << std::endl;
    std::cout << "1. 运行程序并查看调试输出中的路径长度" << std::endl;
    std::cout << "2. 根据实际长度值判断单位" << std::endl;
    std::cout << "3. 相应调整最小路径长度阈值" << std::endl;
    std::cout << "4. 重新测试筛选效果" << std::endl;

    char a = std::getchar();
    return 0;
}
