# Z+方向遮挡检测优化

## 问题分析

您指出了一个关键问题：既然面是按照Z+方向（法向量）提取的，那么遮挡判断也应该严格按照Z+方向进行。之前的实现存在以下问题：

### 原始问题
1. **方向不一致**：使用通用的喷涂方向而不是Z+方向
2. **深度计算错误**：没有严格按照Z坐标计算深度
3. **遮挡逻辑模糊**：阈值设置不够精确
4. **检测不够严格**：容差过大导致误判

## 解决方案

### 1. 统一使用Z+方向
```cpp
// 使用Z+方向作为深度方向（与面提取方向一致）
gp_Dir depthDirection(0, 0, 1);  // Z+方向

// 计算路径的平均Z坐标作为深度
pathVisibility[i].depth = totalZ / validPoints;  // 平均Z坐标
```

### 2. 严格的Z方向遮挡检测
```cpp
// 在Z+方向上，Z值更大的路径遮挡Z值更小的路径
if (pathVisibility[j].depth <= pathVisibility[i].depth + DEPTH_THRESHOLD) continue;
```

### 3. 点级别的Z方向检测
```cpp
// 检查Z坐标：只有Z值更高的路径才能遮挡当前点
if (occluderMaxZ <= pointZ + pathSpacing * 0.1) {
    return false;
}
```

## 核心改进

### 1. 深度计算优化
**之前**：
```cpp
// 使用复杂的向量投影
gp_Dir sprayDirection = faceDirection.Reversed();
double depth = pointVec.Dot(gp_Vec(sprayDirection.X(), sprayDirection.Y(), sprayDirection.Z()));
```

**现在**：
```cpp
// 直接使用Z坐标
double totalZ = 0.0;
for (const auto& point : path.points) {
    totalZ += point.position.Z();
}
pathVisibility[i].depth = totalZ / validPoints;
```

### 2. 遮挡检测优化
**之前**：
```cpp
const double DEPTH_THRESHOLD = pathSpacing * 0.5;  // 阈值过大
const double OCCLUSION_THRESHOLD = 0.3;  // 阈值过高
```

**现在**：
```cpp
const double DEPTH_THRESHOLD = pathSpacing * 0.1;  // 更敏感的深度检测
const double OCCLUSION_THRESHOLD = 0.2;  // 更严格的遮挡判断
```

### 3. XY平面投影检测
**改进**：
```cpp
// 在XY平面上进行投影比较（Z+方向遮挡检测）
bool xOverlap = (path_minX < occluder_maxX) && (path_maxX > occluder_minX);
bool yOverlap = (path_minY < occluder_maxY) && (path_maxY > occluder_minY);
```

### 4. 点级别遮挡检测
**新增Z坐标检查**：
```cpp
// 检查Z坐标：只有Z值更高的路径才能遮挡当前点
double pointZ = point.position.Z();
double occluderMaxZ = std::numeric_limits<double>::lowest();

for (const auto& occluderPoint : occluderPath.points) {
    occluderMaxZ = std::max(occluderMaxZ, occluderPoint.position.Z());
}

// 如果遮挡路径的最高点都比当前点低，则不能遮挡
if (occluderMaxZ <= pointZ + pathSpacing * 0.1) {
    return false;
}
```

## 参数调整

### 关键阈值优化
1. **深度阈值**：`pathSpacing * 0.1`（更敏感）
2. **遮挡阈值**：`0.2`（20%重叠即认为遮挡）
3. **点检测容差**：`pathSpacing * 0.1`（更精确）
4. **Z方向容差**：`pathSpacing * 0.1`（严格的Z方向检测）

### 检测逻辑
```
对于路径A和路径B：
1. 计算平均Z坐标：Z_A, Z_B
2. 如果 Z_B > Z_A + threshold：B可能遮挡A
3. 检查XY平面投影重叠
4. 计算重叠比例
5. 如果重叠比例 > 20%：A被B遮挡
```

## 调试信息

### 新增调试输出
```
路径深度信息（Z坐标）：
路径 0: Z=125.50, 可见=是
路径 1: Z=123.20, 可见=否, 被路径 0 遮挡
路径 2: Z=127.80, 可见=是
路径 3: Z=124.10, 可见=否, 被路径 2 遮挡
...
```

这些信息帮助您：
- 验证Z坐标计算是否正确
- 检查遮挡关系是否合理
- 调试参数设置

## 预期效果

### 改进后的表现
1. **更精确的遮挡检测**：严格按照Z+方向判断
2. **更少的误判**：减少错误的遮挡识别
3. **更好的路径保留**：保留真正需要的表层路径
4. **更清晰的结果**：只显示最顶层的可见轨迹

### 针对您的问题
- **多层结构**：能够正确识别不同Z层级的路径
- **遮挡关系**：严格按照Z+方向判断上下层关系
- **路径分割**：精确保留露出的顶层部分
- **结果优化**：显著减少不必要的路径

## 使用建议

### 1. 参数调整
如果遮挡检测仍然不够理想，可以调整：
- 减小`DEPTH_THRESHOLD`使检测更敏感
- 降低`OCCLUSION_THRESHOLD`使遮挡判断更严格
- 调整容差值以适应具体的几何精度

### 2. 验证方法
- 查看控制台的深度信息输出
- 检查Z坐标是否符合预期
- 验证遮挡关系是否正确

### 3. 故障排除
如果仍有问题：
1. 检查输入面的Z坐标分布
2. 验证切割平面的生成是否正确
3. 调整检测阈值参数
4. 查看调试输出信息

## 总结

这次优化完全按照您的要求，将遮挡检测严格限制在Z+方向：

- ✅ **方向一致**：遮挡检测与面提取方向完全一致
- ✅ **逻辑严格**：只有Z值更高的路径才能遮挡下层路径
- ✅ **检测精确**：使用更严格的阈值和更精确的算法
- ✅ **调试友好**：提供详细的深度信息输出

现在系统应该能够更好地去除被遮挡的路径，只保留真正需要喷涂的最顶层可见部分！
