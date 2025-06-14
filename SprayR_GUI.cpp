//
// Created by 25736 on 25-6-1.
//

#include "SprayR_GUI.h"
#include <QFileDialog>
#include <QDebug>
#include <vtkHardwareSelector.h>
#include <vtkSelection.h>
#include <vtkSelectionNode.h>
#include <vtkIdTypeArray.h>
#include <vtkUnsignedCharArray.h>
#include <vtkCellData.h>
#include <vtkCellArray.h>
#include <vtkAppendPolyData.h>
#include <vtkCamera.h>
#include <vtkProperty.h>
#include <vtkRendererCollection.h>
#include <vtkScalarBarActor.h>
#include <vtkLookupTable.h>
#include <vtkIdFilter.h>
#include <vtkPoints.h>
#include <vtkActorCollection.h>
#include <TopoDS_Face.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <GProp_GProps.hxx>
#include <BRepGProp.hxx>
#include <gp_Pln.hxx>
#include <set>
#include <map>
#include <vector>
#include <limits>
#include <cmath>
#include <QTimer>
#include <vtkTransform.h>
#include <vtkPolyDataNormals.h>
#include <vtkArrowSource.h>
#include <vtkGlyph3D.h>
#include <vtkPointData.h>
#include <vtkSphereSource.h>
#include <QMessageBox>
#include <QLabel>

#include "FaceProcessor.h"


Spray_GUI::Spray_GUI(QWidget* parent)
    : QMainWindow(parent), useColorBarMode(false), m_sprayPathVTKActor(nullptr), m_nonSprayPathVTKActor(nullptr)
{
    setupUI();
    connectSignals();

    // 初始化VTKViewer
    vtkViewer.setRenderWindow(renderWindow);
    // 延迟设置交互器，因为此时interactor可能尚未创建
    QTimer::singleShot(100, this, [this]() {
        auto interactor = renderWindow->GetInteractor();
        if (interactor) {
            vtkViewer.setInteractor(interactor);
        }
    });
}

Spray_GUI::~Spray_GUI() {}

void Spray_GUI::setupUI() {
    // 创建中央主窗口部件
    centralWidget = new QWidget(this);
    // 创建主水平布局
    mainLayout = new QHBoxLayout(centralWidget);
    // 创建左侧垂直布局（用于按钮和VTK窗口）
    leftLayout = new QVBoxLayout();
    // 创建按钮水平布局
    buttonLayout = new QHBoxLayout();
    // 创建所有按钮并格式化命名
    btnLoadModel         = new QPushButton(QStringLiteral("导入STEP模型"), centralWidget);
    btnRotateX           = new QPushButton(QStringLiteral("绕X旋转90"), centralWidget);
    btnRotateY           = new QPushButton(QStringLiteral("绕Y旋转90"), centralWidget);
    btnRotateZ           = new QPushButton(QStringLiteral("绕Z旋转90"), centralWidget);
    btnextractFaces     = new QPushButton(QStringLiteral("提取shells"), centralWidget);
    btnaddcutFaces      = new QPushButton(QStringLiteral("添加切割面"), centralWidget); // 添加切割面按钮
    // 将所有按钮添加到按钮布局
    buttonLayout->addWidget(btnLoadModel);
    buttonLayout->addWidget(btnRotateX);
    buttonLayout->addWidget(btnRotateY);
    buttonLayout->addWidget(btnRotateZ);
    buttonLayout->addWidget(btnextractFaces);
    buttonLayout->addWidget(btnaddcutFaces); // 添加切割面按钮

    // 按钮布局加入左侧垂直布局
    leftLayout->addLayout(buttonLayout);
    // 创建VTK显示窗口并加入左侧布局
    vtkWidget = new QVTKOpenGLNativeWidget(this);
    leftLayout->addWidget(vtkWidget, 1);
    // 左侧布局加入主布局，比例为3
    mainLayout->addLayout(leftLayout, 3);
    // 设置主窗口的中央部件
    setCentralWidget(centralWidget);
    // 设置主窗口初始大小
    resize(1100, 700);
    // 设置主窗口标题
    setWindowTitle("STEP Model Viewer (Qt + VTK)");
    // 创建PolyDataMapper用于模型渲染
    renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();    // 初始化渲染器// 将渲染窗口绑定到VTK控件
    vtkWidget->setRenderWindow(renderWindow);
    // 创建坐标轴Actor（用于方向指示）
    axes = vtkSmartPointer<vtkAxesActor>::New();
    // 创建方向标记控件并设置坐标轴
    orientationWidget = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    orientationWidget->SetOrientationMarker(axes);
    // 初始化原点坐标轴（显示在模型原点，随模型一起旋转）
    originAxes = vtkSmartPointer<vtkAxesActor>::New();
    originAxes->SetTotalLength(1.0, 1.0, 1.0); // 长度后续自适应
    originAxes->SetShaftTypeToCylinder();
    originAxes->SetCylinderRadius(0.01);
    originAxes->SetPosition(0, 0, 0);
    // 让末端坐标轴不显示XYZ字母，只显示三根轴，不显示任何字母
    axes->SetXAxisLabelText("");
    axes->SetYAxisLabelText("");
    axes->SetZAxisLabelText("");
    originAxes->SetXAxisLabelText("");
    originAxes->SetYAxisLabelText("");
    originAxes->SetZAxisLabelText("");
}

void Spray_GUI::connectSignals() {
    // 导入STEP模型按钮
    connect(btnLoadModel, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getOpenFileName(this, "选择STEP文件", "", "STEP Files (*.step *.stp)");
        if (fileName.isEmpty()) return;


        // 使用基础修复功能加载STEP文件（保持法向量方向）
        // 参数说明：文件名，移动到原点=true，自动修复=false（避免法向量问题）
        if (!occHandler.loadStepFile(fileName.toStdString(), true, true)) {
            QMessageBox::warning(this, "加载失败", "STEP文件加载失败！");
            return;
        }

        TopoDS_Shape shape = occHandler.getShape();
        std::cout << "📋 模型结构分析：" << std::endl;
        occHandler.printShapeStructure(shape, TopAbs_SHELL, std::cout, 0); // 显示完整结构和统计信息

        vtkSmartPointer<vtkPolyData> poly = occHandler.shapeToPolyData();
        if (!poly || poly->GetNumberOfPoints() == 0) {
            QMessageBox::warning(this, "转换失败", "STEP模型转换为VTK数据失败！");
            return;
        }

        currentPoly = poly; // 保存当前模型数据

        // 渲染参数设置
        defaultOptions.showSurface = true; // 显示表面
        defaultOptions.showWireframe = true; // 显示线框
        defaultOptions.showNormals = true; // 显示法线
        defaultOptions.surfaceOpacity = 1.0; // 不透明度
        defaultOptions.normalScale = 50.0; // 法线箭头大小
        // 明确设置银色
        defaultOptions.surfaceColor[0] = 0.75; // 银色 R
        defaultOptions.surfaceColor[1] = 0.75; // 银色 G
        defaultOptions.surfaceColor[2] = 0.75; // 银色 B

        // 更新VTKViewer显示模型

        // 使用VTKViewer显示模型，替代原来的updateModelView调用
        vtkViewer.setModel(poly, defaultOptions);
        renderWindow->Render();
    });

    // 旋转按钮（恢复为只旋转模型）
    connect(btnRotateX, &QPushButton::clicked, this, [this]() {
        gp_Dir axis(1, 0, 0); // X轴
        occHandler.rotate90(axis);
        vtkSmartPointer<vtkPolyData> poly = occHandler.shapeToPolyData();
        if (!poly || poly->GetNumberOfPoints() == 0) {
            QMessageBox::warning(this, "转换失败", "STEP模型转换为VTK数据失败！");
            return;
        }
        currentPoly = poly; // 更新当前模型数据
        vtkViewer.setModel(poly, defaultOptions);
        renderWindow->Render();

        });
    connect(btnRotateY, &QPushButton::clicked, this, [this]() {
        gp_Dir axis(0, 1, 0); // Y轴
        occHandler.rotate90(axis);
        vtkSmartPointer<vtkPolyData> poly = occHandler.shapeToPolyData();
        if (!poly || poly->GetNumberOfPoints() == 0) {
            QMessageBox::warning(this, "转换失败", "STEP模型转换为VTK数据失败！");
            return;
        }
        currentPoly = poly; // 更新当前模型数据
        vtkViewer.setModel(poly, defaultOptions);
        renderWindow->Render();
        });
    connect(btnRotateZ, &QPushButton::clicked, this, [this]() {
        gp_Dir axis(0, 0, 1); // Z轴
        occHandler.rotate90(axis);
        vtkSmartPointer<vtkPolyData> poly = occHandler.shapeToPolyData();
        if (!poly || poly->GetNumberOfPoints() == 0) {
            QMessageBox::warning(this, "转换失败", "STEP模型转换为VTK数据失败！");
            return;
        }
        currentPoly = poly; // 更新当前模型数据
        vtkViewer.setModel(poly, defaultOptions);
        renderWindow->Render();
        });


    // 提取shells按钮（集成面合并功能）
    connect(btnextractFaces, &QPushButton::clicked, this, [this]() {
            gp_Dir direction(0, 0, 1); // 默认法向量方向为Z轴

            try {
                // 第一步：提取面
                std::cout << "🔍 基于法向量提取面..." << std::endl;
                TopoDS_Shape extractedFaces = occHandler.extractFacesByNormal(direction, 5.0);

                if (extractedFaces.IsNull()) {
                    QMessageBox::warning(this, "提取失败", "未能提取到任何面！");
                    return;
                }

                std::cout << "📋 提取面的原始结构：" << std::endl;
                occHandler.printShapeStructure(extractedFaces, TopAbs_SHELL, std::cout, 0);

                // 第二步：自动进行遮挡裁剪
                std::cout << "🔗 自动进行遮挡裁剪..." << std::endl;
                // 使用更小的高度容差，更精确的分层
                TopoDS_Shape processedShape = occHandler.removeOccludedPortions(extractedFaces, 1.0);

                if (!processedShape.IsNull()) {
                    // 使用遮挡裁剪后的结果
                    extractedShells = processedShape;
                    std::cout << "✅ 遮挡裁剪完成，最终结构：" << std::endl;
                    occHandler.printShapeStructure(extractedShells, TopAbs_SHELL, std::cout, 0);
                } else {
                    // 如果裁剪失败，使用原始提取的面
                    std::cout << "⚠️ 遮挡裁剪失败，使用原始提取的面" << std::endl;
                    extractedShells = extractedFaces;
                }

                std::cout << "✅ 面提取和遮挡裁剪完成，已保存结果用于后续处理" << std::endl;

                // 显示最终结果
                vtkSmartPointer<vtkPolyData> poly = occHandler.shapeToPolyData(extractedShells);
                if (poly && poly->GetNumberOfPoints() > 0) {
                    vtkViewer.setModel(poly, defaultOptions);
                    renderWindow->Render();
                } else {
                    QMessageBox::warning(this, "显示失败", "无法转换结果为可视化数据！");
                }

            } catch (const std::exception& e) {
                QMessageBox::critical(this, "处理错误",
                                    QString("面提取和合并过程中发生错误: %1").arg(e.what()));
            } catch (...) {
                QMessageBox::critical(this, "处理错误", "面提取和合并过程中发生未知错误");
            }
        });

    connect(btnaddcutFaces, &QPushButton::clicked, this, [this]() {
        if (extractedShells.IsNull()) {
            QMessageBox::warning(this, "操作错误", "请先点击'提取shells'按钮提取面！");
            return;
        }

        gp_Dir direction(0, 0, 1); // 默认法向量方向为Z轴

        std::cout << "🎯 使用当前保存的shells生成切割路径..." << std::endl;
        std::cout << "📋 当前shells可能是原始提取的或经过重叠裁剪的" << std::endl;

        FaceProcessor processor;

        processor.setShape(extractedShells);

        try {
            // 增加间距，减少切割平面数量
            // 设置切割参数（可选）- 增加路径间距以减少生成的路径数量
            double pathSpacing = 200;  // 增加间距到200.0
            double offsetDistance = 300.0;  // 保持偏移距离为0.0
            processor.setCuttingParameters(direction, pathSpacing, offsetDistance, 0.2);  // 设置点密度为0.2



            // 生成切割平面
            std::cout << "开始生成切割平面..." << std::endl;
            if (processor.generateCuttingPlanes()) {
                // 显示切割平面
                std::cout << "生成切割平面成功，准备显示..." << std::endl;

                // 创建用于显示切割平面的PolyData
                vtkSmartPointer<vtkPolyData> planeData = processor.cuttingPlanesToPolyData();

                if (planeData && planeData->GetNumberOfPoints() > 0) {
                    // 为切割平面设置渲染选项
                    VTKViewer::RenderOptions planeOptions;
                    planeOptions.surfaceOpacity = 0.3;  // 半透明
                    planeOptions.surfaceColor[0] = 0.2; // 青蓝色
                    planeOptions.surfaceColor[1] = 0.7;
                    planeOptions.surfaceColor[2] = 0.9;
                    planeOptions.showWireframe = false; // 不显示线框
                    planeOptions.showNormals = false;   // 不显示法线

                    // 添加切割平面到视图
                    vtkViewer.addPolyData(planeData, planeOptions);
                }
            }

            // 第三步：为可见面生成路径
            std::cout << "开始为可见面生成路径..." << std::endl;

            // 生成路径
            if (processor.generatePaths()) {
                // 获取生成的路径
                const std::vector<SprayPath>& paths = processor.getPaths();
                std::cout << "成功生成 " << paths.size() << " 条路径" << std::endl;

                if (paths.size() > 500) {
                    QMessageBox::warning(this, "路径数量过多",
                                       "生成了 " + QString::number(paths.size()) + " 条路径，这可能导致性能问题。\n"
                                       "建议增加路径间距或仅处理部分面。\n"
                                       "是否继续？",
                                       QMessageBox::Yes | QMessageBox::No);
                }

                // 第四步：整合轨迹
                std::cout << "开始整合轨迹..." << std::endl;
                if (processor.integrateTrajectories()) {
                    const std::vector<IntegratedTrajectory>& trajectories = processor.getIntegratedTrajectories();
                    std::cout << "成功整合为 " << trajectories.size() << " 条连续轨迹" << std::endl;

                    // 第五步：路径级别的可见性分析（可选，用于进一步优化）
                    std::cout << "开始路径级别的可见性分析..." << std::endl;
                    if (processor.analyzePathVisibility()) {
                        const std::vector<SurfaceLayer>& layers = processor.getSurfaceLayers();
                        std::cout << "可见性分析完成，识别出 " << layers.size() << " 个表面层级" << std::endl;

                        // 显示表层轨迹统计
                        if (!layers.empty()) {
                            std::cout << "路径级别可见性分析完成，最表层包含 " << layers[0].pathIndices.size() << " 条可见路径段" << std::endl;
                            std::cout << "已按Z+方向进行遮挡检测和智能路径分割" << std::endl;
                            std::cout << "保留了所有没被遮挡且长度≥20mm的路径段，删除了被遮挡和过短的部分" << std::endl;
                            std::cout << "颜色说明：绿色=喷涂路径，橙色=连接路径" << std::endl;
                        }
                    } else {
                        std::cout << "跳过路径级别的可见性分析，直接使用面级别的可见性结果" << std::endl;
                    }

                    // 只显示表层可见轨迹
                    vtkSmartPointer<vtkPolyData> integratedData = processor.integratedTrajectoriesToPolyData();

                    if (integratedData && integratedData->GetNumberOfPoints() > 0) {
                        VTKViewer::RenderOptions integratedOptions;
                        // 使用默认颜色，让VTK使用数据中的颜色信息
                        // 绿色=喷涂路径，橙色=连接路径
                        integratedOptions.surfaceOpacity = 1.0;   // 完全不透明
                        integratedOptions.showNormals = false;

                        try {
                            vtkViewer.addPolyData(integratedData, integratedOptions);
                            renderWindow->Render();
                            std::cout << "表层可见轨迹渲染完成!" << std::endl;
                        } catch (const std::exception& e) {
                            QMessageBox::critical(this, "渲染错误",
                                                QString("渲染表层轨迹时发生错误: %1").arg(e.what()));
                        } catch (...) {
                            QMessageBox::critical(this, "渲染错误", "渲染表层轨迹时发生未知错误");
                        }
                    } else {
                        QMessageBox::warning(this, "轨迹生成问题", "表层轨迹数据为空，无法显示。");
                    }
                } else {
                    std::cout << "轨迹整合失败，无法进行表面可见性分析" << std::endl;
                    QMessageBox::warning(this, "轨迹整合失败", "未能生成整合轨迹，请检查输入面或参数设置。");
                }
            } else {
                QMessageBox::warning(this, "路径生成失败", "未能生成任何路径，请检查输入面或参数设置。");
            }
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "处理错误",
                                QString("处理面时发生错误: %1").arg(e.what()));
        } catch (...) {
            QMessageBox::critical(this, "处理错误", "处理面时发生未知错误");
        }
    });
}

void Spray_GUI::updateAxes() {
    // 计算模型尺寸用于设置坐标轴大小
    if (!currentPoly) return;
    double bounds[6];
    currentPoly->GetBounds(bounds);
    double xLen = bounds[1] - bounds[0];
    double yLen = bounds[3] - bounds[2];
    double zLen = bounds[5] - bounds[4];
    double maxLenForAxes = std::max({ xLen, yLen, zLen });
    axes->SetTotalLength(maxLenForAxes / 6, maxLenForAxes / 6, maxLenForAxes / 6);
    axes->SetShaftTypeToCylinder();
    axes->SetCylinderRadius(0.01);
    axes->SetPosition(0, 0, 0);
    // 同步原点坐标轴长度
    if (originAxes) {
        originAxes->SetTotalLength(maxLenForAxes / 6, maxLenForAxes / 6, maxLenForAxes / 6);
        originAxes->SetShaftTypeToCylinder();
        originAxes->SetCylinderRadius(0.01);
        originAxes->SetPosition(0, 0, 0);
    }
}

