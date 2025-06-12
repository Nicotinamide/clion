cmake_minimum_required(VERSION 3.27)
project(SprayR)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_PREFIX_PATH "E:/Qt/6.4.3/msvc2019_64" ${CMAKE_PREFIX_PATH})
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)

# 指定 OCCT 安装路径
set(OpenCASCADE_DIR "E:/CodesE/OCCT/INSTALL/cmake")
find_package(OpenCASCADE REQUIRED)
# 添加VTK
set(VTK_DIR "E:/CodesE/VTK/lib/cmake/vtk-9.2")
find_package(VTK REQUIRED COMPONENTS
        GUISupportQt
        IOGeometry
        InteractionStyle
        RenderingAnnotation  # For vtkAxesActor
        RenderingOpenGL2
        RenderingContextOpenGL2
        RenderingFreeType    # For text rendering in axes
        InteractionWidgets   # For vtkOrientationMarkerWidget
        CommonColor
)




# 启用Qt自动MOC/UIC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)



add_executable(SprayR main.cpp
        SprayR_GUI.cpp
        SprayR_GUI.h
        VTKViewer.h
        VTKViewer.cpp
        OCCHandler.h
        OCCHandler.cpp
        FaceProcessor.h
        FaceProcessor.cpp)

# 添加路径长度测试可执行文件
add_executable(test_path_length test_path_length.cpp
        FaceProcessor.h
        FaceProcessor.cpp)

# 添加简化的路径长度测试程序
add_executable(path_length_test_simple path_length_test_simple.cpp
        FaceProcessor.h
        FaceProcessor.cpp)

# 链接VTK和OpenCASCADE库
target_link_libraries(SprayR PRIVATE
        ${VTK_LIBRARIES}
        Qt6::Widgets
        Qt6::OpenGLWidgets
        TKernel
        TKMath
        TKBRep
        TKGeomBase
        TKV3d
        TKOpenGl
        TKPrim
        TKDESTEP
        TKDEIGES
)

# 为测试程序链接必要的库
target_link_libraries(test_path_length PRIVATE
        ${VTK_LIBRARIES}
        TKernel
        TKMath
        TKBRep
        TKGeomBase
        TKV3d
        TKOpenGl
        TKPrim
        TKDESTEP
        TKDEIGES
)

# 为简化测试程序链接必要的库
target_link_libraries(path_length_test_simple PRIVATE
        ${VTK_LIBRARIES}
        TKernel
        TKMath
        TKBRep
        TKGeomBase
        TKV3d
        TKOpenGl
        TKPrim
        TKDESTEP
        TKDEIGES
)