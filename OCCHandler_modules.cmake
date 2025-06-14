# OCCHandler模块化源文件配置
# 这个文件定义了OCCHandler的所有模块化源文件

# OCCHandler模块化源文件列表
set(OCCHANDLER_SOURCES
    # 核心功能模块
    OCCHandler_Core.cpp
    
    # 修复功能模块
    OCCHandler_Repair.cpp
    OCCHandler_AdvancedRepair.cpp
    
    # 面处理模块
    OCCHandler_FaceProcessing.cpp
    
    # 形状分析模块
    OCCHandler_ShapeAnalysis.cpp
    
    # 可视化模块
    OCCHandler_Visualization.cpp
    
    # 遮挡处理模块
    OCCHandler_Occlusion.cpp
)

# OCCHandler头文件
set(OCCHANDLER_HEADERS
    OCCHandler.h
)

# 模块说明
# OCCHandler_Core.cpp           - 核心功能：构造函数、STEP加载、基本操作
# OCCHandler_Repair.cpp         - 基础修复：基本修复、小面小边修复、线框修复
# OCCHandler_AdvancedRepair.cpp - 高级修复：增强修复、喷涂轨迹优化修复
# OCCHandler_FaceProcessing.cpp - 面处理：面提取、分层、缝合
# OCCHandler_ShapeAnalysis.cpp  - 形状分析：验证、分析、质量评估
# OCCHandler_Visualization.cpp  - 可视化：VTK转换、法向量计算
# OCCHandler_Occlusion.cpp      - 遮挡处理：遮挡检测、布尔裁剪

# 使用方法：
# 在主CMakeLists.txt中包含此文件：
# include(OCCHandler_modules.cmake)
# 
# 然后将源文件添加到目标：
# add_executable(your_target ${OCCHANDLER_SOURCES} ${OCCHANDLER_HEADERS} other_sources...)
# 
# 或者创建一个库：
# add_library(OCCHandler ${OCCHANDLER_SOURCES} ${OCCHANDLER_HEADERS})

# 依赖项说明
# 所有模块都需要以下依赖：
# - OpenCASCADE (OCCT 7.9)
# - VTK (用于可视化模块)
# - Qt (用于GUI集成)

# 编译选项建议
set(OCCHANDLER_COMPILE_DEFINITIONS
    -DHAVE_FREETYPE
    -DHAVE_OPENGL_EXT
    -DHAVE_TK
    -DHAVE_VTK
    -DOCCT_DEBUG
    -DUNICODE
    -D_UNICODE
    -DWIN64
    -D_WIN64
)

# 包含目录建议
# set(OCCHANDLER_INCLUDE_DIRS
#     ${OCCT_INCLUDE_DIRS}
#     ${VTK_INCLUDE_DIRS}
#     ${Qt6_INCLUDE_DIRS}
# )

# 链接库建议
# set(OCCHANDLER_LIBRARIES
#     ${OCCT_LIBRARIES}
#     ${VTK_LIBRARIES}
#     Qt6::Core
#     Qt6::Widgets
#     Qt6::OpenGL
#     Qt6::OpenGLWidgets
# )
