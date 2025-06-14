#include "OCCHandler.h"
#include <BRepCheck_Analyzer.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <ShapeFix_Shape.hxx>
#include <ShapeFix_Wire.hxx>
#include <ShapeFix_Face.hxx>
#include <ShapeFix_Edge.hxx>
#include <ShapeFix_Wireframe.hxx>
#include <ShapeFix_FixSmallFace.hxx>
#include <ShapeFix_ShapeTolerance.hxx>
#include <BRepTools.hxx>
#include <Precision.hxx>
// 形状分析相关
#include <ShapeAnalysis_Wire.hxx>
#include <ShapeAnalysis_Edge.hxx>
#include <ShapeAnalysis_Shell.hxx>
#include <ShapeAnalysis_FreeBounds.hxx>
#include <ShapeAnalysis_CheckSmallFace.hxx>
#include <ShapeAnalysis_ShapeTolerance.hxx>
#include <ShapeAnalysis_ShapeContents.hxx>
// 形状升级相关
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <ShapeUpgrade_RemoveInternalWires.hxx>
#include <ShapeUpgrade_ShapeDivideArea.hxx>
// 消息和状态相关
#include <ShapeExtend_MsgRegistrator.hxx>
#include <ShapeExtend_Status.hxx>
// 构建和重塑相关
#include <ShapeBuild_ReShape.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <iostream>

// 增强的模型修复函数（基于OCCT 7.9）
bool OCCHandler::enhancedModelRepair(double tolerance, bool verbose) {
    if (shape.IsNull()) {
        if (verbose) {
            std::cerr << "❌ 没有加载的模型，无法进行修复" << std::endl;
        }
        return false;
    }

    if (verbose) {
        std::cout << "🚀 开始增强模型修复（基于OCCT 7.9）..." << std::endl;
        std::cout << "📏 使用容差: " << tolerance << std::endl;
    }

    try {
        // 步骤1: 详细的形状分析
        if (verbose) {
            std::cout << "\n📊 步骤1: 详细形状分析..." << std::endl;
        }
        
        // 基本有效性检查
        BRepCheck_Analyzer analyzer(shape);
        bool isValid = analyzer.IsValid();
        
        // 容差分析
        ShapeAnalysis_ShapeTolerance toleranceAnalyzer;
        Standard_Real avgTolerance = toleranceAnalyzer.Tolerance(shape, 0);
        Standard_Real maxTolerance = toleranceAnalyzer.Tolerance(shape, 1);
        Standard_Real minTolerance = toleranceAnalyzer.Tolerance(shape, -1);
        
        if (verbose) {
            std::cout << "   基本有效性: " << (isValid ? "✅" : "❌") << std::endl;
            std::cout << "   平均容差: " << avgTolerance << std::endl;
            std::cout << "   最大容差: " << maxTolerance << std::endl;
            std::cout << "   最小容差: " << minTolerance << std::endl;
        }

        // 步骤2: 自由边界分析
        if (verbose) {
            std::cout << "\n🔍 步骤2: 自由边界分析..." << std::endl;
        }
        
        ShapeAnalysis_FreeBounds freeBoundsAnalyzer(shape);
        TopoDS_Compound closedWires = freeBoundsAnalyzer.GetClosedWires();
        TopoDS_Compound openWires = freeBoundsAnalyzer.GetOpenWires();
        
        // 统计自由边界
        int closedWireCount = 0, openWireCount = 0;
        for (TopExp_Explorer exp(closedWires, TopAbs_WIRE); exp.More(); exp.Next()) closedWireCount++;
        for (TopExp_Explorer exp(openWires, TopAbs_WIRE); exp.More(); exp.Next()) openWireCount++;
        
        if (verbose) {
            std::cout << "   封闭自由边界: " << closedWireCount << std::endl;
            std::cout << "   开放自由边界: " << openWireCount << std::endl;
        }

        // 步骤3: 小面检查
        if (verbose) {
            std::cout << "\n🔬 步骤3: 小面检查..." << std::endl;
        }
        
        ShapeAnalysis_CheckSmallFace smallFaceChecker;
        int smallFaceCount = 0;
        for (TopExp_Explorer faceExp(shape, TopAbs_FACE); faceExp.More(); faceExp.Next()) {
            TopoDS_Face face = TopoDS::Face(faceExp.Current());
            TopoDS_Edge edge1, edge2;
            if (smallFaceChecker.CheckSpotFace(face, tolerance) ||
                smallFaceChecker.CheckStripFace(face, edge1, edge2, tolerance)) {
                smallFaceCount++;
            }
        }
        
        if (verbose) {
            std::cout << "   发现小面数量: " << smallFaceCount << std::endl;
        }

        // 步骤4: 高级缝合修复
        if (verbose) {
            std::cout << "\n🧵 步骤4: 高级缝合修复..." << std::endl;
        }
        
        BRepBuilderAPI_Sewing sewing(tolerance);
        sewing.SetTolerance(tolerance);
        sewing.SetFaceMode(Standard_True);
        sewing.SetFloatingEdgesMode(Standard_True);
        sewing.SetNonManifoldMode(Standard_False);
        
        // 添加所有面
        int faceCount = 0;
        for (TopExp_Explorer faceExp(shape, TopAbs_FACE); faceExp.More(); faceExp.Next()) {
            sewing.Add(faceExp.Current());
            faceCount++;
        }
        
        if (verbose) {
            std::cout << "   处理 " << faceCount << " 个面..." << std::endl;
        }
        
        sewing.Perform();
        TopoDS_Shape sewedShape = sewing.SewedShape();
        
        if (!sewedShape.IsNull()) {
            shape = sewedShape;
            if (verbose) {
                std::cout << "✅ 高级缝合修复成功" << std::endl;
            }
        }

        // 步骤5: ShapeFix_Shape 全面修复
        if (verbose) {
            std::cout << "\n🛠️ 步骤5: 全面形状修复..." << std::endl;
        }
        
        Handle(ShapeFix_Shape) shapeFixer = new ShapeFix_Shape();
        shapeFixer->Init(shape);
        shapeFixer->SetPrecision(tolerance);
        shapeFixer->SetMaxTolerance(tolerance * 100);
        shapeFixer->SetMinTolerance(tolerance * 0.01);
        
        bool fixResult = shapeFixer->Perform();
        
        if (fixResult) {
            TopoDS_Shape fixedShape = shapeFixer->Shape();
            if (!fixedShape.IsNull()) {
                shape = fixedShape;
                if (verbose) {
                    std::cout << "✅ 全面形状修复成功" << std::endl;
                }
            }
        }

        // 步骤6: 最终验证
        if (verbose) {
            std::cout << "\n✔️ 步骤6: 最终验证..." << std::endl;
        }
        
        BRepCheck_Analyzer finalAnalyzer(shape);
        bool finalValid = finalAnalyzer.IsValid();
        
        // 统计最终结果
        int finalFaceCount = 0, finalEdgeCount = 0, finalVertexCount = 0;
        for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) finalFaceCount++;
        for (TopExp_Explorer exp(shape, TopAbs_EDGE); exp.More(); exp.Next()) finalEdgeCount++;
        for (TopExp_Explorer exp(shape, TopAbs_VERTEX); exp.More(); exp.Next()) finalVertexCount++;
        
        if (verbose) {
            std::cout << "📊 最终统计:" << std::endl;
            std::cout << "   面数量: " << finalFaceCount << std::endl;
            std::cout << "   边数量: " << finalEdgeCount << std::endl;
            std::cout << "   顶点数量: " << finalVertexCount << std::endl;
            std::cout << "   模型有效性: " << (finalValid ? "✅" : "⚠️") << std::endl;
            std::cout << "🎉 增强模型修复完成！" << std::endl;
        }

        return true;

    } catch (const std::exception& e) {
        if (verbose) {
            std::cerr << "❌ 增强修复过程中发生异常: " << e.what() << std::endl;
        }
        return false;
    } catch (...) {
        if (verbose) {
            std::cerr << "❌ 增强修复过程中发生未知异常" << std::endl;
        }
        return false;
    }
}

// 针对喷涂轨迹优化的修复
bool OCCHandler::sprayTrajectoryOptimizedRepair(double tolerance, bool verbose) {
    if (shape.IsNull()) {
        if (verbose) {
            std::cerr << "❌ 没有加载的模型，无法进行修复" << std::endl;
        }
        return false;
    }

    if (verbose) {
        std::cout << "🎯 开始喷涂轨迹优化修复..." << std::endl;
        std::cout << "📏 使用容差: " << tolerance << "mm（适合喷涂应用）" << std::endl;
    }

    try {
        // 步骤1: 面连接性优化（对轨迹生成最重要）
        if (verbose) {
            std::cout << "\n🔗 步骤1: 面连接性优化..." << std::endl;
        }
        
        BRepBuilderAPI_Sewing sewing(tolerance);
        sewing.SetTolerance(tolerance);
        sewing.SetFaceMode(Standard_True);
        sewing.SetFloatingEdgesMode(Standard_True);
        sewing.SetNonManifoldMode(Standard_False);
        
        // 添加所有面
        int originalFaceCount = 0;
        for (TopExp_Explorer faceExp(shape, TopAbs_FACE); faceExp.More(); faceExp.Next()) {
            sewing.Add(faceExp.Current());
            originalFaceCount++;
        }
        
        sewing.Perform();
        TopoDS_Shape sewedShape = sewing.SewedShape();
        
        if (!sewedShape.IsNull()) {
            shape = sewedShape;
            if (verbose) {
                std::cout << "✅ 面连接性优化完成，处理了 " << originalFaceCount << " 个面" << std::endl;
            }
        }

        // 步骤2: 移除内部线框（简化轨迹生成）
        if (verbose) {
            std::cout << "\n🧹 步骤2: 移除内部线框..." << std::endl;
        }
        
        Handle(ShapeUpgrade_RemoveInternalWires) wireRemover = new ShapeUpgrade_RemoveInternalWires(shape);
        wireRemover->MinArea() = tolerance * tolerance; // 最小面积阈值
        wireRemover->RemoveFaceMode() = Standard_False; // 不移除面，只移除内部线框
        
        wireRemover->Perform();
        
        if (wireRemover->Status(ShapeExtend_DONE1)) {
            TopoDS_Shape cleanedShape = wireRemover->GetResult();
            if (!cleanedShape.IsNull()) {
                shape = cleanedShape;
                if (verbose) {
                    std::cout << "✅ 内部线框移除完成" << std::endl;
                }
            }
        }

        return true;

    } catch (const std::exception& e) {
        if (verbose) {
            std::cerr << "❌ 喷涂轨迹优化修复过程中发生异常: " << e.what() << std::endl;
        }
        return false;
    } catch (...) {
        if (verbose) {
            std::cerr << "❌ 喷涂轨迹优化修复过程中发生未知异常" << std::endl;
        }
        return false;
    }
}
