#include "OCCHandler.h"
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <BRep_Builder.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <gp_Trsf.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <iostream>
#include <map>
#include <vector>
#include <algorithm>

// 按高度分层并进行遮挡裁剪
TopoDS_Shape OCCHandler::removeOccludedPortions(const TopoDS_Shape& extractedFaces, double heightTolerance) {
    std::cout << "🔍 开始按高度分层并进行遮挡裁剪..." << std::endl;

    if (extractedFaces.IsNull()) {
        std::cerr << "⚠️ 输入的形状为空，无法处理遮挡" << std::endl;
        return TopoDS_Shape();
    }

    // 从输入形状中提取所有面
    TopTools_ListOfShape allFaces;
    for (TopExp_Explorer faceExplorer(extractedFaces, TopAbs_FACE); faceExplorer.More(); faceExplorer.Next()) {
        allFaces.Append(faceExplorer.Current());
    }

    if (allFaces.IsEmpty()) {
        std::cerr << "⚠️ 没有找到可处理的面" << std::endl;
        return TopoDS_Shape();
    }

    std::cout << "📊 输入面数量: " << allFaces.Extent() << std::endl;

    // 按高度分层
    std::map<double, TopTools_ListOfShape> layeredFaces = groupFacesByHeight(allFaces, heightTolerance);

    if (layeredFaces.empty()) {
        std::cerr << "⚠️ 分层失败" << std::endl;
        return TopoDS_Shape();
    }

    std::cout << "📋 分层完成，共 " << layeredFaces.size() << " 层" << std::endl;

    // 将分层结果转换为向量，便于处理
    std::vector<std::pair<double, TopTools_ListOfShape>> layers(layeredFaces.begin(), layeredFaces.end());
    
    // 按高度从高到低排序
    std::sort(layers.begin(), layers.end(), [](const auto& a, const auto& b) {
        return a.first > b.first; // 降序排列
    });

    std::cout << "🔄 开始逐层遮挡处理..." << std::endl;

    // 逐层处理遮挡
    for (size_t i = 0; i < layers.size(); i++) {
        double currentHeight = layers[i].first;
        TopTools_ListOfShape& currentLayerFaces = layers[i].second;

        std::cout << "\n🎯 处理第 " << (i + 1) << " 层 (Z=" << currentHeight << "), "
                  << currentLayerFaces.Extent() << " 个面" << std::endl;

        // 首先处理同层内的重叠面
        if (currentLayerFaces.Extent() > 1) {
            std::cout << "   🔍 处理同层内的重叠面..." << std::endl;
            TopTools_ListOfShape processedSameLayerFaces;

            for (TopTools_ListIteratorOfListOfShape it1(currentLayerFaces); it1.More(); it1.Next()) {
                TopoDS_Shape currentFace = it1.Value();
                bool isOverlapped = false;

                // 检查当前面是否与已处理的面重叠
                for (TopTools_ListIteratorOfListOfShape it2(processedSameLayerFaces); it2.More(); it2.Next()) {
                    if (checkFaceOverlapInXY(currentFace, it2.Value())) {
                        isOverlapped = true;
                        break;
                    }
                }

                // 如果没有重叠，添加到处理结果中
                if (!isOverlapped) {
                    processedSameLayerFaces.Append(currentFace);
                }
            }

            currentLayerFaces = processedSameLayerFaces;
            std::cout << "     ✅ 同层重叠处理完成，剩余 " << currentLayerFaces.Extent() << " 个面" << std::endl;
        }

        // 当前层的面需要被所有上层的面遮挡裁剪
        for (size_t j = 0; j < i; j++) {
            double upperHeight = layers[j].first;
            const TopTools_ListOfShape& upperLayerFaces = layers[j].second;

            std::cout << "   🔍 检查被第 " << (j + 1) << " 层 (Z=" << upperHeight << ") 的遮挡..." << std::endl;

            // 对当前层的每个面进行遮挡检查和裁剪
            TopTools_ListOfShape processedFaces;

            for (TopTools_ListIteratorOfListOfShape currentIt(currentLayerFaces); currentIt.More(); currentIt.Next()) {
                TopoDS_Face currentFace = TopoDS::Face(currentIt.Value());
                TopoDS_Shape resultFace = currentFace;

                // 检查当前面是否被上层的任何面遮挡
                for (TopTools_ListIteratorOfListOfShape upperIt(upperLayerFaces); upperIt.More(); upperIt.Next()) {
                    TopoDS_Face upperFace = TopoDS::Face(upperIt.Value());

                    // 检查两个面是否在XY平面上重叠
                    if (checkFaceOverlapInXY(resultFace, upperFace)) {
                        // 如果重叠，进行布尔裁剪
                        TopoDS_Shape projectedUpperFace = projectFaceToPlane(upperFace, currentHeight);

                        if (!projectedUpperFace.IsNull()) {
                            try {
                                BRepAlgoAPI_Cut cutter(resultFace, projectedUpperFace);
                                if (cutter.IsDone()) {
                                    TopoDS_Shape cutResult = cutter.Shape();
                                    if (!cutResult.IsNull()) {
                                        resultFace = cutResult;
                                    }
                                }
                            } catch (...) {
                                std::cerr << "⚠️ 布尔裁剪操作失败，保持原面" << std::endl;
                            }
                        }
                    }
                }

                // 如果裁剪后的面不为空，添加到处理结果中
                if (!resultFace.IsNull()) {
                    // 检查结果是否包含面
                    bool hasFaces = false;
                    for (TopExp_Explorer faceExp(resultFace, TopAbs_FACE); faceExp.More(); faceExp.Next()) {
                        processedFaces.Append(faceExp.Current());
                        hasFaces = true;
                    }

                    if (!hasFaces) {
                        std::cout << "     ❌ 面被完全遮挡，已移除" << std::endl;
                    }
                }
            }

            // 更新当前层的面列表
            currentLayerFaces = processedFaces;

            std::cout << "     ✅ 遮挡处理完成，剩余 " << currentLayerFaces.Extent() << " 个面" << std::endl;
        }
    }

    // 后处理：检查跨层遮挡
    std::cout << "\n🔄 后处理：检查跨层遮挡..." << std::endl;
    for (size_t i = 0; i < layers.size(); i++) {
        double currentHeight = layers[i].first;
        TopTools_ListOfShape& currentLayerFaces = layers[i].second;

        if (currentLayerFaces.IsEmpty()) continue;

        // 收集所有更高层的面
        TopTools_ListOfShape allHigherFaces;
        for (size_t j = 0; j < i; j++) {
            for (TopTools_ListIteratorOfListOfShape it(layers[j].second); it.More(); it.Next()) {
                allHigherFaces.Append(it.Value());
            }
        }

        if (!allHigherFaces.IsEmpty()) {
            std::cout << "   🎯 检查第 " << (i + 1) << " 层被 " << allHigherFaces.Extent() << " 个更高层面的跨层遮挡..." << std::endl;

            TopTools_ListOfShape finalLayerFaces;
            for (TopTools_ListIteratorOfListOfShape currentIt(currentLayerFaces); currentIt.More(); currentIt.Next()) {
                TopoDS_Shape currentFace = currentIt.Value();
                bool isCompletelyOccluded = false;

                // 检查是否被任何更高层的面完全遮挡
                for (TopTools_ListIteratorOfListOfShape higherIt(allHigherFaces); higherIt.More(); higherIt.Next()) {
                    TopoDS_Shape higherFace = higherIt.Value();

                    // 计算重叠程度
                    if (checkFaceOverlapInXY(currentFace, higherFace)) {
                        // 计算重叠面积比例
                        try {
                            TopoDS_Shape proj1 = projectFaceToPlane(currentFace, 0.0);
                            TopoDS_Shape proj2 = projectFaceToPlane(higherFace, 0.0);

                            if (!proj1.IsNull() && !proj2.IsNull()) {
                                GProp_GProps props1, props2;
                                BRepGProp::SurfaceProperties(proj1, props1);
                                BRepGProp::SurfaceProperties(proj2, props2);

                                BRepAlgoAPI_Common commonOp(proj1, proj2);
                                if (commonOp.IsDone()) {
                                    TopoDS_Shape intersection = commonOp.Shape();
                                    if (!intersection.IsNull()) {
                                        GProp_GProps intersectionProps;
                                        BRepGProp::SurfaceProperties(intersection, intersectionProps);

                                        double currentArea = props1.Mass();
                                        double intersectionArea = intersectionProps.Mass();

                                        // 如果当前面被遮挡超过80%，认为完全遮挡
                                        if (intersectionArea > currentArea * 0.8) {
                                            isCompletelyOccluded = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        } catch (...) {
                            // 忽略计算错误
                        }
                    }
                }

                if (!isCompletelyOccluded) {
                    finalLayerFaces.Append(currentFace);
                }
            }

            int removedCount = currentLayerFaces.Extent() - finalLayerFaces.Extent();
            if (removedCount > 0) {
                std::cout << "     ❌ 移除了 " << removedCount << " 个被跨层遮挡的面" << std::endl;
            }
            currentLayerFaces = finalLayerFaces;
        }
    }

    // 收集所有处理后的面
    TopTools_ListOfShape finalFaces;
    int totalProcessedFaces = 0;
    
    for (const auto& layer : layers) {
        for (TopTools_ListIteratorOfListOfShape it(layer.second); it.More(); it.Next()) {
            finalFaces.Append(it.Value());
            totalProcessedFaces++;
        }
    }

    std::cout << "\n📊 遮挡处理完成:" << std::endl;
    std::cout << "   输入面数: " << allFaces.Extent() << std::endl;
    std::cout << "   输出面数: " << totalProcessedFaces << std::endl;
    std::cout << "   移除面数: " << (allFaces.Extent() - totalProcessedFaces) << std::endl;

    // 创建最终的复合形状
    if (finalFaces.IsEmpty()) {
        std::cerr << "⚠️ 所有面都被遮挡，返回空形状" << std::endl;
        return TopoDS_Shape();
    }

    try {
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);

        for (TopTools_ListIteratorOfListOfShape it(finalFaces); it.More(); it.Next()) {
            builder.Add(compound, it.Value());
        }

        std::cout << "✅ 遮挡裁剪完成！" << std::endl;
        return compound;

    } catch (...) {
        std::cerr << "❌ 创建最终形状时发生异常" << std::endl;
        return TopoDS_Shape();
    }
}

// 检查两个面是否在XY平面上重叠
bool OCCHandler::checkFaceOverlapInXY(const TopoDS_Shape& face1, const TopoDS_Shape& face2) const {
    try {
        // 将两个面投影到Z=0平面
        TopoDS_Shape proj1 = projectFaceToPlane(face1, 0.0);
        TopoDS_Shape proj2 = projectFaceToPlane(face2, 0.0);
        
        if (proj1.IsNull() || proj2.IsNull()) {
            return false;
        }

        // 计算投影面的面积
        GProp_GProps props1, props2;
        BRepGProp::SurfaceProperties(proj1, props1);
        BRepGProp::SurfaceProperties(proj2, props2);
        
        double area1 = props1.Mass();
        double area2 = props2.Mass();
        
        if (area1 < 1e-10 || area2 < 1e-10) {
            return false;
        }

        // 尝试计算交集
        try {
            BRepAlgoAPI_Common commonOp(proj1, proj2);
            if (commonOp.IsDone()) {
                TopoDS_Shape intersection = commonOp.Shape();
                if (!intersection.IsNull()) {
                    GProp_GProps intersectionProps;
                    BRepGProp::SurfaceProperties(intersection, intersectionProps);
                    double intersectionArea = intersectionProps.Mass();
                    
                    // 如果交集面积大于较小面积的20%，认为有重叠
                    double minArea = std::min(area1, area2);
                    return (intersectionArea > minArea * 0.2);
                }
            }
        } catch (...) {
            // 如果布尔运算失败，使用边界盒检查
            Bnd_Box box1, box2;
            BRepBndLib::Add(proj1, box1);
            BRepBndLib::Add(proj2, box2);
            
            return !box1.IsOut(box2);
        }
        
        return false;
        
    } catch (...) {
        return false;
    }
}

// 将面投影到指定Z平面
TopoDS_Shape OCCHandler::projectFaceToPlane(const TopoDS_Shape& face, double targetZ) const {
    try {
        // 计算当前面的Z高度
        GProp_GProps props;
        BRepGProp::SurfaceProperties(face, props);
        gp_Pnt centroid = props.CentreOfMass();
        double currentZ = centroid.Z();
        
        // 创建平移变换
        gp_Trsf translation;
        translation.SetTranslation(gp_Vec(0, 0, targetZ - currentZ));
        
        // 应用变换
        BRepBuilderAPI_Transform transformer(face, translation);
        return transformer.Shape();
        
    } catch (...) {
        std::cerr << "❌ 投影面到平面时发生异常" << std::endl;
        return TopoDS_Shape();
    }
}

// 将形状移动到指定Z平面
TopoDS_Shape OCCHandler::moveShapeToPlane(const TopoDS_Shape& shape, double targetZ) const {
    try {
        if (shape.IsNull()) {
            return TopoDS_Shape();
        }

        // 计算形状的边界盒
        Bnd_Box boundingBox;
        BRepBndLib::Add(shape, boundingBox);
        
        Standard_Real xMin, yMin, zMin, xMax, yMax, zMax;
        boundingBox.Get(xMin, yMin, zMin, xMax, yMax, zMax);
        
        // 计算Z方向的中心
        double currentZ = (zMin + zMax) / 2.0;
        
        // 创建平移变换
        gp_Trsf translation;
        translation.SetTranslation(gp_Vec(0, 0, targetZ - currentZ));
        
        // 应用变换
        BRepBuilderAPI_Transform transformer(shape, translation);
        return transformer.Shape();
        
    } catch (...) {
        std::cerr << "❌ 移动形状时发生异常" << std::endl;
        return TopoDS_Shape();
    }
}
