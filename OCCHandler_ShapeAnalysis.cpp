#include "OCCHandler.h"
#include <BRepCheck_Analyzer.hxx>
#include <ShapeAnalysis_Wire.hxx>
#include <ShapeAnalysis_Edge.hxx>
#include <ShapeAnalysis_Shell.hxx>
#include <ShapeAnalysis_FreeBounds.hxx>
#include <ShapeAnalysis_CheckSmallFace.hxx>
#include <ShapeAnalysis_ShapeTolerance.hxx>
#include <ShapeAnalysis_ShapeContents.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <iostream>

// 形状验证和分析
bool OCCHandler::validateAndAnalyzeShape(bool verbose) {
    if (shape.IsNull()) {
        if (verbose) {
            std::cerr << "❌ 没有加载的模型，无法进行验证" << std::endl;
        }
        return false;
    }

    if (verbose) {
        std::cout << "🔍 开始形状验证和分析..." << std::endl;
    }

    try {
        // 基本有效性检查
        if (verbose) {
            std::cout << "\n📋 基本有效性检查..." << std::endl;
        }
        
        BRepCheck_Analyzer analyzer(shape);
        bool isValid = analyzer.IsValid();
        
        if (verbose) {
            std::cout << "   基本有效性: " << (isValid ? "✅ 有效" : "❌ 无效") << std::endl;
        }

        // 容差分析
        if (verbose) {
            std::cout << "\n📐 容差分析..." << std::endl;
        }
        
        ShapeAnalysis_ShapeTolerance toleranceAnalyzer;
        Standard_Real avgTolerance = toleranceAnalyzer.Tolerance(shape, 0);
        Standard_Real maxTolerance = toleranceAnalyzer.Tolerance(shape, 1);
        Standard_Real minTolerance = toleranceAnalyzer.Tolerance(shape, -1);
        
        Standard_Real maxVertexTol = toleranceAnalyzer.Tolerance(shape, 1, TopAbs_VERTEX);
        Standard_Real maxEdgeTol = toleranceAnalyzer.Tolerance(shape, 1, TopAbs_EDGE);
        Standard_Real maxFaceTol = toleranceAnalyzer.Tolerance(shape, 1, TopAbs_FACE);
        
        if (verbose) {
            std::cout << "   整体平均容差: " << avgTolerance << std::endl;
            std::cout << "   整体最大容差: " << maxTolerance << std::endl;
            std::cout << "   整体最小容差: " << minTolerance << std::endl;
            std::cout << "   顶点最大容差: " << maxVertexTol << std::endl;
            std::cout << "   边最大容差: " << maxEdgeTol << std::endl;
            std::cout << "   面最大容差: " << maxFaceTol << std::endl;
        }

        // 几何内容分析
        if (verbose) {
            std::cout << "\n🔬 几何内容分析..." << std::endl;
        }
        
        ShapeAnalysis_ShapeContents contentAnalyzer;
        contentAnalyzer.Perform(shape);
        
        if (verbose) {
            std::cout << "   自由曲线数量: " << contentAnalyzer.NbFreeEdges() << std::endl;
            std::cout << "   共享边数量: " << contentAnalyzer.NbSharedEdges() << std::endl;
        }

        // 自由边界分析
        if (verbose) {
            std::cout << "\n🔗 自由边界分析..." << std::endl;
        }
        
        ShapeAnalysis_FreeBounds freeBoundsAnalyzer(shape);
        TopoDS_Compound closedWires = freeBoundsAnalyzer.GetClosedWires();
        TopoDS_Compound openWires = freeBoundsAnalyzer.GetOpenWires();
        
        int closedWireCount = 0, openWireCount = 0;
        for (TopExp_Explorer exp(closedWires, TopAbs_WIRE); exp.More(); exp.Next()) closedWireCount++;
        for (TopExp_Explorer exp(openWires, TopAbs_WIRE); exp.More(); exp.Next()) openWireCount++;
        
        if (verbose) {
            std::cout << "   封闭自由边界: " << closedWireCount << std::endl;
            std::cout << "   开放自由边界: " << openWireCount << std::endl;
        }

        // 小面检查
        if (verbose) {
            std::cout << "\n🔬 小面检查..." << std::endl;
        }
        
        ShapeAnalysis_CheckSmallFace smallFaceChecker;
        int spotFaceCount = 0, stripFaceCount = 0;
        double minFaceArea = 1e-6;
        
        for (TopExp_Explorer faceExp(shape, TopAbs_FACE); faceExp.More(); faceExp.Next()) {
            TopoDS_Face face = TopoDS::Face(faceExp.Current());
            TopoDS_Edge edge1, edge2;
            
            if (smallFaceChecker.CheckSpotFace(face, minFaceArea)) {
                spotFaceCount++;
            }
            if (smallFaceChecker.CheckStripFace(face, edge1, edge2, minFaceArea)) {
                stripFaceCount++;
            }
        }
        
        if (verbose) {
            std::cout << "   点状小面: " << spotFaceCount << std::endl;
            std::cout << "   条状小面: " << stripFaceCount << std::endl;
        }

        // 边分析
        if (verbose) {
            std::cout << "\n📏 边分析..." << std::endl;
        }
        
        ShapeAnalysis_Edge edgeAnalyzer;
        int edgesWithoutCurve3d = 0;
        int edgesWithSameParameterIssues = 0;
        
        for (TopExp_Explorer edgeExp(shape, TopAbs_EDGE); edgeExp.More(); edgeExp.Next()) {
            TopoDS_Edge edge = TopoDS::Edge(edgeExp.Current());
            
            if (!edgeAnalyzer.HasCurve3d(edge)) {
                edgesWithoutCurve3d++;
            }
            
            Standard_Real maxDev = 0.0;
            if (edgeAnalyzer.CheckSameParameter(edge, maxDev)) {
                edgesWithSameParameterIssues++;
            }
        }
        
        if (verbose) {
            std::cout << "   缺少3D曲线的边: " << edgesWithoutCurve3d << std::endl;
            std::cout << "   SameParameter问题的边: " << edgesWithSameParameterIssues << std::endl;
        }

        // 壳分析
        if (verbose) {
            std::cout << "\n🐚 壳分析..." << std::endl;
        }

        int shellCount = 0;
        int invalidShellCount = 0;

        for (TopExp_Explorer shellExp(shape, TopAbs_SHELL); shellExp.More(); shellExp.Next()) {
            shellCount++;
            TopoDS_Shell shell = TopoDS::Shell(shellExp.Current());

            // 使用正确的ShapeAnalysis_Shell API
            ShapeAnalysis_Shell shellAnalyzer;
            shellAnalyzer.LoadShells(shell);

            // 检查壳的有效性
            if (shellAnalyzer.HasBadEdges()) {
                invalidShellCount++;
            }
        }

        if (verbose) {
            std::cout << "   壳总数: " << shellCount << std::endl;
            std::cout << "   无效壳数: " << invalidShellCount << std::endl;
        }

        // 总体统计
        if (verbose) {
            std::cout << "\n📊 总体统计..." << std::endl;
        }
        
        int solidCount = 0, faceCount = 0, wireCount = 0, edgeCount = 0, vertexCount = 0;
        for (TopExp_Explorer exp(shape, TopAbs_SOLID); exp.More(); exp.Next()) solidCount++;
        for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) faceCount++;
        for (TopExp_Explorer exp(shape, TopAbs_WIRE); exp.More(); exp.Next()) wireCount++;
        for (TopExp_Explorer exp(shape, TopAbs_EDGE); exp.More(); exp.Next()) edgeCount++;
        for (TopExp_Explorer exp(shape, TopAbs_VERTEX); exp.More(); exp.Next()) vertexCount++;
        
        if (verbose) {
            std::cout << "   实体: " << solidCount << std::endl;
            std::cout << "   壳: " << shellCount << std::endl;
            std::cout << "   面: " << faceCount << std::endl;
            std::cout << "   线框: " << wireCount << std::endl;
            std::cout << "   边: " << edgeCount << std::endl;
            std::cout << "   顶点: " << vertexCount << std::endl;
        }

        // 边界盒信息
        if (verbose) {
            std::cout << "\n📦 边界盒信息..." << std::endl;
        }
        
        Bnd_Box boundingBox;
        BRepBndLib::Add(shape, boundingBox);
        Standard_Real xMin, yMin, zMin, xMax, yMax, zMax;
        boundingBox.Get(xMin, yMin, zMin, xMax, yMax, zMax);
        
        if (verbose) {
            std::cout << "   X范围: [" << xMin << ", " << xMax << "] (长度: " << (xMax - xMin) << ")" << std::endl;
            std::cout << "   Y范围: [" << yMin << ", " << yMax << "] (长度: " << (yMax - yMin) << ")" << std::endl;
            std::cout << "   Z范围: [" << zMin << ", " << zMax << "] (长度: " << (zMax - zMin) << ")" << std::endl;
        }

        // 质量评估
        if (verbose) {
            std::cout << "\n⭐ 质量评估..." << std::endl;
        }
        
        bool highQuality = isValid && 
                          (edgesWithoutCurve3d == 0) && 
                          (edgesWithSameParameterIssues == 0) && 
                          (invalidShellCount == 0) && 
                          (spotFaceCount + stripFaceCount < faceCount * 0.1);
        
        if (verbose) {
            if (highQuality) {
                std::cout << "🏆 模型质量: 优秀 - 适合高精度喷涂轨迹生成" << std::endl;
            } else if (isValid) {
                std::cout << "👍 模型质量: 良好 - 适合一般喷涂轨迹生成" << std::endl;
            } else {
                std::cout << "⚠️ 模型质量: 需要修复 - 建议先进行修复再生成轨迹" << std::endl;
            }
            std::cout << "✅ 形状验证和分析完成！" << std::endl;
        }

        return isValid;

    } catch (const std::exception& e) {
        if (verbose) {
            std::cerr << "❌ 验证分析过程中发生异常: " << e.what() << std::endl;
        }
        return false;
    } catch (...) {
        if (verbose) {
            std::cerr << "❌ 验证分析过程中发生未知异常" << std::endl;
        }
        return false;
    }
}
