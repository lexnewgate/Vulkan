# Vulkan C++ 例子

## 代码的获取和编译

示例的[Vulkan源代码](https://github.com/math3d/Vulkan)，是基于SaschaWillems开源的[示例程序](https://github.com/SaschaWillems/Vulkan)修改而来。可以运行在Ubuntu和Windows环境。

Vulkan源码的获得：
```
$git clone https://github.com/math3d/Vulkan.git
$git checkout -b  projection_perspective remotes/origin/projection_perspective
$git submodule init
$git submodule update
```

Vulkan源码的编译-Ubuntu 18.04：
```
$cmake CMakeLists.txt 
$make
```

Vulkan源码的编译-Windows 10：
```
$cmake -G "Visual Studio 15 2017 Win64"
```
用Visual Studio 打开项目vulkanExamples.sln，进行编译。

## 主要例子

#### 第1章 	3D程序分析方法

[Vulkan顶点和顶点颜色数据](examples/projection_perspective_quad/)

#### 第2章 	3D图形学基础

[光照的实现](examples/projection_perspective_lighting/)

#### 第4章  视图变换和眼睛坐标系

[视图变换和眼睛坐标系](examples/projection_perspective_lookat/)

#### 第7章  3D顶点 − 3D世界的1

[实现NDC是单位1的顶点](examples/projection_perspective_quad/)

[全窗口显示一个纹理](examples/projection_perspective_texture/)

#### 第8章  纹理坐标

[点原语及其纹理坐标](examples/primitive_point_particle/)

[平面纹理映射](examples/projection_perspective_mesh_quad/)

[球体纹理映射](examples/projection_perspective_mesh_sphere/)

#### 第9章 	VR的枕型畸变和消除

[VR畸变校正](examples/vr_lens_distorter/)

#### 第10章  一种特殊的全窗口显示的方法

[三个顶点实现全窗口显示](examples/projection_perspective_specialfullscreen_texture/)

[0顶点实现全窗口显示](examples/projection_perspective_specialfullscreen_texture_novertex/)

#### 第11章  光线追踪

[光线追踪平面](examples/raytracing_plane)

[Skybox](examples/raytracing_skybox)

[光线追踪三角形](examples/raytracing_triangle)

[光线追踪球](examples/raytracing_sphere/)

[光线追踪的阴影实现](examples/raytracing_shadow/)

#### 第12章  透视投影的其他应用

[延迟渲染](examples/deferred/)

[阴影](examples/shadowmapping/)
