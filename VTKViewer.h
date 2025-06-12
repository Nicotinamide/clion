#pragma once

#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkPolyData.h>
#include <vtkAxesActor.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkActor.h>

class VTKViewer {
public:
    // 模型显示选项
    struct RenderOptions {
        bool showSurface = true;       // 是否显示面
        bool showWireframe = true;     // 是否显示线框
        bool showNormals = true;       // 是否显示法线
        double surfaceOpacity = 1.0;   // 面的不透明度
        double normalScale = 50.0;     // 法线箭头大小
        double surfaceColor[3] = {0.75, 0.75, 0.75}; // 面的颜色（默认银色）
        double wireframeColor[3] = {0.0, 0.0, 0.0};  // 线框颜色（默认黑色）
        double normalColor[3] = {1.0, 0.0, 0.0};     // 法线颜色（默认红色）
    };

    VTKViewer();
    ~VTKViewer();

    // 设置要显示的PolyData (旧接口，使用默认渲染选项)
    void setModel(vtkSmartPointer<vtkPolyData> polyData);

    // 设置要显示的PolyData (新接口，带渲染选项)
    void setModel(vtkSmartPointer<vtkPolyData> polyData, const RenderOptions& options);

    // 在现有模型基础上添加新的PolyData
    void addPolyData(vtkSmartPointer<vtkPolyData> polyData, const RenderOptions& options);

    // 创建和显示面
    vtkSmartPointer<vtkActor> createSurfaceActor(vtkSmartPointer<vtkPolyData> polyData, const RenderOptions& options);

    // 创建和显示线框
    vtkSmartPointer<vtkActor> createWireframeActor(vtkSmartPointer<vtkPolyData> polyData, const RenderOptions& options);

    // 创建和显示法线
    vtkSmartPointer<vtkActor> createNormalsActor(vtkSmartPointer<vtkPolyData> polyData, const RenderOptions& options);

    // 获取渲染器
    vtkSmartPointer<vtkRenderer> getRenderer() const;
    // 获取坐标轴
    vtkSmartPointer<vtkAxesActor> getAxes() const;
    // 获取方向标记控件
    vtkSmartPointer<vtkOrientationMarkerWidget> getOrientationWidget() const;

    // 更新坐标轴大小
    void updateAxesSize(vtkSmartPointer<vtkPolyData> polyData);

    // 设置渲染窗口
    void setRenderWindow(vtkSmartPointer<vtkRenderWindow> renderWindow);

    // 设置交互器
    void setInteractor(vtkSmartPointer<vtkRenderWindowInteractor> interactor);

private:
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkAxesActor> axes;
    vtkSmartPointer<vtkOrientationMarkerWidget> orientationWidget;
    RenderOptions defaultOptions; // 默认渲染选项
};
