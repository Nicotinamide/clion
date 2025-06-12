# 表面可见性分析功能说明

## 概述

表面可见性分析功能解决了喷涂过程中的关键问题：**只喷涂最表层可见表面，避免对被遮挡表面的无效喷涂**。这个功能通过分析轨迹的空间关系，自动识别并过滤掉被遮挡的路径段，确保喷涂设备只在真正需要的表面上工作。

## 核心问题

### 原始问题
- 所有切割平面生成的路径都被包含在喷涂轨迹中
- 内层或被遮挡的表面也会被喷涂
- 浪费涂料和时间
- 可能导致涂层过厚或不均匀

### 解决方案
- 🔍 **深度分析**：计算每条路径在喷涂方向上的深度
- 🚫 **遮挡检测**：识别被其他路径遮挡的区域
- 📊 **层级分类**：将路径按深度分为不同表面层级
- ✅ **表层过滤**：只保留最表层的可见路径

## 核心算法

### 1. 深度计算 (`calculatePathDepths`)
```cpp
// 计算路径在喷涂方向上的投影深度
for each path:
    depth = average(point.position · sprayDirection)
```

**原理**：
- 使用喷涂方向（通常是面法向量的反方向）作为深度轴
- 计算路径上所有点在该方向上的平均投影
- 深度值越大，表示越靠近喷涂源（越表层）

### 2. 遮挡检测 (`detectOcclusions`)
```cpp
for each path A:
    for each path B:
        if (depth_B > depth_A + threshold):
            if (projectionOverlap(A, B)):
                A is occluded by B
```

**检测条件**：
- 深度差异：遮挡路径必须比被遮挡路径更靠前
- 投影重叠：在垂直于喷涂方向的平面上有重叠区域
- 遮挡比例：重叠面积超过设定阈值

### 3. 层级分类 (`classifySurfaceLayers`)
```cpp
sort paths by depth (descending)
for each path:
    if (depth_difference > layer_threshold):
        create new layer
    add path to current layer
```

**分层策略**：
- 按深度从大到小排序
- 深度差异超过阈值时创建新层级
- 第0层为最表层（最重要的喷涂目标）

### 4. 点级别可见性分析 (`analyzePointLevelVisibility`)
- 检查每个路径点是否被其他路径遮挡
- 生成每个点的可见性标记
- 识别连续的可见段

### 5. 路径分割 (`segmentPartiallyOccludedPaths`)
- 对部分遮挡的路径进行智能分割
- 保留露出的可见段，删除被遮挡的部分
- 生成新的路径段，每段都是完全可见的

### 6. 可见性过滤 (`filterVisiblePaths`)
- 标记被遮挡路径的点为非喷涂点
- 保留可见路径段的喷涂属性
- 统计分割和过滤效果

## 数据结构

### VisibilityInfo（可见性信息）
```cpp
struct VisibilityInfo {
    bool isVisible;                 // 是否可见
    double depth;                   // 深度值
    int occludingPathIndex;         // 遮挡路径索引
    double occlusionRatio;          // 遮挡比例
};
```

### SurfaceLayer（表面层级）
```cpp
struct SurfaceLayer {
    std::vector<int> pathIndices;   // 该层路径索引
    double averageDepth;            // 平均深度
    int layerIndex;                 // 层级索引
};
```

## 算法参数

### 关键阈值
- **深度阈值** (`DEPTH_THRESHOLD`): `pathSpacing * 0.5`
  - 用于判断两条路径是否在同一深度层
  - 过小：过度敏感，可能误判
  - 过大：可能遗漏真实的遮挡关系

- **遮挡阈值** (`OCCLUSION_THRESHOLD`): `0.3` (30%)
  - 重叠面积比例超过此值才认为被遮挡
  - 避免因微小重叠而误判

- **层级阈值** (`LAYER_THRESHOLD`): `pathSpacing * 0.8`
  - 用于区分不同表面层级
  - 基于路径间距，确保合理分层

## 使用效果

### 可视化说明
- **绿色轨迹**：喷涂路径（实际进行喷涂的路径段）
- **橙色轨迹**：连接路径（非喷涂的过渡段）
- **青蓝色平面**：切割平面（半透明）
- **颜色统一**：所有切割面生成的喷涂路径都使用相同的绿色
- **功能区分**：通过颜色清晰区分喷涂段和过渡段

### 性能提升
1. **涂料节省**：避免对不可见表面的喷涂
2. **时间效率**：减少无效的喷涂动作
3. **质量改善**：避免过度喷涂导致的质量问题
4. **成本降低**：减少涂料消耗和设备磨损
5. **界面简洁**：只显示必要的轨迹，操作更直观

## 使用方法

### GUI操作流程
1. 加载STEP文件
2. 点击"提取faces"按钮
3. 点击"添加切割面"按钮
4. 系统自动执行：
   - 路径生成
   - 轨迹整合
   - **表面可见性分析**
   - 表层轨迹提取

### 编程接口
```cpp
FaceProcessor processor;
processor.setShape(shape);
processor.setCuttingParameters(direction, spacing, offset, density);

if (processor.generatePaths()) {
    if (processor.integrateTrajectories()) {
        // 关键步骤：表面可见性分析
        if (processor.analyzeSurfaceVisibility()) {
            auto layers = processor.getSurfaceLayers();
            auto trajectories = processor.getIntegratedTrajectories();
            // 现在轨迹只包含表层可见部分
        }
    }
}
```

## 算法优势

### 1. 智能化
- 自动识别表面层级关系
- 无需人工干预的遮挡检测
- 基于几何分析的科学方法

### 2. 高效性
- O(n²)的遮挡检测复杂度
- 基于边界框的快速重叠计算
- 分层处理减少计算量

### 3. 准确性
- 多重条件验证遮挡关系
- 可调节的阈值参数
- 考虑实际喷涂几何约束

### 4. 实用性
- 直接应用于工业喷涂场景
- 显著的成本和效率提升
- 易于集成到现有系统

## 扩展功能

### 未来改进方向
1. **精确遮挡检测**：使用射线追踪等更精确的方法
2. **动态阈值**：根据几何复杂度自适应调整参数
3. **部分遮挡处理**：对部分被遮挡的路径进行分段处理
4. **多角度分析**：考虑不同喷涂角度的可见性
5. **实时优化**：在喷涂过程中动态调整轨迹

## 故障排除

### 常见问题
1. **过度过滤**：降低遮挡阈值或深度阈值
2. **遗漏遮挡**：提高检测精度或调整层级阈值
3. **分层不当**：调整层级阈值参数
4. **性能问题**：优化边界框计算或使用空间索引

### 调试方法
- 检查深度计算结果
- 验证遮挡检测逻辑
- 分析层级分类效果
- 可视化对比原始和过滤后的轨迹
