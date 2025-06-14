#include "OCCHandler.h"
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Iterator.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <gp_Pln.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <Geom_Surface.hxx>
#include <iostream>
#include <map>
#include <algorithm>
#include <functional>

// 提取形状中的所有面
TopTools_ListOfShape OCCHandler::extractAllFaces(const TopoDS_Shape& sourceShape) {
    TopTools_ListOfShape faces;
    
    // 使用传入的形状，如果为空则使用当前形状
    TopoDS_Shape shapeToUse = sourceShape.IsNull() ? shape : sourceShape;
    
    if (shapeToUse.IsNull()) {
        std::cerr << "没有加载模型，无法提取面" << std::endl;
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

    // 创建包含这些面的新形状
    try {
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);

        for (TopTools_ListIteratorOfListOfShape it(facesToUse); it.More(); it.Next()) {
            builder.Add(compound, it.Value());
        }

        return compound;
    } catch (...) {
        std::cerr << "❌ 创建面形状时发生异常" << std::endl;
        return TopoDS_Shape();
    }
}

// 按Z高度对面进行分层
std::map<double, TopTools_ListOfShape> OCCHandler::groupFacesByHeight(const TopTools_ListOfShape& faces, double heightTolerance) const {
    std::map<double, TopTools_ListOfShape> layeredFaces;

    if (faces.IsEmpty()) {
        std::cout << "⚠️ 输入的面列表为空" << std::endl;
        return layeredFaces;
    }

    std::cout << "🔄 开始按Z高度对 " << faces.Extent() << " 个面进行分层..." << std::endl;
    std::cout << "📏 高度容差: " << heightTolerance << std::endl;

    // 遍历所有面，计算其Z高度并分组
    for (TopTools_ListIteratorOfListOfShape it(faces); it.More(); it.Next()) {
        TopoDS_Face face = TopoDS::Face(it.Value());
        double faceHeight = calculateFaceHeight(face);

        // 查找是否已有相近高度的分组
        bool foundGroup = false;
        for (auto& pair : layeredFaces) {
            if (std::abs(pair.first - faceHeight) <= heightTolerance) {
                pair.second.Append(face);
                foundGroup = true;
                break;
            }
        }

        // 如果没有找到相近的分组，创建新分组
        if (!foundGroup) {
            TopTools_ListOfShape newGroup;
            newGroup.Append(face);
            layeredFaces[faceHeight] = newGroup;
        }
    }

    // 输出分层结果
    std::cout << "📊 分层结果: 共 " << layeredFaces.size() << " 层" << std::endl;
    int layerIndex = 1;
    for (const auto& pair : layeredFaces) {
        std::cout << "   第 " << layerIndex << " 层 (Z=" << pair.first << "): " 
                  << pair.second.Extent() << " 个面" << std::endl;
        layerIndex++;
    }

    return layeredFaces;
}

// 计算面的Z高度（中心点Z坐标）
double OCCHandler::calculateFaceHeight(const TopoDS_Face& face) const {
    try {
        // 计算面的重心
        GProp_GProps props;
        BRepGProp::SurfaceProperties(face, props);
        gp_Pnt centroid = props.CentreOfMass();
        
        return centroid.Z();
    } catch (...) {
        std::cerr << "⚠️ 计算面高度时发生异常，返回默认值0" << std::endl;
        return 0.0;
    }
}

// 使用缝合算法将面组合成shell
TopoDS_Shape OCCHandler::sewFacesToShells(const TopTools_ListOfShape& faces, double tolerance) const {
    try {
        if (faces.IsEmpty()) {
            return TopoDS_Shape();
        }

        // 创建缝合器
        BRepBuilderAPI_Sewing sewing(tolerance);
        sewing.SetTolerance(tolerance);
        sewing.SetFaceMode(Standard_True);
        sewing.SetFloatingEdgesMode(Standard_False);
        sewing.SetNonManifoldMode(Standard_False);

        // 添加所有面到缝合器
        for (TopTools_ListIteratorOfListOfShape it(faces); it.More(); it.Next()) {
            sewing.Add(it.Value());
        }

        // 执行缝合
        sewing.Perform();
        TopoDS_Shape sewedShape = sewing.SewedShape();

        if (sewedShape.IsNull()) {
            std::cout << "⚠️ 缝合失败，返回空形状" << std::endl;
            return TopoDS_Shape();
        }

        // 统计缝合结果
        int shellCount = 0;
        int faceCount = 0;
        for (TopExp_Explorer shellExp(sewedShape, TopAbs_SHELL); shellExp.More(); shellExp.Next()) {
            shellCount++;
        }
        for (TopExp_Explorer faceExp(sewedShape, TopAbs_FACE); faceExp.More(); faceExp.Next()) {
            faceCount++;
        }

        std::cout << "✅ 缝合完成: " << faces.Extent() << " 个面 → " 
                  << shellCount << " 个shell, " << faceCount << " 个面" << std::endl;

        return sewedShape;

    } catch (const std::exception& e) {
        std::cerr << "❌ 缝合过程中发生异常: " << e.what() << std::endl;
        return TopoDS_Shape();
    } catch (...) {
        std::cerr << "❌ 缝合过程中发生未知异常" << std::endl;
        return TopoDS_Shape();
    }
}
