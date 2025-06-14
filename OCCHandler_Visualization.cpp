#include "OCCHandler.h"
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <TopExp_Explorer.hxx>
#include <Poly_Triangulation.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopLoc_Location.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <Poly_Array1OfTriangle.hxx>
#include <Poly_Triangle.hxx>
#include <vtkSmartPointer.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkDoubleArray.h>
#include <vtkCellData.h>
#include <vtkPolyData.h>
#include <vtkTriangle.h>
#include <vtkCell.h>
#include <gp_Vec.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <iostream>
#include <map>

// TopoDS_Shape转vtkPolyData（无默认参数）
vtkSmartPointer<vtkPolyData> OCCHandler::shapeToPolyData() const {
    return shapeToPolyData(shape);
}

// TopoDS_Shape转vtkPolyData（带参数）
vtkSmartPointer<vtkPolyData> OCCHandler::shapeToPolyData(const TopoDS_Shape& shape) const {
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

// 计算shell的主法向量方向
gp_Dir OCCHandler::calculateShellMainNormal(const TopoDS_Shell& shell) const {
    if (shell.IsNull()) {
        std::cerr << "⚠️ Shell为空，返回默认法向量" << std::endl;
        return gp_Dir(0, 0, 1);
    }

    gp_Vec averageNormal(0, 0, 0);
    double totalArea = 0.0;

    // 遍历shell中的所有面
    for (TopExp_Explorer faceExplorer(shell, TopAbs_FACE); faceExplorer.More(); faceExplorer.Next()) {
        TopoDS_Face face = TopoDS::Face(faceExplorer.Current());

        double area = 0.0;
        gp_Vec geometricNormal;

        try {
            // 计算面的面积
            GProp_GProps props;
            BRepGProp::SurfaceProperties(face, props);
            area = props.Mass();

            if (area < 1e-10) {
                continue; // 跳过面积过小的面
            }

            // 计算面的几何法向量
            BRepAdaptor_Surface surfaceAdaptor(face);

            // 获取面的参数范围
            Standard_Real uMin, uMax, vMin, vMax;
            BRepTools::UVBounds(face, uMin, uMax, vMin, vMax);

            // 在面的中心点计算法向量
            Standard_Real uMid = (uMin + uMax) / 2.0;
            Standard_Real vMid = (vMin + vMax) / 2.0;

            gp_Pnt point;
            gp_Vec du, dv;
            surfaceAdaptor.D1(uMid, vMid, point, du, dv);

            // 计算几何法向量（du × dv）
            geometricNormal = du.Crossed(dv);
            if (geometricNormal.Magnitude() < 1e-10) {
                continue; // 跳过法向量计算失败的面
            }

            geometricNormal.Normalize();
        } catch (...) {
            continue; // 跳过计算失败的面
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
