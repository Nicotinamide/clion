#include "VTKViewer.h"
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkPolyDataNormals.h>
#include <vtkArrowSource.h>
#include <vtkGlyph3D.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkCell.h>

VTKViewer::VTKViewer() {
    renderer = vtkSmartPointer<vtkRenderer>::New();
    axes = vtkSmartPointer<vtkAxesActor>::New();
    orientationWidget = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    orientationWidget->SetOrientationMarker(axes);
}

VTKViewer::~VTKViewer() {
    // 析构函数实现
}

void VTKViewer::setModel(vtkSmartPointer<vtkPolyData> polyData) {
    // 使用默认渲染选项调用新的setModel方法
    setModel(polyData, defaultOptions);
}

void VTKViewer::setModel(vtkSmartPointer<vtkPolyData> polyData, const RenderOptions& options) {
    // 清除旧actor
    renderer->RemoveAllViewProps();

    // 我们不再需要VTK计算法向量，因为OCCHandler已经提供了正确的法向量
    // 只需直接使用polyData而不进行法线修正
    vtkSmartPointer<vtkPolyData> polyWithNormals = polyData;

    // 根据选项添加不同的可视化元素
    if (options.showSurface) {
        vtkSmartPointer<vtkActor> surfaceActor = createSurfaceActor(polyWithNormals, options);
        renderer->AddActor(surfaceActor);
    }

    if (options.showWireframe) {
        vtkSmartPointer<vtkActor> wireframeActor = createWireframeActor(polyWithNormals, options);
        renderer->AddActor(wireframeActor);
    }

    if (options.showNormals) {
        vtkSmartPointer<vtkActor> normalsActor = createNormalsActor(polyWithNormals, options);
        renderer->AddActor(normalsActor);
    }

    // 添加坐标轴
    renderer->AddActor(axes);

    // 更新坐标轴大小
    updateAxesSize(polyData);

    // 重置相机
    renderer->ResetCamera();
}

// 在现有模型基础上添加新的PolyData
void VTKViewer::addPolyData(vtkSmartPointer<vtkPolyData> polyData, const RenderOptions& options) {
    if (!polyData) {
        return; // 如果polyData为空，则不进行任何操作
    }

    // 直接使用polyData，不进行法线修正
    vtkSmartPointer<vtkPolyData> polyWithNormals = polyData;

    // 根据选项添加不同的可视化元素
    if (options.showSurface) {
        vtkSmartPointer<vtkActor> surfaceActor = createSurfaceActor(polyWithNormals, options);
        renderer->AddActor(surfaceActor);
    }

    if (options.showWireframe) {
        vtkSmartPointer<vtkActor> wireframeActor = createWireframeActor(polyWithNormals, options);
        renderer->AddActor(wireframeActor);
    }

    if (options.showNormals) {
        vtkSmartPointer<vtkActor> normalsActor = createNormalsActor(polyWithNormals, options);
        renderer->AddActor(normalsActor);
    }

    // 重置相机，确保所有添加的内容都可见
    renderer->ResetCamera();
}

vtkSmartPointer<vtkActor> VTKViewer::createSurfaceActor(vtkSmartPointer<vtkPolyData> polyData, const RenderOptions& options) {
    vtkSmartPointer<vtkPolyDataMapper> surfaceMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    surfaceMapper->SetInputData(polyData);
    surfaceMapper->ScalarVisibilityOff();

    vtkSmartPointer<vtkActor> surfaceActor = vtkSmartPointer<vtkActor>::New();
    surfaceActor->SetMapper(surfaceMapper);
    surfaceActor->GetProperty()->SetColor(options.surfaceColor[0], options.surfaceColor[1], options.surfaceColor[2]);
    surfaceActor->GetProperty()->SetOpacity(options.surfaceOpacity);
    surfaceActor->GetProperty()->SetAmbient(0.3);
    surfaceActor->GetProperty()->SetDiffuse(0.7);
    surfaceActor->GetProperty()->BackfaceCullingOff();
    surfaceActor->GetProperty()->FrontfaceCullingOff();

    return surfaceActor;
}

vtkSmartPointer<vtkActor> VTKViewer::createWireframeActor(vtkSmartPointer<vtkPolyData> polyData, const RenderOptions& options) {
    vtkSmartPointer<vtkPolyDataMapper> wireframeMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    wireframeMapper->SetInputData(polyData);

    vtkSmartPointer<vtkActor> wireframeActor = vtkSmartPointer<vtkActor>::New();
    wireframeActor->SetMapper(wireframeMapper);
    wireframeActor->GetProperty()->SetRepresentationToWireframe();
    wireframeActor->GetProperty()->SetColor(options.wireframeColor[0], options.wireframeColor[1], options.wireframeColor[2]);
    wireframeActor->GetProperty()->SetLineWidth(1.5);
    wireframeActor->GetProperty()->SetOpacity(1.0);
    wireframeActor->GetProperty()->SetAmbient(1.0);
    wireframeActor->GetProperty()->SetDiffuse(0.0);
    wireframeActor->GetProperty()->SetSpecular(0.0);

    return wireframeActor;
}

vtkSmartPointer<vtkActor> VTKViewer::createNormalsActor(vtkSmartPointer<vtkPolyData> polyData, const RenderOptions& options) {
    vtkDataArray* cellNormals = polyData->GetCellData()->GetNormals();
    vtkSmartPointer<vtkPoints> cellCenters = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkDoubleArray> cellNormalArray = vtkSmartPointer<vtkDoubleArray>::New();
    cellNormalArray->SetNumberOfComponents(3);
    cellNormalArray->SetName("Normals");

    for (vtkIdType i = 0; i < polyData->GetNumberOfCells(); ++i) {
        vtkCell* cell = polyData->GetCell(i);
        double center[3] = { 0,0,0 };
        int npts = cell->GetNumberOfPoints();
        for (int j = 0; j < npts; ++j) {
            double pt[3];
            polyData->GetPoint(cell->GetPointId(j), pt);
            center[0] += pt[0];
            center[1] += pt[1];
            center[2] += pt[2];
        }
        center[0] /= npts;
        center[1] /= npts;
        center[2] /= npts;
        cellCenters->InsertNextPoint(center);
        double n[3];
        cellNormals->GetTuple(i, n);
        cellNormalArray->InsertNextTuple(n);
    }

    vtkSmartPointer<vtkPolyData> cellNormalPoly = vtkSmartPointer<vtkPolyData>::New();
    cellNormalPoly->SetPoints(cellCenters);
    cellNormalPoly->GetPointData()->SetNormals(cellNormalArray);

    vtkSmartPointer<vtkArrowSource> arrowSource = vtkSmartPointer<vtkArrowSource>::New();
    vtkSmartPointer<vtkGlyph3D> glyph = vtkSmartPointer<vtkGlyph3D>::New();
    glyph->SetSourceConnection(arrowSource->GetOutputPort());
    glyph->SetInputData(cellNormalPoly);
    glyph->SetVectorModeToUseNormal();
    glyph->SetScaleFactor(options.normalScale); // 使用选项中的法线缩放因子
    glyph->OrientOn();
    glyph->Update();

    vtkSmartPointer<vtkPolyDataMapper> normalMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    normalMapper->SetInputConnection(glyph->GetOutputPort());
    vtkSmartPointer<vtkActor> normalActor = vtkSmartPointer<vtkActor>::New();
    normalActor->SetMapper(normalMapper);
    normalActor->GetProperty()->SetColor(options.normalColor[0], options.normalColor[1], options.normalColor[2]);

    return normalActor;
}

vtkSmartPointer<vtkRenderer> VTKViewer::getRenderer() const {
    return renderer;
}

vtkSmartPointer<vtkAxesActor> VTKViewer::getAxes() const {
    return axes;
}

vtkSmartPointer<vtkOrientationMarkerWidget> VTKViewer::getOrientationWidget() const {
    return orientationWidget;
}

// 添加新方法 - 更新坐标轴大小
void VTKViewer::updateAxesSize(vtkSmartPointer<vtkPolyData> polyData) {
    double bounds[6];
    polyData->GetBounds(bounds);
    double xLen = bounds[1] - bounds[0];
    double yLen = bounds[3] - bounds[2];
    double zLen = bounds[5] - bounds[4];
    double maxLenForAxes = std::max({xLen, yLen, zLen});

    axes->SetTotalLength(maxLenForAxes / 6, maxLenForAxes / 6, maxLenForAxes / 6);
    axes->SetShaftTypeToCylinder();
    axes->SetCylinderRadius(0.01);
    axes->SetPosition(0, 0, 0);
}

// 设置渲染窗口
void VTKViewer::setRenderWindow(vtkSmartPointer<vtkRenderWindow> renderWindow) {
    renderWindow->AddRenderer(renderer);
}

// 设置交互器
void VTKViewer::setInteractor(vtkSmartPointer<vtkRenderWindowInteractor> interactor) {
    orientationWidget->SetInteractor(interactor);
    orientationWidget->SetEnabled(1);
    orientationWidget->InteractiveOn();
}
