#include "OCCHandler.h"
#include <BRep_Tool.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <TopExp_Explorer.hxx>
#include <Poly_Triangulation.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx> // 添加顶点类型的完整定义
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkTriangle.h>
#include <vtkDoubleArray.h> // 添加此头文件，用于存储法向量
#include <STEPControl_Reader.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <iostream>
#include <map>
#include <vector>
#include <gp_Vec.hxx> // 添加此头文件
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <gp_Trsf.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx> // 用于形状类型的枚举
#include <TopoDS_Iterator.hxx>  // 用于遍历子形状
// 以下是新添加的头文件，用于修复功能
#include <TopTools_ListOfShape.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_MakeShell.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <ShapeFix_Shape.hxx>
#include <ShapeFix_Shell.hxx>
#include <ShapeFix_Solid.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <TopoDS_Compound.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
// 旋转相关
#include <gp_Ax1.hxx> // 用于定义旋转轴
// 表面和法向量相关
#include <BRepAdaptor_Surface.hxx>
#include <gp_Pln.hxx>
#include <TopoDS_Iterator.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <Geom_Surface.hxx>
#include <vtkCellData.h>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <GeomAPI_IntCS.hxx>
#include <Geom_Line.hxx>
#include <Geom_Plane.hxx>
#include <BRepAlgoAPI_Common.hxx>  // 用于布尔交运算
OCCHandler::OCCHandler() {
    // 构造函数实现
}

OCCHandler::~OCCHandler() {
    // 析构函数实现
}

bool OCCHandler::loadStepFile(const std::string& filename, bool moveToOrigin /*  = false */ ) {
    STEPControl_Reader reader;
    IFSelect_ReturnStatus status = reader.ReadFile(filename.c_str());
    if (status != IFSelect_RetDone) {
        std::cerr << "STEP文件加载失败: " << filename << std::endl;
        return false;
    }

    // 转换STEP实体到OCCT数据结构
    reader.TransferRoots();
    shape = reader.OneShape();

    if (shape.IsNull()) {
        std::cerr << "无法从STEP文件获取有效形状: " << filename << std::endl;
        return false;
    }

    // 如果需要，将模型移动到原点
    if (moveToOrigin) {
        this->moveShapeToOrigin();
    }

    return true;
}

TopoDS_Shape OCCHandler::getShape() const {
    return shape;
}

vtkSmartPointer<vtkPolyData> OCCHandler::shapeToPolyData() const {
    return shapeToPolyData(shape);
}

vtkSmartPointer<vtkPolyData> OCCHandler::shapeToPolyData(const TopoDS_Shape& shape /*  = shape */)  const {
    // 对形状进行三角剖分
    BRepMesh_IncrementalMesh mesher(shape, 0.5);
    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto triangles = vtkSmartPointer<vtkCellArray>::New();

    // 用于避免重复点的映射
    std::map<gp_Pnt, vtkIdType, bool(*)(const gp_Pnt&, const gp_Pnt&)> pointMap([](const gp_Pnt& a, const gp_Pnt& b){
        if (a.X() != b.X()) return a.X() < b.X();
        if (a.Y() != b.Y()) return a.Y() < b.Y();
        return a.Z() < b.Z();
    });

    // 创建一个数组用于存储单元法向量
    vtkSmartPointer<vtkDoubleArray> cellNormals = vtkSmartPointer<vtkDoubleArray>::New();
    cellNormals->SetNumberOfComponents(3);
    cellNormals->SetName("Normals");

    // 遍历所有面
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
        TopoDS_Face face = TopoDS::Face(exp.Current());
        TopLoc_Location loc;
        Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);

        if (tri.IsNull()) continue;

        int nbNodes = tri->NbNodes();
        int nbTriangles = tri->NbTriangles();

        // 创建OCCT点索引到VTK点索引的映射
        std::vector<vtkIdType> occ2vtk(nbNodes + 1, -1);

        // 添加点
        for (int i = 1; i <= nbNodes; ++i) {
            gp_Pnt p = tri->Node(i);
            // 应用位置变换
            p.Transform(loc.Transformation());

            // 避免重复点
            auto it = pointMap.find(p);
            vtkIdType pid;
            if (it == pointMap.end()) {
                pid = points->InsertNextPoint(p.X(), p.Y(), p.Z());
                pointMap[p] = pid;
            } else {
                pid = it->second;
            }
            occ2vtk[i] = pid;
        }

        // 计算面的几何法向量（不考虑方向）
        BRepAdaptor_Surface surface(face);
        gp_Dir geometricNormal;
        bool isPlane = (surface.GetType() == GeomAbs_Plane);

        if (isPlane) {
            // 对于平面，直接获取法向量
            gp_Pln plane = surface.Plane();
            geometricNormal = plane.Axis().Direction();
        } else {
            // 对于非平面，计算参数中点的法向量
            double uMin, uMax, vMin, vMax;
            uMin = surface.Surface().FirstUParameter();
            uMax = surface.Surface().LastUParameter();
            vMin = surface.Surface().FirstVParameter();
            vMax = surface.Surface().LastVParameter();

            double uMid = (uMin + uMax) / 2.0;
            double vMid = (vMin + vMax) / 2.0;

            gp_Pnt point;
            gp_Vec d1u, d1v;
            surface.D1(uMid, vMid, point, d1u, d1v);
            gp_Vec normal = d1u.Crossed(d1v);
            if (normal.Magnitude() > 1e-7) {
                normal.Normalize();
                geometricNormal = gp_Dir(normal);
            } else {
                geometricNormal = gp_Dir(0, 0, 1); // 默认法向量
            }
        }

        // 添加三角形并保存法向量，根据面的拓扑方向确定最终法向量方向
        for (int i = 1; i <= nbTriangles; ++i) {
            Poly_Triangle triangle = tri->Triangle(i);
            Standard_Integer n1, n2, n3;
            triangle.Get(n1, n2, n3);

            // 创建三角形
            vtkIdType ids[3] = { occ2vtk[n1], occ2vtk[n2], occ2vtk[n3] };

            // 获取三角形的顶点坐标
            double p1[3], p2[3], p3[3];
            points->GetPoint(ids[0], p1);
            points->GetPoint(ids[1], p2);
            points->GetPoint(ids[2], p3);

            // 根据面的拓扑方向调整顶点顺序
            if (face.Orientation() == TopAbs_REVERSED) {
                // 如果面是反向的，交换顶点顺序以反转法向量
                std::swap(ids[1], ids[2]);
            }

            // 添加三角形到单元数组
            triangles->InsertNextCell(3, ids);

            // 计算并存储最终的法向量（考虑了拓扑方向）
            double finalNormal[3] = {geometricNormal.X(), geometricNormal.Y(), geometricNormal.Z()};

            // 如果面是反向的，则反转法向量
            if (face.Orientation() == TopAbs_REVERSED) {
                finalNormal[0] = -finalNormal[0];
                finalNormal[1] = -finalNormal[1];
                finalNormal[2] = -finalNormal[2];
            }

            cellNormals->InsertNextTuple(finalNormal);
        }
    }

    polyData->SetPoints(points);
    polyData->SetPolys(triangles);
    polyData->GetCellData()->SetNormals(cellNormals); // 为PolyData设置单元法向量

    return polyData;
}

void OCCHandler::moveShapeToOrigin() {
    if (shape.IsNull()) {
        // 如果形状为空，直接返回
        return;
    }

    // Step 1: 创建并计算边界框
    Bnd_Box boundingBox;
    BRepBndLib::Add(shape, boundingBox);

    // Step 2: 获取边界框的中心点
    Standard_Real xMin, yMin, zMin, xMax, yMax, zMax;
    boundingBox.Get(xMin, yMin, zMin, xMax, yMax, zMax);
    gp_Pnt center(xMin, yMin, zMin);

    // Step 3: 创建平移变换
    gp_Vec translationVector(-center.X(), -center.Y(), -center.Z());
    gp_Trsf transformation;
    transformation.SetTranslation(translationVector); // 使用 gp_Vec 的形式

    // Step 4: 应用变换到形状
    TopoDS_Shape transformedShape = BRepBuilderAPI_Transform(shape, transformation).Shape();

    // 更新成员变量为平移后的形状
    shape = transformedShape;
}

std::string OCCHandler::getShapeTypeString(const TopAbs_ShapeEnum& shapeType) const {
    switch (shapeType) {
        case TopAbs_COMPOUND:   return "COMPOUND";
        case TopAbs_COMPSOLID:  return "COMPSOLID";
        case TopAbs_SOLID:      return "SOLID";
        case TopAbs_SHELL:      return "SHELL";
        case TopAbs_FACE:       return "FACE";
        case TopAbs_WIRE:       return "WIRE";
        case TopAbs_EDGE:       return "EDGE";
        case TopAbs_VERTEX:     return "VERTEX";
        case TopAbs_SHAPE:      return "SHAPE";
        default:                return "UNKNOWN";
    }
}

void OCCHandler::printShapeStructure(const TopoDS_Shape& shapeArg, TopAbs_ShapeEnum stopAtType, std::ostream& out, int indent) const {
    // 使用传入的形状或者使用内部存储的形状
    TopoDS_Shape shapeToUse = shapeArg.IsNull() ? shape : shapeArg;

    if (shapeToUse.IsNull()) {
        out << "No shape loaded" << std::endl;
        return;
    }

    // 打印缩进
    std::string indentStr(indent * 2, ' ');

    // 获取形状类型
    TopAbs_ShapeEnum shapeType = shapeToUse.ShapeType();
    std::string typeStr = getShapeTypeString(shapeType);

    // 打印当前形状的类型
    out << indentStr << "|- " << typeStr;

    // 获取形状的方向（正向、反向或内部）
    if (shapeToUse.Orientation() == TopAbs_REVERSED) {
        out << " (REVERSED)";
    } else if (shapeToUse.Orientation() == TopAbs_INTERNAL) {
        out << " (INTERNAL)";
    } else if (shapeToUse.Orientation() == TopAbs_EXTERNAL) {
        out << " (EXTERNAL)";
    }

    // 如果是面，显示面积
    if (shapeType == TopAbs_FACE) {
        TopoDS_Face face = TopoDS::Face(shapeToUse);
        // 获取面积
        GProp_GProps props;
        BRepGProp::SurfaceProperties(face, props);
        double area = props.Mass();
        out << " (Area: " << area << " sq units)";
    }
    // 如果是边，显示长度
    else if (shapeType == TopAbs_EDGE) {
        TopoDS_Edge edge = TopoDS::Edge(shapeToUse);
        // 获取长度
        GProp_GProps props;
        BRepGProp::LinearProperties(edge, props);
        double length = props.Mass();
        out << " (Length: " << length << " units)";
    }
    // 如果是顶点，显示坐标
    else if (shapeType == TopAbs_VERTEX) {
        TopoDS_Vertex vertex = TopoDS::Vertex(shapeToUse);
        gp_Pnt point = BRep_Tool::Pnt(vertex);
        out << " (Coords: " << point.X() << ", " << point.Y() << ", " << point.Z() << ")";
    }

    out << std::endl;

    // 检查是否达到指定的停止类型
    // 注意：在TopAbs中，数值越小表示层级越高
    // COMPOUND(0) > COMPSOLID(1) > SOLID(2) > SHELL(3) > FACE(4) > WIRE(5) > EDGE(6) > VERTEX(7)
    // 只有当当前形状类型大于停止类型时才继续递归
    if (stopAtType == TopAbs_SHAPE || shapeType < stopAtType) {
        // 递归打印子形状
        TopoDS_Iterator it(shapeToUse, Standard_True, Standard_True);
        for (; it.More(); it.Next()) {
            printShapeStructure(it.Value(), stopAtType, out, indent + 1);
        }
    } else if (indent == 0 || (indent > 0 && shapeType == stopAtType)) {
        // 如果当前形状类型是指定的终止类型，打印省略号
        std::string nextIndentStr((indent + 1) * 2, ' ');
        out << nextIndentStr << "|- ..." << std::endl;
    }

    // 打印形状的主要成分计数（仅针对顶层形状）
    if (indent == 0) {
        out << "\n--- 形状统计 ---" << std::endl;

        int solidCount = 0, shellCount = 0, faceCount = 0,
            wireCount = 0, edgeCount = 0, vertexCount = 0;

        // 统计不同类型的子形状数量
        for (TopExp_Explorer expSolid(shapeToUse, TopAbs_SOLID); expSolid.More(); expSolid.Next()) solidCount++;
        for (TopExp_Explorer expShell(shapeToUse, TopAbs_SHELL); expShell.More(); expShell.Next()) shellCount++;
        for (TopExp_Explorer expFace(shapeToUse, TopAbs_FACE); expFace.More(); expFace.Next()) faceCount++;
        for (TopExp_Explorer expWire(shapeToUse, TopAbs_WIRE); expWire.More(); expWire.Next()) wireCount++;
        for (TopExp_Explorer expEdge(shapeToUse, TopAbs_EDGE); expEdge.More(); expEdge.Next()) edgeCount++;
        for (TopExp_Explorer expVertex(shapeToUse, TopAbs_VERTEX); expVertex.More(); expVertex.Next()) vertexCount++;

        out << "实体(SOLID): " << solidCount << std::endl;
        out << "壳(SHELL): " << shellCount << std::endl;
        out << "面(FACE): " << faceCount << std::endl;
        out << "线环(WIRE): " << wireCount << std::endl;
        out << "边(EDGE): " << edgeCount << std::endl;
        out << "点(VERTEX): " << vertexCount << std::endl;

        // 计算并打印边界盒信息
        Bnd_Box boundingBox;
        BRepBndLib::Add(shapeToUse, boundingBox);
        Standard_Real xMin, yMin, zMin, xMax, yMax, zMax;
        boundingBox.Get(xMin, yMin, zMin, xMax, yMax, zMax);

        out << "\n--- 边界盒信息 ---" << std::endl;
        out << "X范围: " << xMin << " 到 " << xMax << " (长度: " << (xMax - xMin) << ")" << std::endl;
        out << "Y范围: " << yMin << " 到 " << yMax << " (长度: " << (yMax - yMin) << ")" << std::endl;
        out << "Z范围: " << zMin << " 到 " << zMax << " (长度: " << (zMax - zMin) << ")" << std::endl;
        out << "体积: " << (xMax - xMin) * (yMax - yMin) * (zMax - zMin) << " 立方单位" << std::endl;
    }
}

// 绕指定坐标轴旋转90度（轴：axis）
void OCCHandler::rotate90(gp_Dir axis) {
    if (shape.IsNull()) {
        std::cerr << "没有可旋转的模型" << std::endl;
        return;
    }

    // 创建旋转变换
    gp_Trsf transformation;
    gp_Ax1 rotationAxis;

    // 根据输入的轴设置旋转轴

    rotationAxis = gp_Ax1(gp_Pnt(0, 0, 0), axis);


    // 设置旋转变换（90度转换为弧度）
    transformation.SetRotation(rotationAxis, M_PI / 2.0);

    // 应用变换
    BRepBuilderAPI_Transform transformer(shape, transformation);
    shape = transformer.Shape();

    // 将模型移动到原点
    moveShapeToOrigin();
}

// 计算shell的主法向量方向
gp_Dir OCCHandler::calculateShellMainNormal(const TopoDS_Shell& shell) const {
    // 用于累加所有面的法向量
    gp_Vec averageNormal(0, 0, 0);
    double totalArea = 0.0;

    // 遍历shell中的所有面
    for (TopExp_Explorer explorer(shell, TopAbs_FACE); explorer.More(); explorer.Next()) {
        TopoDS_Face face = TopoDS::Face(explorer.Current());

        // 获取面的法向量
        BRepAdaptor_Surface surface(face);

        // 获取面积
        GProp_GProps props;
        BRepGProp::SurfaceProperties(face, props);
        double area = props.Mass();

        // 面积很小的面对主法向量的贡献可以忽略
        if (area < 1e-6) continue;

        // 计算面的几何法向量（不考虑方向）
        gp_Dir geometricNormal;
        bool isPlane = (surface.GetType() == GeomAbs_Plane);

        if (isPlane) {
            // 对于平面，直接获取法向量
            gp_Pln plane = surface.Plane();
            geometricNormal = plane.Axis().Direction();
        } else {
            // 对于非平面，计算参数中点的法向量
            double uMin, uMax, vMin, vMax;
            uMin = surface.Surface().FirstUParameter();
            uMax = surface.Surface().LastUParameter();
            vMin = surface.Surface().FirstVParameter();
            vMax = surface.Surface().LastVParameter();

            double uMid = (uMin + uMax) / 2.0;
            double vMid = (vMin + vMax) / 2.0;

            gp_Pnt point;
            gp_Vec d1u, d1v;
            surface.D1(uMid, vMid, point, d1u, d1v);
            gp_Vec normal = d1u.Crossed(d1v);
            if (normal.Magnitude() > 1e-7) {
                normal.Normalize();
                geometricNormal = gp_Dir(normal);
            } else {
                continue;
            }
        }

        // 考虑面的拓扑方向，创建最终的法向量
        gp_Vec finalNormal(geometricNormal.X(), geometricNormal.Y(), geometricNormal.Z());

        // 如果面的方向是反向的，则反转法向量
        if (face.Orientation() == TopAbs_REVERSED) {
            finalNormal.Reverse();
        }

        // 使用面积作为权重，累加面的法向量
        averageNormal += finalNormal * area;
        totalArea += area;
    }

    // 如果没有有效面，返回Z轴方向作为默认值
    if (totalArea < 1e-6) {
        return gp_Dir(0, 0, 1);
    }

    // 计算加权平均法向量
    averageNormal /= totalArea;

    // 如果法向量接近零向量，返回Z轴方向作为默认值
    if (averageNormal.Magnitude() < 1e-7) {
        return gp_Dir(0, 0, 1);
    }

    // 标准化并返回结果
    return gp_Dir(averageNormal);
}

// 基于法向量方向提取shell并创建新形状
TopoDS_Shape OCCHandler::extractShellsByNormal(const gp_Dir& direction, double angleTolerance, bool returnExtracted) {
    if (shape.IsNull()) {
        std::cerr << "没有加载模型，无法提取shell" << std::endl;
        return TopoDS_Shape();
    }

    // 角度容差（从度转换为弧度）
    double angleToleranceRad = angleTolerance * M_PI / 180.0;
    double cosAngleTolerance = cos(angleToleranceRad);

    // 创建两个列表，分别用于存储符合条件的shell和不符合条件的shell
    TopTools_ListOfShape matchingShells;
    TopTools_ListOfShape nonMatchingShells;

    // 先提取所有shell
    TopTools_ListOfShape allShells = extractAllShells();

    // 遍历所有shell，根据法向量进行分类
    for (TopTools_ListIteratorOfListOfShape it(allShells); it.More(); it.Next()) {
        TopoDS_Shell shell = TopoDS::Shell(it.Value());
        gp_Dir shellNormal = calculateShellMainNormal(shell);
        double dotProduct = shellNormal.Dot(direction);

        if (dotProduct > cosAngleTolerance) {
            matchingShells.Append(shell);
        } else {
            nonMatchingShells.Append(shell);
        }
    }

    // 如果没有找到任何符合条件的shell，输出警告
    if (matchingShells.IsEmpty() && nonMatchingShells.IsEmpty()) {
        std::cerr << "模型中没有找到任何shell" << std::endl;
        return TopoDS_Shape();
    }

    // 根据returnExtracted参数决定返回哪组shell
    TopTools_ListOfShape& shellsToUse = returnExtracted ? matchingShells : nonMatchingShells;

    // 如果没有符合条件的shell，输出信息
    if (shellsToUse.IsEmpty()) {
        std::cerr << "没有" << (returnExtracted ? "符合" : "不符合") << "条件的shell" << std::endl;
        return TopoDS_Shape();
    }

    // 输出提取的shell数量
    std::cout << "找到 " << shellsToUse.Extent() << " 个"
              << (returnExtracted ? "符合" : "不符合") << "条件的shell" << std::endl;

    // 从选择的shell创建新形状
    return createShapeFromShells(shellsToUse);
}

// 创建一个只包含指定shell的新形状
TopoDS_Shape OCCHandler::createShapeFromShells(const TopTools_ListOfShape& shells) {
    if (shells.IsEmpty()) {
        std::cerr << "没有shell可用于创建形状" << std::endl;
        return TopoDS_Shape();
    }

    // 直接创建一个复合体，不尝试创建实体
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);

    // 将所有shell添加到复合体中
    for (TopTools_ListIteratorOfListOfShape it(shells); it.More(); it.Next()) {
        builder.Add(compound, it.Value());
    }

    std::cout << "创建了包含 " << shells.Extent() << " 个shell的复合体" << std::endl;

    return compound;
}

// 提取形状中的所有shell
TopTools_ListOfShape OCCHandler::extractAllShells(const TopoDS_Shape& sourceShape) {
    // 使用传入的形状或者使用内部存储的形状
    TopoDS_Shape shapeToUse = sourceShape.IsNull() ? shape : sourceShape;

    TopTools_ListOfShape shells;

    if (shapeToUse.IsNull()) {
        std::cerr << "没有可用的模型，无法提取shell" << std::endl;
        return shells;
    }

    // 递归处理形状及其子形状
    std::function<void(const TopoDS_Shape&)> processShape;
    processShape = [&](const TopoDS_Shape& currentShape) {
        if (currentShape.IsNull()) return;

        // 根据形状类型进行不同处理
        TopAbs_ShapeEnum shapeType = currentShape.ShapeType();

        if (shapeType == TopAbs_SHELL) {
            // 如果是shell，直接添加
            shells.Append(currentShape);
        }
        else if (shapeType == TopAbs_SOLID) {
            // 处理实体：查找其中的shell
            for (TopExp_Explorer explorer(currentShape, TopAbs_SHELL); explorer.More(); explorer.Next()) {
                shells.Append(explorer.Current());
            }
        }
        else if (shapeType == TopAbs_COMPOUND || shapeType == TopAbs_COMPSOLID) {
            // 对复合体的每个子形状递归处理
            for (TopoDS_Iterator it(currentShape); it.More(); it.Next()) {
                processShape(it.Value());
            }
        }
        else {
            // 对于其他类型，查找是否包含shell
            for (TopExp_Explorer explorer(currentShape, TopAbs_SHELL); explorer.More(); explorer.Next()) {
                shells.Append(explorer.Current());
            }
        }
    };

    // 开始处理形状
    processShape(shapeToUse);

    std::cout << "从模型中提取了 " << shells.Extent() << " 个shell" << std::endl;

    return shells;
}

// 提取形状中的所有面
TopTools_ListOfShape OCCHandler::extractAllFaces(const TopoDS_Shape& sourceShape) {
    // 使用传入的形状或者使用内部存储的形状
    TopoDS_Shape shapeToUse = sourceShape.IsNull() ? shape : sourceShape;

    TopTools_ListOfShape faces;

    if (shapeToUse.IsNull()) {
        std::cerr << "没有可用的模型，无法提取面" << std::endl;
        return faces;
    }

    // 递归处理形状及其子形状
    std::function<void(const TopoDS_Shape&)> processShape;
    processShape = [&](const TopoDS_Shape& currentShape) {
        if (currentShape.IsNull()) return;

        // 根据形状类型进行不同处理
        TopAbs_ShapeEnum shapeType = currentShape.ShapeType();

        if (shapeType == TopAbs_FACE) {
            // 如果是面，直接添加
            faces.Append(currentShape);
        }
        else if (shapeType == TopAbs_SHELL || shapeType == TopAbs_SOLID ||
                 shapeType == TopAbs_COMPOUND || shapeType == TopAbs_COMPSOLID) {
            // 对复合体、实体、壳等的每个子形状递归处理
            for (TopoDS_Iterator it(currentShape); it.More(); it.Next()) {
                processShape(it.Value());
            }
        }
    };

    // 开始处理形状
    processShape(shapeToUse);

    std::cout << "从模型中提取了 " << faces.Extent() << " 个面" << std::endl;

    return faces;
}

// 基于法向量方向提取面并创建新形状
TopoDS_Shape OCCHandler::extractFacesByNormal(const gp_Dir& direction, double angleTolerance, bool returnExtracted) {
    if (shape.IsNull()) {
        std::cerr << "没有加载模型，无法提取面" << std::endl;
        return TopoDS_Shape();
    }

    // 角度容差（从度转换为弧度）
    double angleToleranceRad = angleTolerance * M_PI / 180.0;
    double cosAngleTolerance = cos(angleToleranceRad);

    // 创建两个列表，分别用于存储符合条件的面和不符合条件的面
    TopTools_ListOfShape matchingFaces;
    TopTools_ListOfShape nonMatchingFaces;

    // 先提取所有面
    TopTools_ListOfShape allFaces = extractAllFaces();

    // 遍历所有面，根据法向量进行分类
    for (TopTools_ListIteratorOfListOfShape it(allFaces); it.More(); it.Next()) {
        TopoDS_Face face = TopoDS::Face(it.Value());

        // 计算面的法向量
        BRepAdaptor_Surface surface(face);
        gp_Dir faceNormal;
        bool isPlane = (surface.GetType() == GeomAbs_Plane);

        if (isPlane) {
            // 对于平面，直接获取法向量
            gp_Pln plane = surface.Plane();
            faceNormal = plane.Axis().Direction();
        } else {
            // 对于非平面，计算参数中点的法向量
            double uMin, uMax, vMin, vMax;
            uMin = surface.Surface().FirstUParameter();
            uMax = surface.Surface().LastUParameter();
            vMin = surface.Surface().FirstVParameter();
            vMax = surface.Surface().LastVParameter();

            double uMid = (uMin + uMax) / 2.0;
            double vMid = (vMin + vMax) / 2.0;

            gp_Pnt point;
            gp_Vec d1u, d1v;
            surface.D1(uMid, vMid, point, d1u, d1v);
            gp_Vec normal = d1u.Crossed(d1v);
            if (normal.Magnitude() > 1e-7) {
                normal.Normalize();
                faceNormal = gp_Dir(normal);
            } else {
                // 如果无法计算法向量，使用默认值并跳过此面
                continue;
            }
        }

        // 考虑面的拓扑方向
        if (face.Orientation() == TopAbs_REVERSED) {
            faceNormal.Reverse();
        }

        // 计算面法向量与参考方向的点积
        double dotProduct = faceNormal.Dot(direction);

        // 根据点积决定面是否符合条件
        if (dotProduct > cosAngleTolerance) {
            matchingFaces.Append(face);
        } else {
            nonMatchingFaces.Append(face);
        }
    }

    // 如果没有找到任何符合条件的面，输出警告
    if (matchingFaces.IsEmpty() && nonMatchingFaces.IsEmpty()) {
        std::cerr << "模型中没有找到任何面" << std::endl;
        return TopoDS_Shape();
    }

    // 根据returnExtracted参数决定返回哪组面
    TopTools_ListOfShape& facesToUse = returnExtracted ? matchingFaces : nonMatchingFaces;

    // 如果没有符合条件的面，输出信息
    if (facesToUse.IsEmpty()) {
        std::cerr << "没有" << (returnExtracted ? "符合" : "不符合") << "条件的面" << std::endl;
        return TopoDS_Shape();
    }

    // 输出提取的面数量
    std::cout << "找到 " << facesToUse.Extent() << " 个"
              << (returnExtracted ? "符合" : "不符合") << "条件的面" << std::endl;

    // 创建一个复合体包含选定的面
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);

    for (TopTools_ListIteratorOfListOfShape it(facesToUse); it.More(); it.Next()) {
        builder.Add(compound, it.Value());
    }

    std::cout << "创建了包含 " << facesToUse.Extent() << " 个面的复合体" << std::endl;
    return compound;
}


