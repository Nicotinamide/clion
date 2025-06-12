//
// Created by 25736 on 25-6-1.
//

#ifndef SPRAYR_GUI_H
#define SPRAYR_GUI_H

#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkSmartPointer.h>
#include <vtkPolyDataMapper.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkPolyData.h>
#include <vtkRenderer.h>
#include <vtkActor.h>
#include <vtkAxesActor.h>
#include <vtkOrientationMarkerWidget.h>
#include "VTKViewer.h"
#include "OCCHandler.h"

// --- 新增的VTK头文件，用于路径显示 ---
#include <vtkPoints.h>         // For path points
#include <vtkCellArray.h>      // For path line segments
#include <vtkPolyLine.h>       // For defining the path as a polyline
#include <vtkProperty.h>       // For path actor properties (color, line width)
// --- 结束新增 ---

class Spray_GUI : public QMainWindow {
    Q_OBJECT
public:
    explicit Spray_GUI(QWidget* parent = nullptr);
    ~Spray_GUI();

private:
    QWidget* centralWidget;
    QHBoxLayout* mainLayout;
    QVBoxLayout* leftLayout;
    QHBoxLayout* buttonLayout;
    QPushButton* btnLoadModel;
    QPushButton* btnRotateX;
    QPushButton* btnRotateY;
    QPushButton* btnRotateZ;
    QPushButton* btnextractFaces;
    QPushButton* btnaddcutFaces; // 添加切割面

    QVTKOpenGLNativeWidget* vtkWidget;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow;
    vtkSmartPointer<vtkPolyData> currentPoly;
    vtkSmartPointer<vtkActor> m_modelActor; // << 你已有的模型Actor (在 modelProcessor 中也有一个, 注意区分)
    vtkSmartPointer<vtkAxesActor> axes;
    vtkSmartPointer<vtkOrientationMarkerWidget> orientationWidget;
    bool useColorBarMode;
    vtkSmartPointer<vtkAxesActor> originAxes;
    // --- 新增的成员变量，用于路径显示 ---
    vtkSmartPointer<vtkActor> m_sprayPathVTKActor; //  使用 "VTK" 后缀以区分
    vtkSmartPointer<vtkActor> m_nonSprayPathVTKActor; // 非喷涂路径可视化Actor
    // --- 结束新增 ---
    VTKViewer vtkViewer; // 添加VTKViewer成员
    OCCHandler occHandler;
    // --- 新增的渲染选项 ---
    VTKViewer::RenderOptions defaultOptions; // 默认渲染选项

    // std::set<vtkIdType> getVisibleCellIdsByHardwareSelector(vtkRenderWindow* renderWindow, vtkRenderer* renderer, vtkPolyData* polyData);

    void setupUI();
    void connectSignals();
    void updateAxes();
    // void resetCameraToShowActor(vtkRenderer* renderer, vtkActor* actor); // 添加重置相机视角方法
    // void showExportWindow(const std::set<vtkIdType>& filteredVisibleCellIds);
    // void previewVisibleFacesWithColorBar(const double viewDir[3], bool onlyColorBar, const std::set<vtkIdType>* externalVisibleCellIds = nullptr);

};

/*
    STEP文件
    │
    ▼
    STEPControl_Reader
    │
    ▼
    TopoDS_Shape（OpenCASCADE几何体）
    │
    ▼
    Poly_Triangulation（OpenCASCADE三角网格）
    │
    ▼
    vtkPolyData（VTK网格数据）
    │
    ▼
    vtkPolyDataMapper（数据映射器）
    │
    ▼
    vtkActor（渲染对象）
    │
    ▼
    vtkRenderer（渲染器）
    │
    ▼
    vtkRenderWindow（渲染窗口）

 */


#endif //SPRAYR_GUI_H
