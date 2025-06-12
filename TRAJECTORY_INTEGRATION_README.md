# 喷涂轨迹整合功能说明

## 概述

本功能解决了从多条分散轨迹整合为连续可用喷涂轨迹的问题。原始的路径生成会为每个切割平面与面的交线生成独立的路径段，这些分散的路径段不适合直接用于喷涂作业。轨迹整合功能将这些分散的路径段连接成连续的喷涂轨迹。

## 问题背景

### 原始问题
- 每个切割平面与面的交线生成独立路径
- 路径之间没有连接关系
- 无法直接用于连续喷涂作业
- 缺乏路径优化和排序

### 解决方案
- 按切割平面分组路径
- 智能排序和连接相邻路径
- 生成连接路径（过渡段）
- 区分喷涂段和非喷涂段

## 核心数据结构

### PathPoint（路径点）
```cpp
struct PathPoint {
    gp_Pnt position;     // 点的位置
    gp_Dir normal;       // 点的法向量
    bool isSprayPoint;   // 是否为喷涂点
};
```

### SprayPath（喷涂路径）
```cpp
struct SprayPath {
    std::vector<PathPoint> points;  // 路径上的点
    double width;                   // 路径宽度
    int pathIndex;                  // 路径索引
    int planeIndex;                 // 所属切割平面索引
    bool isConnected;               // 是否已连接到其他路径
};
```

### ConnectionPath（连接路径）
```cpp
struct ConnectionPath {
    std::vector<PathPoint> points;  // 连接路径上的点
    int fromPathIndex;              // 起始路径索引
    int toPathIndex;                // 目标路径索引
    bool isTransition;              // 是否为过渡路径（非喷涂）
};
```

### IntegratedTrajectory（整合轨迹）
```cpp
struct IntegratedTrajectory {
    std::vector<PathPoint> points;  // 整合后的所有点
    std::vector<int> pathSegments;  // 路径段分界点索引
    double totalLength;             // 总长度
    int trajectoryIndex;            // 轨迹索引
};
```

## 核心算法

### 1. 路径分组（groupPathsByPlane）
- 将路径按所属切割平面分组
- 为每个平面的路径组创建独立的整合轨迹

### 2. 路径排序（sortPathsInPlane）
- 使用贪心算法对平面内路径排序
- 每次选择距离当前路径最近的未访问路径
- 最小化路径间的移动距离

### 3. 路径连接（connectAdjacentPaths）
- 连接排序后的相邻路径
- 检查并优化路径方向
- 创建连接路径填补间隙

### 4. 连接路径生成（createConnectionPath）
- 在两条路径之间创建平滑过渡
- 使用线性插值生成连接点
- 标记连接点为非喷涂点

### 5. 方向优化（shouldReversePath）
- 判断是否需要反转路径方向
- 基于端点距离选择最优连接方式
- 确保路径连接的连续性

## 使用方法

### 1. 基本使用流程
```cpp
FaceProcessor processor;
processor.setShape(shape);
processor.setCuttingParameters(direction, spacing, offset, density);

// 生成原始路径
if (processor.generatePaths()) {
    // 整合轨迹
    if (processor.integrateTrajectories()) {
        // 获取整合结果
        const auto& trajectories = processor.getIntegratedTrajectories();
        
        // 可视化
        auto polyData = processor.integratedTrajectoriesToPolyData();
    }
}
```

### 2. GUI操作步骤
1. 加载STEP文件
2. 点击"提取faces"按钮
3. 点击"添加切割面"按钮
4. 系统自动进行轨迹整合
5. 查看结果：
   - 绿色线条：整合后的连续轨迹
   - 红色线条：原始分散路径

## 可视化说明

### 颜色编码
- **绿色轨迹**：整合后的连续喷涂轨迹
- **红色路径**：原始分散路径段
- **灰色段**：连接路径（过渡段，非喷涂）

### 点类型
- **喷涂点**：实际进行喷涂的点（isSprayPoint = true）
- **过渡点**：路径连接时的过渡点（isSprayPoint = false）

## 优势和效果

### 1. 提高喷涂效率
- 减少喷涂设备的启停次数
- 连续的轨迹减少空行程时间
- 优化的路径顺序减少总移动距离

### 2. 改善喷涂质量
- 连续轨迹确保覆盖均匀
- 减少重复喷涂和遗漏
- 平滑的过渡减少喷涂缺陷

### 3. 优化机器人运动
- 连续轨迹减少急停急起
- 平滑的路径连接
- 明确区分工作段和移动段

## 参数调整建议

### 路径间距（pathSpacing）
- 较小值：更密集的路径，更好的覆盖
- 较大值：较少的路径，更快的处理

### 偏移距离（offsetDistance）
- 根据喷涂设备和工件调整
- 确保喷涂距离合适

### 点密度（pointDensity）
- 影响路径的平滑度
- 较高密度提供更精确的轨迹

## 故障排除

### 常见问题
1. **轨迹不连续**：检查路径间距设置
2. **连接间隙过大**：调整连接算法参数
3. **路径方向错误**：检查方向优化逻辑
4. **性能问题**：减少点密度或增加路径间距

### 调试方法
- 使用测试程序验证算法
- 检查轨迹统计信息
- 验证轨迹连续性
- 可视化检查结果

## 扩展功能

### 未来改进方向
1. 更智能的路径排序算法
2. 考虑喷涂方向的优化
3. 多层轨迹的整合
4. 碰撞检测和避障
5. 速度和加速度优化
