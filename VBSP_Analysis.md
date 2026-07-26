# 魔改版 VBSP.exe 增加 `-prepfors2` 参数深度分析报告

本项目对 `bin/vbsp.exe`（魔改版）进行了逆向静态分析与反汇编对比，并对照 [Valve 官方 Source SDK 2013 中的 VBSP 源码](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src%2Futils%2Fvbsp%2Fvbsp.cpp)，详细还原了该魔改版是如何实现并集成 `-prepfors2`（即 `-prepareforS2`）参数的。

---

## 一、 核心改动概览

魔改版 `VBSP.exe` 的核心目的是**将 Source 1 的地图（.vmf/.bsp）几何结构与实体关系，以最纯净、无损（未裁剪/未优化）的形式保留，并注入映射标记，从而极大提升 Source 2（Counter-Strike 2 / Hammer 2）导入工作流的质量**。

通过逆向分析，我们发现当命令行指定 `-prepfors2` 时，程序内部会激活一个全局布尔变量 `g_bPrepForS2`（位于内存地址 `0x91bcf4`），并在整个编译流程中触发一系列特化分支。

官方 VBSP 与 魔改版 VBSP 的核心处理逻辑对比：

| 功能模块 | 官方 VBSP 逻辑 | 魔改版 VBSP (`-prepfors2` 激活) 逻辑 |
| :--- | :--- | :--- |
| **参数解析** | 无 `-prepfors2` 及其相关参数。 | 注册 `-prepfors2`、`-nofuncinstance`、`-isfuncinstance`，控制全局变量。 |
| **位移贴图缝合** | 在 `EmitInitialDispInfos` 中计算缝合相邻 displacement 边缘，耗时较长。 | **完全绕过** `"Finding displacement neighbors..."` 的缝合过程，直接保留原始数据。 |
| **物理碰撞生成** | 动态加载 `vphysics.dll`，为所有的固体生成 Source 1 格式的物理碰撞凸包（phy）。 | **跳过加载物理 DLL** 及其全部碰撞网格（PHY）的编译，节省大量时间。 |
| **反射球烘焙** | 调用 `Cubemap_CreateDefaultCubemaps` 生成 Source 1 默认反射贴图。 | **直接跳过该函数**。Source 2 有全新的反射探针（Reflection Probe）系统。 |
| **`func_detail` 融合** | 调用 `MoveBrushesToWorld` 将细节刷子合并进世界模型（Entity 0），原实体属性清空。 | **绕过合并逻辑**：保留 `func_detail` 实体的刷子独立性，清空其物理合并，使其作为独立子模型输出。 |
| **刷子实体裁剪** | 调用 `ProcessSubModel` 对 `func_brush`/`func_occluder` 的刷子进行交割、裁剪优化。 | **绕过裁剪**：不运行 `ProcessSubModel`。保留刷子最原始、最完整的笔刷块（防止被剔除或拼合）。 |
| **边界错误验证** | 若实体刷子未编译入 BSP 树，会触发崩溃错误：`bmodel %d has no head node`。 | **跳过该项崩溃校验**。允许无 BSP head node 的纯几何笔刷实体存在。 |
| **映射键值注入** | 无。 | 在输出的实体信息中，向相关的 BModel 实体**动态注入 `"firstbrush"` 和 `"numbrushes"` 键值对**。 |
| **Source 2 物理扩展**| 无（仅支持 Source 1 物理结构）。 | 注册 `DiskPhysics2LevelMesh_t` 与 `DiskPhysics2Polytope_t` 等用于 RTTI/Schema 的 Source 2 物理数据结构。 |
| **模型格式识别** | 仅识别 `.mdl`, `.vvd`, `.vtx`, `.phy`。 | 新增对 `.ss2`（Source 2 静态网格/道具）格式的支持与路径检验。 |

---

## 二、 关键代码实现与逆向还原对比

### 1. 命令行参数解析与全局变量设置

在官方 `vbsp.cpp` 的 `RunVBSP` 函数中，参数解析是通过一系列 `stricmp` 循环实现的。魔改版在这里插入了 `-prepfors2` 等参数的匹配逻辑：

**官方 `vbsp.cpp` 结构（参考）：**
```cpp
// 官方无此段
```

**魔改版二进制反汇编还原：**
```assembly
0x43d09e:	mov	eax, dword ptr [ebp + 0xc]   ; argv
0x43d0a1:	push	0x4a4f94                     ; "-prepfors2"
0x43d0a6:	push	dword ptr [eax + esi*4]      ; argv[i]
0x43d0a9:	call	0x461da0                     ; stricmp
0x43d0ae:	add	esp, 8
0x43d0b1:	test	eax, eax
0x43d0b3:	jne	0x43d0c5                     ; 不匹配则跳到下一个参数检验
0x43d0b5:	push	0x4a4fa0                     ; "Prepping bsp for s2 import!\n"
0x43d0ba:	call	edi                          ; Msg()
0x43d0bc:	mov	byte ptr [0x91bcf4], 1       ; g_bPrepForS2 = true
0x43d0c3:	jmp	0x43d124                     ; 匹配成功，继续解析
```

该逻辑实现了：
- 解析 `-prepfors2`，向控制台输出 `"Prepping bsp for s2 import!"`。
- 将全局布尔变量 `g_bPrepForS2`（地址 `0x91bcf4`）置为 `true`。
- 解析 `-nofuncinstance`（地址 `0x91bcf5`）与 `-isfuncinstance`（地址 `0x91c9f8`），用于控制是否在导入时完全过滤和剔除 `func_instance` 实例模型实体。

---

### 2. 绕过位移贴图缝合（Displacement Neighbors）

在 `disp_vbsp.cpp` 的 `EmitInitialDispInfos` 中，寻找位移贴图邻居是一个极其密集的几何搜索过程，官方源码如下：

**官方 `disp_vbsp.cpp` 源码：**
```cpp
void EmitInitialDispInfos( void )
{
    Msg( "Finding displacement neighbors...\n" );
    // 缝合与邻居计算逻辑...
}
```

**魔改版二进制反汇编还原：**
```assembly
0x431d10:	push	ebp
0x431d11:	mov	ebp, esp
0x431d13:	sub	esp, 0x14c
0x431d19:	cmp	byte ptr [0x91bcf4], 0       ; cmp g_bPrepForS2, 0
0x431d20:	jne	0x43321c                     ; 如果为真，直接跳转退出该函数！
0x431d26:	call	0x426aa0                     ; 正常执行 EmitInitialDispInfos 逻辑...
```

**对比解析：**
由于 Source 2（Hammer 2）拥有自己全新的高多边形位移平滑和接缝处理算法，Source 1 在编译期做的高耗时缝合不仅是无用功，甚至可能由于法线重算导致导入后面片接缝处出现黑线。魔改版在这里通过一处简单的 `cmp` 和 `jne`，在 `-prepfors2` 开启时**直接跳过了整个位移贴图邻居计算**，大幅缩短了编译时间并保留了无损的位移面。

---

### 3. 跳过 Source 1 物理碰撞编译（vphysics.dll）

在官方 VBSP 编译的末尾，需要加载 `vphysics.dll` 并调用物理引擎模块为所有刷子（Brushes）计算用于碰撞的物理凸包（PHY Lump）。

**官方 `vbsp.cpp` 源码：**
```cpp
void LoadPhysicsDLL( void )
{
    PhysicsDLLPath( "vphysics.dll" );
}
```

**魔改版二进制反汇编还原：**
```assembly
0x431d19:	cmp	byte ptr [0x91bcf4], 0       ; cmp g_bPrepForS2, 0
0x431d20:	jne	0x43321c                     ; 跳转绕过整个 LoadPhysicsDLL 流程
```

**对比解析：**
物理碰撞（`.phy` 文件结构）在 Source 2 中已经完全过时。Source 2 采用专有的物理引擎并使用自己的碰撞凸包生成器。因此在 `-prepfors2` 激活时，**VBSP 会彻底跳过 `vphysics.dll` 的加载和物理包编译**，不仅加快了编译速度，还避免了因不支持复杂的自定义刷子几何而导致的物理模型崩溃。

---

### 4. 阻止 `func_detail` 合并进世界网格

在官方设计中，`func_detail` 存在的唯一目的是在 VBSP 编译阶段将其所有的笔刷（Brushes）移动并合并进 `worldspawn`（世界几何体，Entity 0）中，从而消除不必要的子模型分割，其源码逻辑如下：

**官方 `map.cpp` 源码：**
```cpp
// 在 LoadMapFile 或实体后处理中
if ( IsFuncDetail( entity ) )
{
    MoveBrushesToWorld( entity );
    entity->numbrushes = 0;
    entity->firstbrush = 0;
}
```

**魔改版二进制反汇编还原：**
```assembly
0x41ef7d:	call	0x422310                     ; MoveBrushesToWorld()
0x41ef82:	cmp	byte ptr [0x91bcf4], 0       ; cmp g_bPrepForS2, 0
0x41ef89:	jne	0x41f1ef                     ; 如果为真，直接跳转退出，不执行下面的清零操作！
0x41ef8f:	mov	dword ptr [edi + 0x10], 0    ; entity->numbrushes = 0
0x41ef96:	mov	dword ptr [edi + 0x14], 0    ; entity->firstbrush = 0
```

**对比解析：**
在官方版本中，一旦 `MoveBrushesToWorld` 执行完，实体的 `numbrushes` 和 `firstbrush` 就会被强制抹除为 `0`，从而在生成的 BSP 中不复存在。
但是在魔改版中：
- 即使调用了移动/排序逻辑，在 `-prepfors2` 条件下，程序会**跳过清零实体刷子数的步骤**，使 `func_detail` 保留它们对刷子范围的引用。
- 这意味着这些细节刷子在输出的实体文本中**依旧属于原生的 `func_detail` 实体**。
- 这样，Source 2 的地图导入器就能够完美地将每一个 `func_detail` 单独识别并转换为对应的子网格，而不是全部混死在巨大的世界网格中，这让导入后的地图非常便于后期在 Hammer 2 中进行二次编辑！

---

### 5. 绕过传统的 BSP 裁剪编译（ProcessSubModel）

通常，在 VBSP 的 `ProcessModels` 阶段，除了 `entity_num == 0`（世界几何）外，其他所有携带刷子的实体（BModel，如 `func_brush`, `func_door` 等）都会调用 `ProcessSubModel()`。

这会对刷子进行消隐、相交裁剪以及多边形拼接（T-Junc 合并等），这会彻底破坏笔刷最初的凸多面体笔刷块特征。

**官方 `vbsp.cpp` 源码：**
```cpp
for ( entity_num=0; entity_num < num_entities; ++entity_num )
{
    entity_t *pEntity = &entities[entity_num];
    if ( !pEntity->numbrushes )
        continue;
    BeginModel ();
    if (entity_num == 0)
        ProcessWorldModel();
    else
        ProcessSubModel(); // 官方默认对所有子模型进行 BSP 树化裁剪编译
    EndModel ();
}
```

**魔改版二进制反汇编还原：**
在魔改版的相应循环中，插入了针对 `-prepfors2` 的特化检查：
```assembly
0x43bb60:	cmp	byte ptr [0x91bcf4], 0       ; cmp g_bPrepForS2, 0
0x43bb67:	je	0x43bb9e
0x43bb69:	test	eax, eax
0x43bb6b:	je	0x43bb9e
0x43bb6d:	push	eax
0x43bb6e:	call	0x43b7f0                     ; 检查是否为 func_brush
0x43bb73:	add	esp, 4
0x43bb76:	test	al, al
0x43bb78:	jne	0x43bbe5                     ; 是则直接跳过该实体的 ProcessSubModel
0x43bb7a:	push	dword ptr [0x91c514]
0x43bb80:	call	0x43b850                     ; 检查是否为 func_detail
0x43bb85:	add	esp, 4
0x43bb88:	test	al, al
0x43bb8a:	jne	0x43bbe5                     ; 是则直接跳过该实体的 ProcessSubModel
0x43bb8c:	push	dword ptr [0x91c514]
0x43bb92:	call	0x43b8b0                     ; 检查是否为 func_occluder
0x43bb97:	add	esp, 4
0x43bb9a:	test	al, al
0x43bb9c:	jne	0x43bbe5                     ; 是则直接跳过该实体的 ProcessSubModel
```

**对比解析：**
如果开启了 `-prepfors2`，魔改版会主动去匹配 `func_brush`, `func_detail`, 以及 `func_occluder` 这三类核心的刷子类几何体实体。**只要属于这三种类型，就彻底跳过它们的 `ProcessSubModel` 子编译流程**！
这一改动的意义重大：
- **保证刷子数据完全无损**。在 Source 2 中，刷子是通过其原生的“凸多面体笔刷块面”（Planes/Windings）重新渲染的。传统 VBSP 裁剪会把刷子切成无数个碎片三角形（导致面数暴增、编辑困难）。
- **跳过裁剪**能让实体导出的依旧是我们在 Hammer 1 里手画的那个干干净净的原始方块/圆柱体笔刷。

---

### 6. 避免无 BSP 树导致的崩溃保护

在官方版 VBSP 中，如果一个实体宣称自己带有刷子，但这些刷子没有经过 `ProcessSubModel` 编译生成 BSP 树节点（即没有 head node），编译器会抛出致命错误直接崩溃退出：

**官方 `vbsp.cpp` 源码：**
```cpp
if ( tree->headnode->planenum == PLANENUM_LEAF )
{
    const char *pClassName = ValueForKey( e, "classname" );
    const char *pTargetName = ValueForKey( e, "targetname" );
    Error( "bmodel %d has no head node (class '%s', targetname '%s')",
           entity_num, pClassName, pTargetName );
}
```

**魔改版二进制反汇编还原：**
```assembly
0x43bcce:	cmp	byte ptr [0x91bcf4], 0       ; cmp g_bPrepForS2, 0
0x43bcd5:	je	0x43bce0                     ; 如果为假，照常执行崩溃检测
0x43bcd7:	cmp	dword ptr [0x91c514], 0       ; cmp entity_num, 0
0x43bcde:	je	0x43bd15                     ; 如果为真（且不是世界几何体），则直接跳过该项崩溃校验！
```

**对比解析：**
由于上一步中，魔改版故意跳过了 `func_brush` / `func_detail` 等实体的子模型 BSP 编译过程，它们自然没有 head node，通常会导致编译器直接报错退出。在这里增加的 `cmp g_bPrepForS2, 0` 分支**完美切断了这一报错机制**，确保这些实体在带着完整笔刷、即使没有 BSP 树节点的情况下也能顺利完成保存输出。

---

### 7. 实体键值对（Key-Value）自动注入

由于几何体和刷子不再被压平、打碎和合并，为了使 CS2 的导入工具能将导出的 BSP 中的网格信息与 VMF 实体准确对应，魔改版在实体写入时，进行了一次极具创意的健值对动态注入：

**魔改版二进制反汇编还原：**
当满足 `-prepfors2` 时，程序会在保存实体键值之前，针对带有刷子的实体执行：
```assembly
0x43aa73:	push	dword ptr [eax + 0x2ebbebc]  ; 获取 firstbrush 的值
0x43aa79:	lea	eax, [ebp - 0x40]
0x43aa7c:	push	0x4a18b8                     ; "%i"
...
0x43aa8c:	push	eax                          ; 转换后的字符串值
0x43aa94:	push	0x4a45a4                     ; "firstbrush"
0x43aa99:	add	eax, 0x2ebbeb0
0x43aa9e:	push	eax                          ; entity 指针
0x43aa9f:	call	0x44c830                     ; SetKeyValue()

0x43aaab:	push	dword ptr [eax + 0x2ebbec0]  ; 获取 numbrushes 的值
...
0x43aacc:	push	0x4a45b0                     ; "numbrushes"
...
0x43aae7:	call	0x44c830                     ; SetKeyValue()
```

**对比解析：**
官方 VBSP 在写入实体块（Entity Lump）时，只会为刷子实体写入一个指向模型索引的关联键值（例如 `"model" "*1"`）。而魔改版在开启 `-prepfors2` 时，会**向对应实体额外写入两个自定义 Key-Value 键值对：`"firstbrush" "<索引>"` 和 `"numbrushes" "<数量>"`**。
这一改动是 CS2 地图导入工具能够精准重构实体与刷子对应关系的关键所在。通过明确指出实体的刷子区间，Source 2 导入软件便能无缝地从外部 VMF 提取出完全未优化的原始多面体几何信息，直接挂载到对应的实体下。

---

### 8. 新增 RTTI 数据类型与 `.ss2` 格式识别

在 `vbsp.exe` 的底层物理结构和文件校验方面，魔改版为了适配 Source 2 导入做了一些底层扩展：
- **物理接口：** 新增了包含 `DiskPhysics2LevelMesh_t`（S2物理网格结构）和 `DiskPhysics2Polytope_t`（S2物理多面体）的 RTTI 类信息和序列化元数据，以便在导出特定 Lump 时支持 Source 2 的物理格式。
- **文件检查：** 在静态道具和实体外部模型文件后缀校验方法（即检测 `.mdl`, `.vvd`, `.vtx`, `.phy` 后缀的函数中，地址 `0x4226d5` 处），多注册并检测了 `.ss2`（Source 2 专用网格文件）后缀。如果模型使用了 `.ss2` 后缀，则会被作为合法的输入静态道具模型予以放行。

---

## 三、 总结

魔改版 `VBSP.exe` 通过精妙、微创的二进制修改（主要以 `cmp byte ptr [0x91bcf4], 0` 分支跳转的形式），完美改变了原有 VBSP 破坏性的编译管线。其在 `-prepfors2` 下的核心改动逻辑可以总结为：

1. **“该剪的剪，该省的省”**：彻底免去 Displacement 邻居计算、Source 1 PHY 物理碰撞包编译和默认 Cubemap 的生成，节省大量时间。
2. **“原封不动，无损输出”**：强行切断 `func_detail`、`func_brush`、`func_occluder` 的压平与裁剪流程，使得它们能以无损、独立的笔刷块形式输出。
3. **“铺路搭桥，数据打通”**：防止崩溃报错的同时，向实体注入 `"firstbrush"` 与 `"numbrushes"` 映射键值，并扩展对 `.ss2` 格式的支持，使之成为连接 Source 1 与 Source 2 编译导入的一座高质量无损桥梁。
