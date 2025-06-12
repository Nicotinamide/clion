# 路径长度问题解决指南

## 问题描述
在喷涂轨迹生成中，路径长度计算和筛选出现问题，主要原因是单位不匹配导致的阈值设置不当。

## 解决方案

### 1. 编译测试程序

```bash
# 在项目根目录
cd cmake-build-release  # 或其他构建目录
ninja path_length_test_simple  # 或 make path_length_test_simple
```

### 2. 运行单位检测测试

```bash
./path_length_test_simple.exe
```

这个程序会：
- 测试已知长度的路径
- 自动检测当前使用的单位
- 推荐合适的阈值设置
- 演示不同阈值的筛选效果

### 3. 在主程序中使用新的工具函数

```cpp
// 在生成路径后，使用以下函数进行单位检测和统计
FaceProcessor processor;

// ... 设置参数和生成路径 ...

// 打印路径长度统计信息
processor.printPathLengthStatistics();

// 自动检测单位并设置最优阈值
processor.detectAndSetOptimalThreshold();

// 手动检测单位
std::string units = processor.detectUnits();
std::cout << "Detected units: " << units << std::endl;
```

### 4. 手动设置阈值

根据检测到的单位，手动设置合适的阈值：

```cpp
// 根据单位设置阈值
if (units == "millimeters") {
    processor.setMinPathLength(20.0);  // 20mm
} else if (units == "meters") {
    processor.setMinPathLength(0.02);  // 0.02m = 20mm
} else if (units == "centimeters") {
    processor.setMinPathLength(2.0);   // 2cm = 20mm
} else if (units == "inches") {
    processor.setMinPathLength(0.787); // 0.787in ≈ 20mm
}
```

## 常见单位和推荐阈值

| 单位 | 测试路径长度 | 推荐阈值 | 说明 |
|------|-------------|----------|------|
| 毫米 (mm) | 10.000 | 20.0 | 最常用单位 |
| 厘米 (cm) | 1.000 | 2.0 | 2cm = 20mm |
| 米 (m) | 0.010 | 0.02 | 0.02m = 20mm |
| 英寸 (in) | 0.394 | 0.787 | 0.787in ≈ 20mm |

## 调试步骤

1. **运行测试程序**：确定当前单位
2. **查看统计信息**：了解路径长度分布
3. **设置合适阈值**：根据单位调整
4. **验证筛选效果**：检查过滤后的路径数量

## 示例输出解读

```
=== Path Length Statistics ===
Total paths: 150
Min length: 0.005
Max length: 0.250
Average length: 0.045
Current threshold: 20.0
Paths that would be filtered: 149 (99.3%)
Detected units: meters
```

这个输出表明：
- 当前单位是米
- 阈值设置为20.0（毫米单位的值）
- 几乎所有路径都会被过滤
- 应该将阈值调整为0.02（米单位）

## 集成到主程序

在 `SprayR_GUI.cpp` 中的路径生成后添加：

```cpp
// 生成路径后
if (processor.generatePaths()) {
    // 添加单位检测和统计
    processor.printPathLengthStatistics();
    processor.detectAndSetOptimalThreshold();
    
    // 继续后续处理...
}
```

## 故障排除

### 问题1：所有路径都被过滤
**原因**：阈值设置过大
**解决**：运行单位检测，调整阈值

### 问题2：没有路径被过滤
**原因**：阈值设置过小
**解决**：增加阈值或检查路径生成质量

### 问题3：单位检测失败
**原因**：STEP文件使用非标准单位
**解决**：手动设置阈值，或检查STEP文件导出设置
