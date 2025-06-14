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
// 几何和拓扑相关
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <TopoDS_Compound.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <Geom_Surface.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <ShapeUpgrade_ShapeDivideArea.hxx>
// 消息和状态相关
#include <ShapeExtend_MsgRegistrator.hxx>
#include <ShapeExtend_Status.hxx>
// 构建和重塑相关
#include <ShapeBuild_ReShape.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <iostream>

// STEP导入后模型修复函数
bool OCCHandler::repairImportedModel(double tolerance, bool verbose) {
    if (shape.IsNull()) {
        if (verbose) {
            std::cerr << "❌ 没有加载的模型，无法进行修复" << std::endl;
        }
        return false;
    }

    if (verbose) {
        std::cout << "🔧 开始STEP模型修复..." << std::endl;
        std::cout << "📏 使用容差: " << tolerance << std::endl;
    }

    try {
        // 步骤1: 模型有效性检查
        if (verbose) {
            std::cout << "\n📋 步骤1: 模型有效性检查..." << std::endl;
        }
        
        BRepCheck_Analyzer analyzer(shape);
        bool isValid = analyzer.IsValid();
        
        if (verbose) {
            if (isValid) {
                std::cout << "✅ 模型基本有效" << std::endl;
            } else {
                std::cout << "⚠️ 模型存在问题，需要修复" << std::endl;
            }
        }

        // 步骤2: 缝合修复（最重要的修复步骤）
        if (verbose) {
            std::cout << "\n🧵 步骤2: 缝合修复..." << std::endl;
        }
        
        BRepBuilderAPI_Sewing sewing(tolerance);
        sewing.SetTolerance(tolerance);
        sewing.SetFaceMode(Standard_True);
        sewing.SetFloatingEdgesMode(Standard_True);
        sewing.SetNonManifoldMode(Standard_False);
        
        // 添加所有面到缝合器
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
                std::cout << "✅ 缝合修复成功" << std::endl;
            }
        } else {
            if (verbose) {
                std::cout << "⚠️ 缝合修复失败，保持原形状" << std::endl;
            }
        }

        // 步骤3: 基本形状修复
        if (verbose) {
            std::cout << "\n🛠️ 步骤3: 基本形状修复..." << std::endl;
        }
        
        Handle(ShapeFix_Shape) shapeFixer = new ShapeFix_Shape();
        shapeFixer->Init(shape);
        shapeFixer->SetPrecision(tolerance);
        shapeFixer->SetMaxTolerance(tolerance * 100);
        shapeFixer->SetMinTolerance(tolerance * 0.1);
        
        // 执行修复
        bool fixResult = shapeFixer->Perform();
        
        if (fixResult) {
            TopoDS_Shape fixedShape = shapeFixer->Shape();
            if (!fixedShape.IsNull()) {
                shape = fixedShape;
                if (verbose) {
                    std::cout << "✅ 形状修复成功" << std::endl;
                }
            }
        } else {
            if (verbose) {
                std::cout << "⚠️ 形状修复未完全成功，但继续处理" << std::endl;
            }
        }

        // 步骤4: 验证修复结果
        if (verbose) {
            std::cout << "\n✔️ 步骤4: 修复结果验证..." << std::endl;
        }
        
        BRepCheck_Analyzer finalAnalyzer(shape);
        bool finalValid = finalAnalyzer.IsValid();
        
        // 统计修复后的面数量
        int finalFaceCount = 0;
        for (TopExp_Explorer faceExp(shape, TopAbs_FACE); faceExp.More(); faceExp.Next()) {
            finalFaceCount++;
        }
        
        if (verbose) {
            std::cout << "📊 修复结果:" << std::endl;
            std::cout << "   最终面数量: " << finalFaceCount << std::endl;
            std::cout << "   模型有效性: " << (finalValid ? "✅" : "⚠️") << std::endl;
            std::cout << "✅ 模型修复完成" << std::endl;
        }

        return true;

    } catch (const std::exception& e) {
        if (verbose) {
            std::cerr << "❌ 修复过程中发生异常: " << e.what() << std::endl;
        }
        return false;
    } catch (...) {
        if (verbose) {
            std::cerr << "❌ 修复过程中发生未知异常" << std::endl;
        }
        return false;
    }
}

// 修复小面和小边
bool OCCHandler::fixSmallFacesAndEdges(double tolerance, bool verbose) {
    if (shape.IsNull()) {
        if (verbose) {
            std::cerr << "❌ 没有加载的模型，无法进行修复" << std::endl;
        }
        return false;
    }

    if (verbose) {
        std::cout << "🔬 开始修复小面和小边..." << std::endl;
        std::cout << "📏 使用容差: " << tolerance << std::endl;
    }

    try {
        // 步骤1: 检测和统计小面
        if (verbose) {
            std::cout << "\n🔍 步骤1: 检测小面..." << std::endl;
        }
        
        ShapeAnalysis_CheckSmallFace smallFaceChecker;
        int spotFaceCount = 0, stripFaceCount = 0, totalFaceCount = 0;
        
        for (TopExp_Explorer faceExp(shape, TopAbs_FACE); faceExp.More(); faceExp.Next()) {
            totalFaceCount++;
            TopoDS_Face face = TopoDS::Face(faceExp.Current());
            TopoDS_Edge edge1, edge2;
            
            if (smallFaceChecker.CheckSpotFace(face, tolerance)) {
                spotFaceCount++;
            }
            if (smallFaceChecker.CheckStripFace(face, edge1, edge2, tolerance)) {
                stripFaceCount++;
            }
        }
        
        if (verbose) {
            std::cout << "   总面数: " << totalFaceCount << std::endl;
            std::cout << "   点状小面: " << spotFaceCount << std::endl;
            std::cout << "   条状小面: " << stripFaceCount << std::endl;
        }

        // 步骤2: 修复小面
        if (spotFaceCount > 0 || stripFaceCount > 0) {
            if (verbose) {
                std::cout << "\n🛠️ 步骤2: 修复小面..." << std::endl;
            }
            
            Handle(ShapeFix_FixSmallFace) smallFaceFixer = new ShapeFix_FixSmallFace();
            smallFaceFixer->Init(shape);
            smallFaceFixer->SetPrecision(tolerance);
            smallFaceFixer->SetMaxTolerance(tolerance * 100);

            smallFaceFixer->Perform();
            TopoDS_Shape fixedShape = smallFaceFixer->FixShape();
            if (!fixedShape.IsNull()) {
                shape = fixedShape;
                if (verbose) {
                    std::cout << "✅ 小面修复完成" << std::endl;
                }
            }
        } else {
            if (verbose) {
                std::cout << "✅ 未发现需要修复的小面" << std::endl;
            }
        }

        // 步骤3: 检测和修复小边
        if (verbose) {
            std::cout << "\n📏 步骤3: 检测和修复小边..." << std::endl;
        }
        
        Handle(ShapeFix_Wireframe) wireframeFixer = new ShapeFix_Wireframe(shape);
        wireframeFixer->SetPrecision(tolerance);
        wireframeFixer->SetMaxTolerance(tolerance * 100);
        
        // 启用小边移除模式
        wireframeFixer->ModeDropSmallEdges() = true;
        
        // 修复小边
        wireframeFixer->FixSmallEdges();
        
        TopoDS_Shape edgeFixedShape = wireframeFixer->Shape();
        if (!edgeFixedShape.IsNull()) {
            shape = edgeFixedShape;
            if (verbose) {
                std::cout << "✅ 小边修复完成" << std::endl;
            }
        }

        return true;

    } catch (const std::exception& e) {
        if (verbose) {
            std::cerr << "❌ 小面小边修复过程中发生异常: " << e.what() << std::endl;
        }
        return false;
    } catch (...) {
        if (verbose) {
            std::cerr << "❌ 小面小边修复过程中发生未知异常" << std::endl;
        }
        return false;
    }
}

// 修复线框问题
bool OCCHandler::fixWireframeIssues(double tolerance, bool verbose) {
    if (shape.IsNull()) {
        if (verbose) {
            std::cerr << "❌ 没有加载的模型，无法进行修复" << std::endl;
        }
        return false;
    }

    if (verbose) {
        std::cout << "🔧 开始修复线框问题..." << std::endl;
        std::cout << "📏 使用容差: " << tolerance << std::endl;
    }

    try {
        // 步骤1: 分析线框问题
        if (verbose) {
            std::cout << "\n🔍 步骤1: 分析线框问题..." << std::endl;
        }

        int totalWireCount = 0;
        int problematicWireCount = 0;

        for (TopExp_Explorer faceExp(shape, TopAbs_FACE); faceExp.More(); faceExp.Next()) {
            TopoDS_Face face = TopoDS::Face(faceExp.Current());

            for (TopExp_Explorer wireExp(face, TopAbs_WIRE); wireExp.More(); wireExp.Next()) {
                totalWireCount++;
                TopoDS_Wire wire = TopoDS::Wire(wireExp.Current());

                ShapeAnalysis_Wire wireAnalyzer(wire, face, tolerance);

                bool hasProblems = false;

                // 检查边顺序
                if (wireAnalyzer.CheckOrder()) {
                    hasProblems = true;
                }

                // 检查连接性
                if (wireAnalyzer.CheckConnected()) {
                    hasProblems = true;
                }

                // 检查小边
                if (wireAnalyzer.CheckSmall(tolerance)) {
                    hasProblems = true;
                }

                // 检查自相交
                if (wireAnalyzer.CheckSelfIntersection()) {
                    hasProblems = true;
                }

                if (hasProblems) {
                    problematicWireCount++;
                }
            }
        }

        if (verbose) {
            std::cout << "   总线框数: " << totalWireCount << std::endl;
            std::cout << "   问题线框数: " << problematicWireCount << std::endl;
        }

        // 步骤2: 修复线框问题
        if (problematicWireCount > 0) {
            if (verbose) {
                std::cout << "\n🛠️ 步骤2: 修复线框问题..." << std::endl;
            }

            // 创建重塑工具
            Handle(ShapeBuild_ReShape) reshapeContext = new ShapeBuild_ReShape();
            reshapeContext->Apply(shape);

            int fixedWireCount = 0;

            for (TopExp_Explorer faceExp(shape, TopAbs_FACE); faceExp.More(); faceExp.Next()) {
                TopoDS_Face face = TopoDS::Face(faceExp.Current());

                for (TopExp_Explorer wireExp(face, TopAbs_WIRE); wireExp.More(); wireExp.Next()) {
                    TopoDS_Wire wire = TopoDS::Wire(wireExp.Current());

                    ShapeAnalysis_Wire wireAnalyzer(wire, face, tolerance);
                    ShapeFix_Wire wireFixer(wire, face, tolerance);

                    bool needsFix = false;

                    // 修复边顺序
                    if (wireAnalyzer.CheckOrder()) {
                        wireFixer.FixReorder();
                        needsFix = true;
                    }

                    // 修复连接性
                    if (wireAnalyzer.CheckConnected()) {
                        wireFixer.FixConnected();
                        needsFix = true;
                    }

                    // 修复小边
                    if (wireAnalyzer.CheckSmall(tolerance)) {
                        Standard_Boolean lockVertex = Standard_True;
                        if (wireFixer.FixSmall(lockVertex, tolerance)) {
                            needsFix = true;
                        }
                    }

                    // 修复自相交
                    if (wireAnalyzer.CheckSelfIntersection()) {
                        if (wireFixer.FixSelfIntersection()) {
                            needsFix = true;
                        }
                    }

                    // 修复缺失的边
                    if (wireFixer.FixLacking(false)) {
                        needsFix = true;
                    }

                    if (needsFix) {
                        fixedWireCount++;
                        TopoDS_Wire fixedWire = wireFixer.Wire();
                        reshapeContext->Replace(wire, fixedWire);
                    }
                }
            }

            // 应用修复
            TopoDS_Shape wireframeFixedShape = reshapeContext->Apply(shape);
            if (!wireframeFixedShape.IsNull()) {
                shape = wireframeFixedShape;
                if (verbose) {
                    std::cout << "✅ 修复了 " << fixedWireCount << " 个线框" << std::endl;
                }
            }
        } else {
            if (verbose) {
                std::cout << "✅ 未发现需要修复的线框问题" << std::endl;
            }
        }

        return true;

    } catch (const std::exception& e) {
        if (verbose) {
            std::cerr << "❌ 线框修复过程中发生异常: " << e.what() << std::endl;
        }
        return false;
    } catch (...) {
        if (verbose) {
            std::cerr << "❌ 线框修复过程中发生未知异常" << std::endl;
        }
        return false;
    }
}
