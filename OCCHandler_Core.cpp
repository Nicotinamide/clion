#include "OCCHandler.h"
#include <STEPControl_Reader.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <iostream>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <gp_Trsf.hxx>
#include <gp_Ax1.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Iterator.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>

// 构造函数
OCCHandler::OCCHandler() {
    // 初始化代码（如果需要）
}

// 析构函数
OCCHandler::~OCCHandler() {
    // 清理代码（如果需要）
}

// 加载STEP文件
bool OCCHandler::loadStepFile(const std::string& filename, bool moveToOrigin, bool autoRepair) {
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

    std::cout << "✅ STEP文件加载成功: " << filename << std::endl;

    // 如果需要，自动修复模型
    if (autoRepair) {
        std::cout << "🔧 开始自动修复导入的模型..." << std::endl;
        
        // 使用增强的修复功能
        bool repairSuccess = sprayTrajectoryOptimizedRepair(1e-3, true);
        if (repairSuccess) {
            std::cout << "✅ 喷涂轨迹优化修复完成" << std::endl;
        } else {
            std::cout << "⚠️ 尝试基本修复..." << std::endl;
            repairSuccess = enhancedModelRepair(1e-6, true);
            if (repairSuccess) {
                std::cout << "✅ 增强模型修复完成" << std::endl;
            } else {
                std::cout << "⚠️ 使用基础修复..." << std::endl;
                repairSuccess = repairImportedModel(1e-6, true);
                if (repairSuccess) {
                    std::cout << "✅ 基础模型修复完成" << std::endl;
                } else {
                    std::cout << "⚠️ 模型修复过程中出现问题，但模型仍可使用" << std::endl;
                }
            }
        }
    }

    // 如果需要，将模型移动到原点
    if (moveToOrigin) {
        std::cout << "📍 将模型移动到原点..." << std::endl;
        this->moveShapeToOrigin();
        std::cout << "✅ 模型已移动到原点" << std::endl;
    }

    return true;
}

// 获取当前模型
TopoDS_Shape OCCHandler::getShape() const {
    return shape;
}

// 移动模型到原点
void OCCHandler::moveShapeToOrigin() {
    if (shape.IsNull()) {
        std::cerr << "没有加载模型，无法移动到原点" << std::endl;
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

// 绕指定坐标轴旋转90度
void OCCHandler::rotate90(gp_Dir axis) {
    if (shape.IsNull()) {
        std::cerr << "没有加载模型，无法旋转" << std::endl;
        return;
    }

    // 创建旋转变换（90度 = π/2弧度）
    gp_Trsf rotation;
    rotation.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), axis), M_PI / 2.0);
    
    // 应用变换
    BRepBuilderAPI_Transform transformer(shape, rotation);
    shape = transformer.Shape();
    
    std::cout << "模型已绕指定轴旋转90度" << std::endl;
}

// 打印TopoDS_Shape结构
void OCCHandler::printShapeStructure(const TopoDS_Shape& shapeArg,
                                   TopAbs_ShapeEnum stopAtType, 
                                   std::ostream& out, 
                                   int indent) const {
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

// 获取形状类型的字符串表示
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

// 提取形状中的所有shell
TopTools_ListOfShape OCCHandler::extractAllShells(const TopoDS_Shape& sourceShape) {
    TopTools_ListOfShape shells;
    
    // 使用传入的形状，如果为空则使用当前形状
    TopoDS_Shape shapeToUse = sourceShape.IsNull() ? shape : sourceShape;
    
    if (shapeToUse.IsNull()) {
        std::cerr << "没有加载模型，无法提取shell" << std::endl;
        return shells;
    }

    // 遍历所有shell
    for (TopExp_Explorer shellExplorer(shapeToUse, TopAbs_SHELL); shellExplorer.More(); shellExplorer.Next()) {
        shells.Append(shellExplorer.Current());
    }

    std::cout << "从模型中提取了 " << shells.Extent() << " 个shell" << std::endl;
    return shells;
}

// 创建一个只包含指定shell的新形状
TopoDS_Shape OCCHandler::createShapeFromShells(const TopTools_ListOfShape& shells) {
    if (shells.IsEmpty()) {
        std::cerr << "shell列表为空，无法创建形状" << std::endl;
        return TopoDS_Shape();
    }

    try {
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);

        // 将所有shell添加到复合体中
        for (TopTools_ListIteratorOfListOfShape it(shells); it.More(); it.Next()) {
            builder.Add(compound, it.Value());
        }

        std::cout << "成功创建包含 " << shells.Extent() << " 个shell的形状" << std::endl;
        return compound;
    } catch (...) {
        std::cerr << "❌ 创建shell形状时发生异常" << std::endl;
        return TopoDS_Shape();
    }
}
