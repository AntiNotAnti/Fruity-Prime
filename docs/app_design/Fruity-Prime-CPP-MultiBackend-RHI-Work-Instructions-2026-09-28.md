# Fruity-Prime C++版 マルチレンダリングバックエンド化 作業指示書
## OpenGL / Vulkan / Direct3D 12 / Metal RHI移行

- 対象リポジトリ: `Zection6V/Fruity-Prime`
- 対象ブランチ: `develop2`
- 監査基準: `develop2 @ 3b899ab480895a56c4798a65613b6e2f9c02002d`
- 作成日: 2026-09-28
- 対象: **C++版のみ**
- 主対象: `src/MphRead.Native`
- 既定の現行レンダラー: OpenGL
- 追加対象: Vulkan / Direct3D 12 / Metal / Android Vulkan
- C#版: **変更禁止**

---

# 0. この作業の目的

現在のC++版は、ゲーム本体の描画、シェーダー、Framebuffer、Texture、OpenGL display list、Skia/Ganesh UI合成、SwapBuffersなどがOpenGLの状態機械へ直接依存している。

この状態にVulkanやDirect3D 12の分岐を直接追加してはならない。

この作業の最終目的は、C++版を以下の構造へ移行することである。

```text
Game / Scene / HUD / Launcher
              │
              ▼
        Renderer Frontend
              │
              ▼
       Render Pass Layer
              │
              ▼
              RHI
      Render Hardware Interface
    ┌────────┬────────┬────────┬────────┬────────┐
    ▼        ▼        ▼        ▼        ▼
 OpenGL   OpenGLES  Vulkan   D3D12    Metal
 Desktop   Android  Desktop/ Windows   macOS
                    Android
    │        │        │        │        │
    └────────┴────────┴────────┴────────┴────────┘
                         │
                         ▼
                        GPU
```

最重要方針:

> **OpenGLへVulkan/DX12を足すのではなく、OpenGLを含む4バックエンドが同じRHI契約を実装する構造へ移行する。**

OpenGLをRHIの仕様にしてはならない。

Vulkan / D3D12 / Metal側でOpenGLの状態変更APIをエミュレートしてはならない。

---

# 1. 対象範囲

## 1.1 対象

この作業で変更してよい主要領域:

```text
src/MphRead.Native/
CMakeLists.txt
cmake/
.github/workflows/
docs/
```

必要に応じてC++ネイティブテスト、診断コード、ビルドスクリプトを追加してよい。

特に以下を重点対象とする。

```text
src/MphRead.Native/Renderer.cpp
src/MphRead.Native/Renderer.hpp
src/MphRead.Native/Shaders.cpp
src/MphRead.Native/Shaders.hpp

src/MphRead.Native/NativeRuntime/OpenTK/
src/MphRead.Native/NativeRuntime/Skia/

src/MphRead.Native/Mods/Render/
src/MphRead.Native/Mods/Launcher/Gui/
```

## 1.2 変更禁止

以下はこの作業の仕様変更対象ではない。

```text
src/MphRead/**/*.cs
src/MphRead.Android/**/*.cs
src/NcsfPlay/**/*.cs
```

**C#版へRHI、Vulkan、D3D12対応を追加しないこと。**

ゲームプレイ、ネットワーク、物理、AI、入力意味、ROMデータ解釈などをレンダラー移行の都合で変更しないこと。

## 1.3 Android

`src/MphRead.Native.Android` およびAndroid GLES経路は、この作業ではVulkan化を必須としない。

初期目標:

- Desktop C++ OpenGLを維持
- Desktop C++ Vulkanを追加
- Windows C++ D3D12を追加
- macOS C++ Metalを追加
- Android GLESを壊さない

Android Vulkanは、この作業完了後の独立タスクとして扱ってよい。

## 1.4 macOS / Metal方針

AppleプラットフォームではMetalをnative backendとして扱う。

設計上の優先順位:

```text
macOS native: Metal
cross-platform experimental/optional: Vulkan via MoltenVK
legacy/reference: OpenGL
```

OpenGL版は既存描画のparity referenceとして維持してよいが、新しいmacOS固有最適化はMetalへ実装する。

Metal 4の新APIは有用だが、この移行作業ではRHIをMetal 4専用に設計しない。従来MetalとMetal 4の差はbackend内部のCapabilitiesと実装選択へ閉じ込める。


macOSについては、OpenGLを永続的な主力backendとして扱わず、**MetalをmacOSの第一級native backend** とする。Vulkan on macOSを提供する場合はMoltenVK等による追加経路として扱ってよいが、Metal backend自体の代替とみなさない。

---

# 2. 現行C++版で確認済みの重要事項

## 2.7 Androidは現在EGL/GLES専用

現行 `CMakeLists.txt` はAndroidで、

```text
EGL
GLESv3
```

を解決し、`MphRead.Native.Android/OffscreenGl.*` を含むGLES系統を使用している。

現時点でAndroid Vulkan経路は存在しない。

したがってAndroid Vulkanは、

```text
既存GLESへVulkan分岐を直書き
```

ではなく、

```text
共通RHI
↓
Vulkan backend
↓
Android surface/lifecycle adapter
```

として追加する。



現行 `develop2` には以下のOpenGL密結合がある。

## 2.1 `Renderer.cpp` がOpenGLを直接操作している

例:

- `GL::BindFramebuffer`
- `GL::Viewport`
- `GL::UseProgram`
- `GL::Uniform*`
- `GL::BindTexture`
- `GL::Enable / Disable`
- `GL::StencilFunc`
- `GL::StencilOp`
- `GL::DepthFunc`
- `GL::BlendFunc`
- `GL::Begin / End`
- `GL::Vertex*`
- `GL::TexCoord*`
- `GL::GenLists / NewList / CallList / DeleteLists`
- `GL::ReadPixels`

これらをSceneやGameplay側へ残したままVulkan/D3D12対応を追加してはならない。

## 2.2 OpenGL compatibility profileへ依存している

`NativeRuntime/OpenTK/RendererPlatform.cpp` は、現行Rendererがimmediate modeを使用するためOpenGL compatibility profileを明示的に要求している。

つまり現状は、

```text
GL compatibility profile
    ↓
glBegin/glEnd
display lists
built-in vertex attributes
GLSL 120
```

に依存している。

これはVulkan/D3D12移行の主要ブロッカーである。

## 2.3 GLSL 120 built-in attributesへ依存している

`Shaders.cpp` の現行shaderは以下を使用する。

```text
#version 120

gl_Vertex
gl_Normal
gl_Color
gl_MultiTexCoord0
```

Vulkan/D3D12へ移すには、これらを明示的なvertex inputへ変換する必要がある。

## 2.4 Scene自身がGPU IDを所有している

`Renderer.hpp` ではScene側に以下のOpenGL IDが存在する。

```text
_shaderProgramId
_rttShaderProgramId
_shiftShaderProgramId
_celShaderProgramId

_frameBuffer
_screenTexture
_renderBuffer
_celTexture
_depthTexture
_celFrameBuffer
_celFrameBufferColor

_ownedTextures
_displayLists
```

最終状態ではこれらのOpenGL固有IDをSceneへ置かないこと。

## 2.5 Skia UIも現在OpenGL Ganeshである

`NativeRuntime/Skia/SkiaGpu.cpp` は現在、

```text
GrGL
OpenGL framebuffer
OpenGL texture
OpenGL program
OpenGL state snapshot / restore
```

を利用している。

現在のフレーム順序は概ね以下。

```text
Game scene OpenGL
        ↓
HUD
        ↓
Skia/Ganesh UI
        ↓
UiOverlay
        ↓
LauncherHunter
        ↓
SwapBuffers
```

この描画順序は維持すること。

Vulkan/D3D12モード時に、裏でOpenGL contextを作ってUIだけOpenGL描画する方式は禁止する。

## 2.6 現在のライブC++ UIはGPU経路

`docs/Fruity-Prime-CPP-CPU-Rendering-Audit-2026-09-28.md` の撤去後状態を維持すること。

新しいVulkan/D3D12経路のために、

```text
GPU renderer
↓
CPU surfaceへ全画面描画
↓
毎フレームGPU upload
```

のようなCPUレンダリングフォールバックを復活させてはならない。

画像decodeやreadbackなどのCPU処理と、全画面CPU rasterizerは区別すること。

---

# 3. 絶対に守る設計原則

## 3.1 RHIをOpenGL風にしない

禁止例:

```cpp
renderer.EnableBlend();
renderer.DisableDepthTest();
renderer.BindTexture(0, texture);
renderer.UseProgram(program);
renderer.BeginTriangles();
renderer.Vertex(...);
renderer.End();
```

これはOpenGL APIを名前だけ変えたものであり、禁止。

RHIはVulkan / D3D12 / Metalの明示的command submissionモデルへ寄せる。

推奨する基本概念:

```text
GraphicsDevice
GraphicsQueue
CommandList

Buffer
Texture
TextureView
Sampler

Shader
Pipeline
PipelineLayout

BindingLayout
BindingSet

Swapchain

Fence
FrameContext

ResourceState
Barrier
```

## 3.2 Renderer FrontendにAPI名を出さない

禁止:

```cpp
if (backend == Vulkan) ...
if (backend == D3D12) ...
if (backend == OpenGL) ...
```

をScene、Entity、HUD、Material、Meshの各所へ増殖させること。

Backend差は原則RHI backend内部へ閉じ込める。

例外はCapabilitiesによる機能判定のみ。

```cpp
if (device.Capabilities().SupportsXYZ) {
    ...
}
```

## 3.3 raw API handleを漏らさない

通常コードで以下を使用しない。

```text
GLuint
VkImage
VkBuffer
VkPipeline
VkCommandBuffer
ID3D12Resource*
ID3D12PipelineState*
```

これらを使用してよいのは各backend実装、および明示的に定義したinterop bridgeのみ。

## 3.4 暗黙フォールバック禁止

ユーザーが `vulkan` または `d3d12` を明示選択した場合、

```text
初期化失敗
↓
黙ってOpenGL
```

は禁止。

初期化失敗を明確に返すこと。

`auto` モードを作る場合のみ、明文化した優先順位で選択してよい。

初期移行期間はOpenGLを既定値のまま維持してよい。

## 3.5 CPU full-frame fallback禁止

Vulkan/D3D12実装が未完成だからという理由で、

```text
CPUでゲーム画面生成
CPUでUI全体生成
CPU bitmapを毎フレームupload
```

してはならない。

## 3.6 実装初期からmulti-thread rendererを作らない

最初は現行render threadを維持する。

以下はOpenGL/Vulkan/D3D12の描画一致後に行う。

- parallel command recording
- async compute
- async transfer queue最適化
- bindless全面化
- render graphによるaliasing最適化

まず正しさを優先する。

---

# 4. 推奨ディレクトリ構成

C++独自のGPU抽象層として、以下のような構造を作る。

```text
src/MphRead.Native/
└─ NativeRuntime/
   └─ Rhi/
      ├─ Rhi.hpp
      ├─ GraphicsDevice.hpp
      ├─ CommandList.hpp
      ├─ Resources.hpp
      ├─ Pipeline.hpp
      ├─ Bindings.hpp
      ├─ Swapchain.hpp
      ├─ FrameContext.hpp
      ├─ Capabilities.hpp
      ├─ ResourceState.hpp
      │
      ├─ OpenGL/
      │  ├─ GlDevice.hpp
      │  ├─ GlDevice.cpp
      │  ├─ GlCommandList.hpp
      │  ├─ GlCommandList.cpp
      │  ├─ GlResources.hpp
      │  ├─ GlResources.cpp
      │  ├─ GlPipeline.hpp
      │  ├─ GlPipeline.cpp
      │  ├─ GlSwapchain.hpp
      │  └─ GlSwapchain.cpp
      │
      ├─ Vulkan/
      │  ├─ VkDevice.hpp
      │  ├─ VkDevice.cpp
      │  ├─ VkCommandList.hpp
      │  ├─ VkCommandList.cpp
      │  ├─ VkResources.hpp
      │  ├─ VkResources.cpp
      │  ├─ VkPipeline.hpp
      │  ├─ VkPipeline.cpp
      │  ├─ VkSwapchain.hpp
      │  └─ VkSwapchain.cpp
      │
      ├─ D3D12/
      │  ├─ D3D12Device.hpp
      │  ├─ D3D12Device.cpp
      │  ├─ D3D12CommandList.hpp
      │  ├─ D3D12CommandList.cpp
      │  ├─ D3D12Resources.hpp
      │  ├─ D3D12Resources.cpp
      │  ├─ D3D12Pipeline.hpp
      │  ├─ D3D12Pipeline.cpp
      │  ├─ D3D12Swapchain.hpp
      │  └─ D3D12Swapchain.cpp
      │
      └─ Metal/
         ├─ MetalDevice.hpp
         ├─ MetalDevice.mm
         ├─ MetalCommandList.hpp
         ├─ MetalCommandList.mm
         ├─ MetalResources.hpp
         ├─ MetalResources.mm
         ├─ MetalPipeline.hpp
         ├─ MetalPipeline.mm
         ├─ MetalSwapchain.hpp
         └─ MetalSwapchain.mm
```

既存プロジェクト方針に合わせ、`.hpp` は対応 `.cpp` と同じ機能ディレクトリへ置く。

`include/` へまとめない。

ファイル名は実装中に既存命名規約へ合わせて調整してよいが、**backendの境界自体は維持すること。**

---

# 5. RHI最小契約

最初から巨大な万能APIを作らない。

最低限、以下を表現できればよい。

## 5.1 Device

責務:

- adapter/device初期化
- resource生成
- pipeline生成
- command list生成
- swapchain生成
- capability公開
- GPU idle待機
- deferred destruction管理

概念:

```cpp
class GraphicsDevice
{
public:
    virtual const DeviceCapabilities& Capabilities() const = 0;

    virtual BufferHandle CreateBuffer(const BufferDesc&) = 0;
    virtual TextureHandle CreateTexture(const TextureDesc&) = 0;
    virtual SamplerHandle CreateSampler(const SamplerDesc&) = 0;

    virtual ShaderHandle CreateShader(const ShaderDesc&) = 0;
    virtual PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc&) = 0;

    virtual BindingLayoutHandle CreateBindingLayout(const BindingLayoutDesc&) = 0;
    virtual BindingSetHandle CreateBindingSet(const BindingSetDesc&) = 0;

    virtual CommandList& BeginCommands() = 0;
    virtual void Submit(CommandList&) = 0;

    virtual void WaitIdle() = 0;
};
```

実際の型設計は既存コードに合わせてよい。

重要なのはOpenGL APIをそのまま写さないこと。

## 5.2 CommandList

最低限:

```text
Begin
End

BeginRendering
EndRendering

SetPipeline

SetVertexBuffer
SetIndexBuffer

SetBindingSet

SetViewport
SetScissor

SetStencilReference

Draw
DrawIndexed

CopyBuffer
CopyBufferToTexture
CopyTextureToBuffer

Transition / Barrier
```

`Vertex()` のような1頂点単位のAPIは禁止。

## 5.3 Resource State

最低限以下を共通状態として持つ。

```text
Undefined
CopySource
CopyDestination
VertexBuffer
IndexBuffer
ConstantBuffer
ShaderResource
UnorderedAccess
RenderTarget
DepthWrite
DepthRead
Present
```

Vulkan backend:

```text
ResourceState
↓
VkPipelineStageFlags2
VkAccessFlags2
VkImageLayout
```

D3D12 backend:

```text
ResourceState
↓
D3D12_RESOURCE_STATES
```

Metal backend:

```text
ResourceState
↓
MTLResourceUsage
render/blit/compute encoder境界
必要なbarrier / fence / event
```

MetalではVulkan/D3D12と同じ形の公開resource-state enumをそのままnative APIへ1対1対応させようとしない。RHI側でhazard intentを表現し、Metal backendがencoder境界・resource usage・barrier/eventへ適切に変換する。

OpenGL backend:

通常は論理状態追跡として保持し、必要な箇所のみGL memory barrier等へ変換する。

## 5.4 Capabilities

API名ではなく機能を問い合わせる。

最低限:

```text
MaxTextureSize
MaxSamples
SupportsCompute
SupportsStorageBuffer
SupportsTimestampQuery
SupportsAnisotropy
SupportsDebugMarkers
SupportsPresentMailbox
SupportsHdr
```

将来用:

```text
SupportsBindless
SupportsMeshShader
SupportsRayTracing
SupportsVariableRateShading
```

---

# 6. 最重要作業: Immediate Mode / Display List撤去

Vulkan/D3D12を実装する前に完了させること。

現行 `Scene::DoDlist()` はNDS RenderInstructionを、

```text
GL::Begin
GL::Color
GL::Normal
GL::TexCoord
GL::Vertex
GL::End
```

へ直接変換している。

また `GenerateLists()` はこれをOpenGL display listへcompileしている。

この構造を廃止する。

## 6.1 GPU用vertex形式

現行shaderが必要とする情報を明示的vertex dataへする。

最低候補:

```text
Position
Normal
Color
TexCoord
MatrixIndex
```

必要なpackingは後から最適化してよい。

最初は意味保存を優先する。

## 6.2 RenderInstructionを一度だけmeshへdecodeする

モデルload時:

```text
NDS RenderInstructionList
        ↓
Geometry Decoder
        ↓
CPU Vertex/Index data
        ↓
RHI VertexBuffer / IndexBuffer
```

フレームごとにinstruction streamを再解釈しない。

## 6.3 Primitive変換

以下をindex bufferへ正確に変換する。

```text
Triangles
Quads
TriangleStrip
QuadStrip
```

特に、

- winding
- stripの奇偶反転
- quad分割方向
- texture coordinate継承
- color継承
- normal継承
- `MTX_RESTORE` のMatrixIndex

を現行描画と一致させる。

## 6.4 `DIF_AMB` 特殊意味を壊さない

現行コードでは頂点color alphaを特殊フラグとしてshader側で利用している。

単純に普通のRGBA colorへ正規化して意味を失わないこと。

## 6.5 Dynamic geometry

以下はtransient bufferへ書き込む。

- particles
- trails
- collision/debug geometry
- fullscreen geometry
- 一時HUD geometry

推奨:

```text
per-frame upload ring
↓
dynamic vertex/index buffer
↓
Draw/DrawIndexed
```

1頂点ずつbackend virtual functionを呼ばない。

## 6.6 Display Listを完全撤去

最終的に通常ライブRendererから以下を撤去する。

```text
GL::GenLists
GL::NewList
GL::EndList
GL::CallList
GL::DeleteLists
Mesh::ListIdをGPU object IDとして使用する設計
```

GPU mesh cacheはRenderer側で所有する。

推奨:

```text
GpuMeshCache
Model/Mesh identity
    ↓
VertexBuffer
IndexBuffer
IndexCount
Topology
```

Gameplay/format modelへbackend固有handleを埋め込まない。

---

# 7. OpenGL backendを最初に完成させる

Vulkan/D3D12より先に、**現行OpenGLの見た目をRHI経由で再現すること。**

これは「OpenGLを残す」ためだけではなく、RHI設計の検証基準になる。

## 7.1 完了条件

`Renderer.cpp` / `Renderer.hpp` から通常描画用の直接 `GL::` 呼び出しをなくす。

Scene側は、

```text
CommandList
Pipeline
TextureHandle
BufferHandle
BindingSet
RenderTarget
```

だけを扱う。

OpenGL API呼び出しは、

```text
NativeRuntime/Rhi/OpenGL/
```

へ集約する。

Skia OpenGL interopなど、明示的なinteropは別途例外リスト化する。

## 7.2 OpenGLもbuffer based描画へ移行する

OpenGL backendだけ旧immediate modeを残してはいけない。

最終的にOpenGLも、

```text
VBO
IBO
VAO
glDrawArrays / glDrawElements
```

型へ移行する。

これによりOpenGL / Vulkan / D3D12が同じgeometry pathを使用できる。

## 7.3 OpenGL compatibility profile依存を減らす

immediate mode撤去後、GL backendはcore-profile互換の構造へ移す。

ただしprofile変更は描画一致を確認してから行う。

一度に、

```text
RHI移行
+
core profile化
+
shader全面改修
```

を同一commitへ詰め込まない。

---

# 8. Pipeline State化

現行OpenGLは描画中に多数の状態を逐次変更している。

例:

```text
DepthFunc
DepthMask
BlendFunc
AlphaFunc
StencilFunc
StencilOp
ColorMask
PolygonOffset
CullFace
PolygonMode
```

Vulkan/D3D12ではこれらの多くをPipeline Stateとして事前定義する必要がある。

## 8.1 `GraphicsPipelineDesc`

少なくとも:

```text
ShaderSet

VertexLayout
PrimitiveTopology

RasterizerState
    CullMode
    FrontFace
    FillMode
    DepthBias

DepthStencilState
    DepthTest
    DepthWrite
    DepthCompare
    StencilEnable
    FrontStencil
    BackStencil

BlendState
    BlendEnable
    Src
    Dst
    Operation
    ColorWriteMask

ColorFormats
DepthStencilFormat

BindingLayout
```

## 8.2 Pipeline cache

現行状態の組み合わせを毎drawで新規PSO生成しない。

descriptorをkeyにしてcacheする。

ただし最初から巨大なpermutation systemを作らず、現行Rendererが実際に使用している状態セットから固定Pipelineを作る。

---

# 9. Alpha Testの移植に注意

Vulkan/D3D12にはOpenGLの固定機能 `AlphaFunc` はない。

現行Rendererでは、

```text
Alpha == 1
Alpha < 1
```

を使い分け、opaque/translucent描画順とStencil制御へ組み込んでいる。

これを省略してはいけない。

shader側へ明示的なalpha-test modeを持たせる。

例:

```text
AlphaTestDisabled
AlphaEqualOne
AlphaLessThanOne
```

HLSL:

```text
clip(...)
```

GLSL:

```text
discard
```

等で同じ意味を実装する。

境界値を勝手に `>= 0.5` 等へ変更しない。

---

# 10. 現行Stencil多段描画をそのまま再現する

`Scene::OnRenderFrame()` の以下の順序は重要である。

概念:

```text
1. opaque
2. decals
3. translucent stencil pre-pass
4. depth clear
5. opaque再描画
6. translucent stencil条件別描画
7. HUD / preview
8. cel outline
9. post process
10. final composite
11. UI
12. present
```

特に現行コードが使う、

```text
Stencil Greater
Stencil Notequal
Stencil Equal

ColorMask false
DepthMask false
Depth clear
PolygonOffset
```

を単純化しない。

OpenGL/Vulkan/D3D12/Metal間で同一アルゴリズムを使用する。

Vulkan/D3D12で実現しにくいからという理由で描画順を変えない。

---

# 11. Shader設計

## 11.1 まずshader interfaceを固定する

最初に以下を明示化する。

### Vertex input

```text
POSITION
NORMAL
COLOR
TEXCOORD
MATRIX_INDEX
```

### Frame data

```text
Projection
View
ViewInverse
Fog
Global render options
```

### Object / Draw data

```text
Matrix stack
Material parameters
Texture transform
Texgen mode
Lighting
Alpha-test mode
```

### Material resources

```text
Texture
Sampler
Mask
```

## 11.2 `ShaderLocations` を最終的に廃止

現在のOpenGL uniform location IDを持つ `ShaderLocations` はbackend-neutralではない。

最終状態では、

```text
FrameConstants
DrawConstants
MaterialConstants
BindingLayout
BindingSet
```

へ置き換える。

## 11.3 canonical shader source

推奨最終形:

```text
Canonical Shader Source
   ├─ → DXIL → D3D12
   ├─ → SPIR-V → Vulkan
   ├─ → MSL / metallib → Metal
   └─ → GLSL → OpenGL
```

実装手段は固定しないが、HLSL + DXCをcanonical sourceにする場合は、Vulkan向けSPIR-Vを生成し、Metal向けにはSPIR-V Cross等によるMSL生成または同等のtoolchainを採用できる。

Metal shaderは最終的に `MTLLibrary` / `MTLFunction` からpipelineを構築する。runtimeで毎回source compileする構造を常用しない。可能ならbuild時に `metal` / `metallib` toolchainで事前コンパイルし、runtimeはprecompiled libraryをロードする。

Apple Silicon / Intel Mac間でshader semanticsを分岐させない。

ただしshader source一本化そのものをVulkan/D3D12初期bring-upのブロッカーにしない。

必要であれば移行途中は、

```text
GLSL: OpenGL
HLSL: Vulkan/D3D12
MSL: Metal
```

を一時併存させてもよい。

ただし、

- constant layout
- binding layout
- vertex semantics
- algorithm
- alpha behavior
- fog
- lighting
- cel shading

は同一仕様にする。

最終的には重複shader実装を減らす。

---

# 12. Resource Binding

OpenGL texture unit直接操作をRenderer Frontendから撤去する。

推奨共通モデル:

```text
BindingLayout
BindingSet
```

例:

```text
Set 0: Frame
    Camera constants
    Fog
    global options

Set 1: Material
    texture
    sampler
    material constants

Set 2: Draw/Object
    matrix stack
    object constants
```

Vulkan:

```text
DescriptorSetLayout
DescriptorSet
```

D3D12:

```text
RootSignature
DescriptorHeap
DescriptorTable
```

Metal:

```text
buffer / texture / sampler binding
argument buffer / argument table（採用する場合）
```

初期実装では単純な明示bindingでよい。Metal固有のargument buffer最適化をRHI必須仕様にしない。

OpenGL:

```text
UBO binding
texture unit
sampler
```

へ各backendが変換する。

Scene側はtexture unit番号を直接意識しない。

---

# 13. Texture / Render Target / Depth

現行以下のOpenGL objectをRHI resourceへ移す。

```text
_screenTexture
_celTexture
_depthTexture
_renderBuffer
_frameBuffer
_celFrameBuffer
```

推奨概念:

```text
SceneColor
SceneDepthStencil
CelColor
CelDepth
SwapchainBackBuffer
```

必要に応じてTextureViewを分離する。

## 13.1 depth format

現行OpenGLの `Depth24Stencil8` 相当を基準にする。

backendごとに対応formatを選択する。

例:

```text
OpenGL: DEPTH24_STENCIL8
Vulkan: D24_UNORM_S8_UINT が利用可能なら使用
D3D12: DXGI_FORMAT_D24_UNORM_S8_UINT
Metal: MTLPixelFormatDepth24Unorm_Stencil8 が利用可能なら使用、
       非対応環境では Depth32Float_Stencil8 等へCapabilities経由で切替
```

利用不可の場合はCapabilitiesで明示し、互換formatへ切り替える。

勝手に精度を下げない。

## 13.2 Cel outline

現行cel outlineはscene depth textureを参照する。

Vulkan/D3D12では、

```text
DepthWrite
↓ barrier
DepthRead / ShaderResource
↓
Cel outline pass
```

を明示する。

## 13.3 readback

以下をGL固有 `ReadPixels` からRHI readbackへ置換する。

- screenshot
- recording
- diagnostics
- scene target capture

推奨:

```text
Texture
↓ CopyTextureToBuffer
Readback buffer
↓ fence
CPU
```

通常描画を毎フレーム同期readbackしない。

---

# 14. FrameContext / GPU同期

Vulkan/D3D12/MetalではGPU完了を前提に即時resource破棄してはならない。

`FrameContext` を導入する。

例:

```text
FrameContext[0]
FrameContext[1]
```

初期値は低遅延を優先し2 frames in flight程度でよい。

将来3へ変更可能な設計にする。

各FrameContext:

```text
Command allocator / pool
Command list / buffer

Upload allocator
Transient buffer allocator
Descriptor allocator

Fence value

Deferred destruction list
```

## 14.1 Deferred destruction

禁止:

```text
DestroyTexture()
↓
backend objectを即delete
```

GPUが使用中の可能性がある。

推奨:

```text
Destroy requested
↓
Retire fence valueを記録
↓
fence完了
↓
native object実破棄
```

OpenGL backendでも同じ上位lifetime契約を維持する。

---

# 15. WindowとSwapchainを分離する

現行 `RendererPlatform::Window` はOpenGL contextと `SwapBuffers()` を所有している。

これをbackend-neutralなWindowへ変更する。

## 15.1 Windowの責務

```text
OS window
size
position
fullscreen
focus
mouse/keyboard callbacks
native handle
```

## 15.2 Swapchainの責務

```text
back buffers
present
vsync/present mode
resize
surface format
```

`Present()` はRHI Swapchainが行う。

## 15.3 GLFW

OpenGL:

```text
GLFW OpenGL context
```

Vulkan/D3D12:

```text
GLFW_CLIENT_API = GLFW_NO_API
```

Vulkan:

```text
GLFW/Vulkan surface
```

D3D12 Windows:

```text
glfwGetWin32Window()
↓
HWND
↓
DXGI SwapChain
```

Metal macOS:

```text
NSWindow / NSView
↓
CAMetalLayer
↓
CAMetalDrawable
↓
drawable.texture
↓
command bufferでpresent
```

GLFWを継続利用する場合、Cocoa native window/viewをplatform adapterから取得し、`CAMetalLayer` をattachする。Metal backendのためだけにGame/Scene側へObjective-C型を漏らさない。

## 15.4 resize

window resizeでDevice全体を作り直さない。

```text
wait/release affected backbuffer usage
↓
swapchain resize/recreate
↓
render targets再生成
```

minimize時の0x0 framebufferを正しく扱う。

---

# 16. OpenGL backend

OpenGLは既存Rendererの「特別扱い」ではなくRHI backendの一つにする。

責務:

```text
RHI Buffer → GL buffer
RHI Texture → GL texture
RHI Pipeline → GL program + fixed state bundle
RHI BindingSet → texture/UBO bindings
RHI CommandList → OpenGL calls
RHI Swapchain → context + glfwSwapBuffers
```

OpenGL global stateの重複変更を減らすため、backend内にstate cacheを持ってよい。

ただしFrontendからGL stateを触らない。

---

# 17. Vulkan backend

## 17.1 初期実装方針

最初は単純構成でよい。

```text
1 graphics queue
1 present queue
可能なら同一queue family
2 frames in flight
1 primary command buffer / frame
```

最初からasync computeや複雑なmulti-queue化をしない。

## 17.2 推奨機能

ターゲット環境が許すならVulkan 1.3を優先する。

推奨:

```text
Dynamic Rendering
Synchronization2
Timeline Semaphore
Debug Utils
```

ただしターゲットGPU互換性のためVulkan 1.2へ落とす必要がある場合はCapabilitiesで扱う。

## 17.3 Validation

Debug buildではValidation Layerを有効化可能にする。

完了判定では少なくとも以下を解消する。

```text
ERROR
resource lifetime violation
layout mismatch
invalid descriptor
invalid synchronization
use-after-free
```

validation warningを無視して完了扱いしない。

## 17.4 Pipeline cache

VkPipelineをdrawごとに生成しない。

PipelineDescからcacheする。

## 17.5 Swapchain

最低限:

```text
FIFO
ImmediateまたはMailboxが利用可能ならVSync設定に応じて選択
```

resize / out-of-date / suboptimalを処理する。

---

# 18. Direct3D 12 backend

## 18.1 対象

Windows C++のみ。

非WindowsでD3D12を選択した場合は明確なunsupported errorを返す。

## 18.2 初期構成

```text
ID3D12Device
Direct Command Queue
Command Allocator per FrameContext
Graphics Command List
DXGI SwapChain
RTV heap
DSV heap
CBV/SRV/UAV descriptor heap
Sampler heap
Fence
```

## 18.3 Root Signature

RHI `BindingLayout` からRoot Signatureを構築する。

Root Signatureをゲームコード側で直接記述しない。

## 18.4 Resource State

RHI ResourceStateをD3D12_RESOURCE_STATESへ変換する。

初期段階は通常のResourceBarrierでよい。

Enhanced Barriersは後の最適化として扱う。

## 18.5 Debug Layer

Debug buildでD3D12 debug layerを有効化可能にする。

可能ならGPU-based validationも診断モードとして追加する。

完了判定では、

```text
resource state mismatch
descriptor misuse
GPU lifetime error
device removed
```

を残さない。

---

# 19. Android Vulkan backend

## 19.1 対象

`src/MphRead.Native.Android` を対象に、既存EGL/GLES rendererと並列でVulkan rendererを追加する。

Android VulkanはDesktop Vulkanの別実装ではなく、**同じRHI Vulkan backendをAndroid platform surfaceへ接続する構成**を優先する。

理想形:

```text
Renderer Frontend
      │
      ▼
      RHI
      │
      ▼
Vulkan Backend
   ├─ Desktop Surface Adapter
   └─ Android Surface Adapter
```

Vulkan resource / pipeline / descriptor / command buffer実装をDesktop用とAndroid用に複製しない。

## 19.2 Android platform surface

Androidでは、

```text
ANativeWindow
↓
VkAndroidSurfaceCreateInfoKHR
↓
vkCreateAndroidSurfaceKHR
↓
VkSurfaceKHR
```

でpresentation surfaceを作る。

`ANativeWindow*` はAndroid platform adapter内に閉じ込める。

Scene、Renderer Frontend、RHI共通headerへAndroid NDK型を漏らさない。

## 19.3 Vulkan loader

AndroidではシステムVulkan loaderを使用する。

CMakeではAndroid Vulkan libraryを明示的に解決する。

例:

```cmake
find_library(FRUITY_ANDROID_VULKAN_LIBRARY vulkan REQUIRED)
```

またはNDKのVulkan package/import targetを利用してよい。

Vulkan backendを有効化していないAndroid buildへ不要なVulkan link依存を強制しない。

## 19.4 Device / queue selection

最低限以下を確認する。

```text
graphics queue
present support
required surface extensions
swapchain support
required image formats
required synchronization features
```

Android端末ではqueue familyやsurface capabilityがDesktopと異なる場合があるため、Desktopのdevice selection結果を仮定しない。

## 19.5 Swapchain

AndroidではActivity / Surface lifecycleに合わせてswapchainを管理する。

特に以下を正しく処理する。

```text
surface created
surface changed
surface destroyed
app pause/resume
orientation change
window size change
VK_ERROR_OUT_OF_DATE_KHR
VK_SUBOPTIMAL_KHR
```

Surface消失時にdevice全体を必ず破棄する設計にしない。

可能ならdevice/resourceを維持し、surface/swapchainだけ再生成する。

## 19.6 App lifecycle

Androidでは描画可能状態とActivity状態が一致しない場合がある。

以下を分離する。

```text
App alive
Game state alive
Device alive
Surface alive
Swapchain alive
```

Surfaceがない間はpresentしない。

Surface復帰時に、

```text
VkSurfaceKHR
Swapchain
backbuffer views
dependent render targets
```

を再構築する。

network/game stateを不要にリセットしない。

## 19.7 Orientation / resolution / density

Androidでは、

```text
logical UI size
surface pixel size
display density
orientation
safe inset
```

を混同しない。

Swapchain extentは実surface pixel寸法を使用する。

UI座標系は既存Android入力/UI契約を維持する。

縦横切替でscene projection、HUD、UI、touch mappingがずれないこと。

## 19.8 Frames in flight

Android VulkanもDesktop Vulkanと同じFrameContext契約を使用する。

端末メモリとlatencyを考慮し、初期値は2 frames in flightを推奨する。

以下を端末ごとに無制限増加させない。

```text
command pools
command buffers
descriptor pools
staging buffers
temporary textures
fences/semaphores
```

## 19.9 Memory

Android GPUはdiscrete desktop GPUと異なるmemory architectureを持つことが多い。

memory type選択をDesktop前提で固定しない。

少なくとも用途を分離する。

```text
GPU-only resource
CPU upload resource
readback resource
transient resource
```

memory budgetが厳しい端末で、

```text
scene texture duplicate
staging buffer常駐
swapchain依存resource過剰保持
```

を避ける。

## 19.10 Texture format

Android端末のformat supportを必ずCapabilitiesで確認する。

特に:

```text
depth/stencil
sampled depth
render target
sRGB
compressed texture
```

をDesktopと同一と仮定しない。

Cel outline用depth samplingが必要な場合、sampleableなdepth formatを選択する。

利用不可なら同等の別formatへ切り替える。

## 19.11 Pipeline cache

Androidではpipeline compilation costが大きくなり得る。

`VkPipelineCache` を利用可能な構造にする。

ただし初期bring-upではcorrectnessを優先し、cache persistenceを完成条件の必須にしなくてよい。

runtime中にdrawごとpipeline生成することは禁止。

## 19.12 Validation

Android debug buildでValidation Layerを利用可能にする。

ただしshipping buildへvalidationを必須依存させない。

最低限以下を検出・解消する。

```text
invalid descriptor
layout mismatch
resource lifetime violation
bad synchronization
use-after-free
swapchain misuse
surface misuse
```

## 19.13 RenderDoc / GPU capture

対応端末ではRenderDoc等でcapture可能な構成を維持する。

debug name / debug labelを付け、

```text
Scene
Cel
HUD
UI
Present
```

が追跡できるようにする。

## 19.14 GLESとの併存

Android Vulkan追加のためにGLESを撤去しない。

backend選択は少なくとも以下を許可する。

```text
OpenGLES
Vulkan
Auto
```

`Auto` の選択方針は明文化する。

推奨:

```text
Vulkan利用可能かつ必要機能あり
    ↓ yes
Vulkan
    ↓ no
OpenGLES
```

ただしユーザーが明示的に `Vulkan` を選択した場合、失敗時に黙ってGLESへfallbackしない。

明示的なエラーを返す。

## 19.15 Android UI / Skia

Android Vulkan時もUIをCPU full-surface rendererへ戻さない。

既存Android UIがGLES surfaceへ依存している箇所を監査する。

最終的には、

```text
Game Vulkan
UI Vulkan-compatible GPU path
Present Vulkan
```

を同一frame ownershipの下で実現する。

OpenGL ES sidecar contextをVulkanモードでhidden生成しない。

---

# 20. Metal backend

## 20.1 対象

macOS C++を主対象とする。

MetalはmacOSでの第一級native backendとして実装する。

初期実装のbaselineは、広いmacOS互換性を確保できる従来のMetal command queue / command buffer / encoderモデルでよい。

Appleの新しいMetal 4 APIを利用可能な環境で追加採用してもよいが、Metal 4専用実装にして古い対応Macを不要に切り捨てない。

Metal 4を利用する場合も、RHI契約は変えずCapabilitiesで選択する。

## 20.2 初期構成

最低限:

```text
MTLDevice
MTLCommandQueue
MTLCommandBuffer
MTLRenderCommandEncoder
MTLBlitCommandEncoder

CAMetalLayer
CAMetalDrawable

MTLBuffer
MTLTexture
MTLSamplerState

MTLLibrary
MTLFunction
MTLRenderPipelineState
MTLDepthStencilState
```

Objective-C++実装は `.mm` に閉じ込める。

通常のC++ frontend/headerへ、

```text
id<MTLDevice>
id<MTLTexture>
CAMetalLayer*
```

等を公開しない。

## 20.3 Presentation

MetalのSwapchain相当はRHI上では `Swapchain` として扱うが、native実装は `CAMetalLayer` / `CAMetalDrawable` で行う。

フレームごとに:

```text
drawable取得
↓
drawable.textureをrender targetとして使用
↓
command encoding
↓
present drawable
↓
commit
```

drawableをフレーム開始時から長時間保持しない。

必要になる直前に取得し、present登録後は不要なstrong referenceを速やかに解放する。

window resize / backing scale変更時は、

```text
CAMetalLayer.drawableSize
```

を物理pixel寸法へ正しく更新する。

Retina scaleをlogical pointと混同しない。

## 20.4 Command encoding

RHIの:

```text
BeginRendering
SetPipeline
SetVertexBuffer
SetIndexBuffer
SetBindingSet
Draw
DrawIndexed
EndRendering
Copy*
```

を、

```text
MTLRenderCommandEncoder
MTLBlitCommandEncoder
```

へ変換する。

OpenGLのようにフレーム全体で暗黙のglobal stateを保持しない。

render pass境界を明示する。

## 20.5 Pipeline

`GraphicsPipelineDesc` から、

```text
MTLRenderPipelineDescriptor
MTLRenderPipelineState
MTLDepthStencilDescriptor
MTLDepthStencilState
```

を生成する。

以下をpipeline keyへ含める。

```text
vertex layout
shader functions
color format
depth/stencil format
blend state
raster state
sample count
```

pipeline stateをdrawごとに生成しない。

## 20.6 Resource binding

初期版では単純なindex-based bindingでよい。

RHI BindingLayoutから、

```text
setVertexBuffer
setFragmentBuffer
setVertexTexture
setFragmentTexture
setVertexSamplerState
setFragmentSamplerState
```

へbackend内部で変換する。

将来argument buffers / Metal 4 argument tablesを利用してもよいが、他backendへMetal固有概念を強制しない。

## 20.7 Resource synchronization

MetalはVulkan/D3D12とresource transition表現が異なる。

そのため、

```text
RHI ResourceState
```

を無理に `MTLResource` の単一stateへ変換しない。

backendは以下を追跡する。

```text
resource usage
encoder ownership
render/blit/compute dependency
CPU/GPU visibility
frame retirement
```

必要に応じて、

```text
memory barrier
MTLFence
MTLEvent / MTLSharedEvent
```

等を使用する。

同一queue・順序保証だけで安全なケースへ不要な同期を乱発しない。

## 20.8 Storage mode

resource用途に応じて適切に選択する。

例:

```text
CPU upload:
    Shared または Managed相当

GPU-only:
    Private

Readback:
    CPU-visible storage
```

Apple Siliconのunified memoryを理由に、全resourceをShared固定にしない。

Intel MacとApple Siliconの差はbackend内部のmemory policyへ閉じ込める。

## 20.9 Frames in flight

他backendと同じFrameContext契約を使う。

Metalのみ別のゲームフレーム管理方式を作らない。

最低限:

```text
FrameContext
command buffer
transient upload area
retire/completion value
deferred destruction
```

を対応させる。

`MTLCommandBuffer` completion handler等を利用してretirementを処理してよい。

## 20.10 Capture / Validation

Debug buildではMetal API Validationを利用可能にする。

Xcode GPU Frame Captureで、

```text
Scene pass
Cel pass
HUD pass
UI pass
Present
```

を追跡できるようdebug labels / groupsを付ける。

Shader Validation / resource hazard / invalid encoder usageを残したまま完成扱いしない。

---

# 21. Skia / Launcher / Map Vote / UI

ここは必須。

Vulkan/D3D12でゲーム本体だけ描画できても、Launcher・Pause・Map Voteが動かなければbackend完成ではない。

## 21.1 現行フレーム順を維持

ゲーム中:

```text
Scene
↓
HUD
↓
Cel/Post Process
↓
Shell::TickUi
↓
UiOverlay
↓
LauncherHunter
↓
Shell::AfterDraw
↓
Present
```

Launcher単独時のUI-only pathも維持する。

## 21.2 cross-API compositor禁止

禁止:

```text
Game = Vulkan
UI = hidden OpenGL context
```

禁止:

```text
Game = D3D12
UI = hidden OpenGL context
```

同一フレームを別APIへ無理に跨がせない。

## 21.3 推奨

Skiaが選択backendを直接使用できる構成にする。

OpenGL:

```text
Skia Ganesh GL
```

Vulkan:

```text
Skia Ganesh/Graphite Vulkan
```

D3D12:

```text
Skia D3D12 backend
```

Metal:

```text
Skia Metal backend
```

Skia側ではMetal対応backendを使用し、ゲーム本体と同一 `MTLDevice` / command submission系統との所有権を明確にする。

選択したSkia build/packageがVulkan/D3D12/Metal backendを含んでいない場合、

- Skia build optionを修正する
- またはRHI上へUI描画を移す

のどちらかを行う。

**CPU full-surface rendererへの退避は禁止。**

## 21.4 OpenGL state snapshot hack

現行 `SkiaGpu.cpp` はOpenGL global stateを大量に保存・復元している。

これはOpenGL backend内のinteropとして一時的に残してよい。

Vulkan/D3D12/Metalでは明示pass / resource state / command recordingに置き換える。

Skia固有OpenGL state restoreを共通RHI契約へ持ち込まない。

---

# 22. Backend選択

共通enumを作る。

例:

```text
RendererBackend
    OpenGL
    OpenGLES
    Vulkan
    D3D12
    Metal
    Auto
```

設定名は既存CLI/settings設計へ合わせる。

初期段階ではOpenGLをdefaultのまま維持してよい。

出力ログには必ず以下を出す。

```text
selected backend
GPU adapter
driver/API version
swapchain format
depth format
frames in flight
validation/debug mode
```

---

# 23. 実行中Backend切替

これはVulkan/D3D12の基本描画とUIが安定した後に実装する。

ゲーム状態とGPU状態を分離する。

```text
Game State
Scene CPU Data
Assets CPU Data
        │
        │ survive
        ▼
--------------------------------
        │
Renderer / GPU resources
        │ recreate
        ▼
OpenGL / Vulkan / D3D12
```

切替手順:

```text
1. 新規frame受付停止
2. GPU WaitIdle
3. UI GPU resource破棄
4. Scene GPU cache破棄
5. Swapchain破棄
6. backend device破棄
7. 新backend生成
8. Swapchain/CAMetalLayer presentation resource生成
9. shader/pipeline再生成
10. texture/mesh GPU cache再構築
11. UI GPU resource再生成
12. render再開
```

ゲームロジック、match state、network connectionまで再生成しない。

---

# 24. CMake再設計

現行はdesktop C++共通ライブラリが直接、

```cmake
find_package(OpenGL REQUIRED)
target_link_libraries(fruity_mphread_native PUBLIC OpenGL::GL)
```

している。

これをbackend単位へ分離する。

推奨option:

```cmake
FRUITY_RENDERER_OPENGL
FRUITY_RENDERER_VULKAN
FRUITY_RENDERER_D3D12
FRUITY_RENDERER_METAL

# Android
FRUITY_ANDROID_RENDERER_GLES
FRUITY_ANDROID_RENDERER_VULKAN
```

推奨target概念:

```text
fruity_rhi
fruity_rhi_opengl
fruity_rhi_vulkan
fruity_rhi_d3d12
fruity_rhi_metal
```

OpenGL依存を共通RHIへPUBLIC伝播させない。

Vulkan:

```cmake
find_package(Vulkan ...)
```

D3D12:

Windows toolchainで、

```text
d3d12
dxgi
dxguid
```

等をbackend targetへPRIVATE linkする。

Metal:

Apple toolchainでのみ有効化し、backend targetへ必要なframeworkをPRIVATE linkする。

例:

```text
Metal
QuartzCore
Foundation
AppKit
```

必要に応じてMetalKitを追加する。

Objective-C++ `.mm` をMetal backend targetへ限定し、共通C++ target全体をObjective-C++化しない。


Android Vulkan:

Androidでは既存の、

```text
android
log
EGL
GLESv3
```

に加えてVulkanをbackend optionに応じてlinkする。

概念:

```cmake
if(ANDROID AND FRUITY_ANDROID_RENDERER_VULKAN)
    find_library(FRUITY_ANDROID_VULKAN_LIBRARY vulkan REQUIRED)
    target_link_libraries(fruity_rhi_vulkan PRIVATE
        "${FRUITY_ANDROID_VULKAN_LIBRARY}")
endif()
```

GLESしか選ばれていないAndroid buildへVulkanを必須化しない。

逆にVulkan-only compile testではEGL/GLES rendererへ不要依存しない構成を目指す。

## 23.1 Windows toolchain

MSVC / MinGWの双方を必要とする場合、D3D12 header/library availabilityを実際にcompile probeする。

`WIN32` だから使えると仮定しない。

未対応toolchainでダミーD3D12実装を作ってビルドだけ通してはならない。

---

# 25. Render Graph

RHIが安定する前に巨大Render Graphを作らない。

まず:

```text
RHI
+
ResourceStateTracker
+
明示Pass
```

でOpenGL/Vulkan/D3D12/Metalを完成させる。

その後必要なら、

```text
ScenePass
CelPass
PostProcessPass
HudPass
UiPass
PresentPass
```

をRender Graphへ昇格する。

Render Graphの目的:

- resource dependency
- barrier生成
- temporary texture lifetime
- pass ordering
- transient resource reuse

であり、backend差分隠蔽そのものではない。

---

# 26. 実装フェーズ

---

## Phase 0: Baseline固定

- [ ] 作業開始直前の `develop2` HEAD SHAを記録
- [ ] OpenGL現行スクリーンショット基準を取得
- [ ] Launcher基準を取得
- [ ] Offline match基準を取得
- [ ] Map Vote基準を取得
- [ ] Pause Menu基準を取得
- [ ] Cel shading ON/OFF基準を取得
- [ ] HUD基準を取得
- [ ] end screen基準を取得
- [ ] scene load/unloadを複数回実行して現行挙動を記録
- [ ] CPUレンダリング撤去済み状態を再確認

**このPhaseでは挙動変更禁止。**

---

## Phase 1: Window / Backend選択分離

- [ ] `RendererBackend` 導入
- [ ] WindowからOpenGL固有責務を分離
- [ ] `SwapBuffers()` を将来RHI Swapchainへ移せる構造にする
- [ ] Vulkan/D3D12用 `GLFW_NO_API` window生成経路追加
- [ ] native window handle取得をplatform adapterへ分離
- [ ] OpenGL既存挙動に回帰がないことを確認

完了条件:

> OpenGLで従来どおり動くが、Window生成が「必ずGL contextを作る」設計ではなくなっている。

---

## Phase 2: RHI Core

- [ ] Device
- [ ] CommandList
- [ ] Buffer
- [ ] Texture
- [ ] Sampler
- [ ] Pipeline
- [ ] BindingLayout
- [ ] BindingSet
- [ ] Swapchain
- [ ] FrameContext
- [ ] ResourceState
- [ ] Capabilities
- [ ] debug name API
- [ ] deferred destruction契約

この時点ではOpenGL backendのみ実装してよい。

---

## Phase 3: Geometry modernization

- [ ] `DoDlist()` をCPU geometry decoderへ分離
- [ ] vertex format固定
- [ ] index生成
- [ ] Triangles変換
- [ ] Quads変換
- [ ] TriangleStrip変換
- [ ] QuadStrip変換
- [ ] `MTX_RESTORE` → MatrixIndex化
- [ ] `DIF_AMB`意味保存
- [ ] model GPU mesh cache導入
- [ ] transient dynamic geometry導入
- [ ] display list撤去
- [ ] OpenGLもVBO/IBO描画化

完了条件:

> 通常ライブゲーム描画で `GL::Begin/End` とdisplay listが不要になっている。

---

## Phase 4: OpenGL完全RHI化

- [ ] shader creationをOpenGL backendへ移動
- [ ] uniform updateを共通constant/bindingへ移動
- [ ] texture creation/uploadをRHIへ移動
- [ ] framebuffer/depth resourceをRHIへ移動
- [ ] pipeline state化
- [ ] stencil state化
- [ ] alpha test shader化
- [ ] readback RHI化
- [ ] Sceneからraw GL resource ID撤去
- [ ] `Renderer.cpp` の直接GL描画を撤去
- [ ] OpenGL描画一致確認

**Vulkanへ進む前にここを完遂する。**

---

## Phase 5: Shader interface統一

- [ ] explicit vertex input
- [ ] Frame constants
- [ ] Material constants
- [ ] Draw/Object constants
- [ ] matrix stack binding
- [ ] texture/sampler binding
- [ ] alpha-test mode
- [ ] fog parity
- [ ] lighting parity
- [ ] texgen parity
- [ ] cel shader parity
- [ ] RTT shader parity
- [ ] shift/whiteout parity
- [ ] `ShaderLocations` OpenGL依存撤去

---

## Phase 6: Vulkan bring-up

順序:

- [ ] Instance
- [ ] Physical device selection
- [ ] Device
- [ ] Queue
- [ ] Surface
- [ ] Swapchain
- [ ] FrameContext
- [ ] Command buffer
- [ ] Fence/Semaphore
- [ ] Buffer
- [ ] Texture/Image
- [ ] Sampler
- [ ] Descriptor
- [ ] Pipeline
- [ ] SceneColor
- [ ] DepthStencil
- [ ] main scene
- [ ] stencil/translucent passes
- [ ] cel
- [ ] post process
- [ ] HUD
- [ ] readback
- [ ] resize
- [ ] fullscreen
- [ ] minimize/restore

この時点ではUI未完成でもscene診断用にbring-upしてよいが、backend完成扱いにはしない。

---

## Phase 7: Vulkan UI統合

- [ ] Launcher
- [ ] Pause Menu
- [ ] Map Vote
- [ ] End Screen
- [ ] Settings
- [ ] UiOverlay
- [ ] LauncherHunter
- [ ] Skia Vulkan integrationまたはRHI UI path
- [ ] Game → UI → Present順序確認
- [ ] GL sidecar contextが存在しないことを確認

---

## Phase 8: D3D12 bring-up

Vulkanと同じRenderer Frontend / RHI契約を使う。

- [ ] Device
- [ ] Command Queue
- [ ] Swapchain
- [ ] FrameContext
- [ ] Command Allocator
- [ ] Command List
- [ ] Fence
- [ ] descriptor heaps
- [ ] Buffer
- [ ] Texture
- [ ] Sampler
- [ ] Pipeline State
- [ ] Root Signature
- [ ] main scene
- [ ] stencil/translucent passes
- [ ] cel
- [ ] post process
- [ ] HUD
- [ ] readback
- [ ] resize
- [ ] fullscreen
- [ ] minimize/restore

---

## Phase 9: D3D12 UI統合

- [ ] Launcher
- [ ] Pause Menu
- [ ] Map Vote
- [ ] End Screen
- [ ] Settings
- [ ] UiOverlay
- [ ] LauncherHunter
- [ ] Skia D3D12 integrationまたはRHI UI path
- [ ] GL sidecar contextが存在しないことを確認

---

## Phase 10: Metal bring-up

OpenGL/Vulkan/D3D12と同じRenderer Frontend / RHI契約を使う。

- [ ] `MTLDevice` 選択
- [ ] command queue
- [ ] `CAMetalLayer`
- [ ] drawable取得 / present
- [ ] FrameContext
- [ ] command buffer
- [ ] render encoder
- [ ] blit encoder
- [ ] Buffer
- [ ] Texture
- [ ] Sampler
- [ ] shader library
- [ ] pipeline state
- [ ] depth/stencil state
- [ ] main scene
- [ ] stencil/translucent passes
- [ ] cel
- [ ] post process
- [ ] HUD
- [ ] readback
- [ ] resize
- [ ] Retina backing scale
- [ ] fullscreen
- [ ] minimize/restore
- [ ] deferred destruction
- [ ] Metal API Validation clean

---

## Phase 11: Metal UI統合

- [ ] Launcher
- [ ] Pause Menu
- [ ] Map Vote
- [ ] End Screen
- [ ] Settings
- [ ] UiOverlay
- [ ] LauncherHunter
- [ ] Skia Metal integrationまたはRHI UI path
- [ ] Game → UI → Present順序確認
- [ ] hidden OpenGL sidecar contextが存在しないことを確認
- [ ] CPU full-surface fallbackが存在しないことを確認

---

## Phase 12: Android Vulkan bring-up

Desktop VulkanのRHI Vulkan実装を再利用し、Android platform surface/lifecycleを接続する。

- [ ] Android Vulkan loader/link
- [ ] instance extensions
- [ ] physical device
- [ ] graphics/present queue
- [ ] `ANativeWindow`
- [ ] `VkSurfaceKHR`
- [ ] swapchain
- [ ] FrameContext
- [ ] command pool/buffer
- [ ] fence/semaphore
- [ ] Buffer
- [ ] Texture/Image
- [ ] Sampler
- [ ] Descriptor
- [ ] Pipeline
- [ ] SceneColor
- [ ] DepthStencil
- [ ] main scene
- [ ] stencil/translucent passes
- [ ] cel
- [ ] post process
- [ ] HUD
- [ ] readback
- [ ] Android UI integration
- [ ] orientation change
- [ ] density/backing size
- [ ] pause/resume
- [ ] surface destroy/recreate
- [ ] app background/foreground
- [ ] validation clean

---

## Phase 13: Android GLES/Vulkan parity

- [ ] 同一端末でGLES/Vulkan描画比較
- [ ] texture orientation一致
- [ ] depth/stencil一致
- [ ] alpha/translucent一致
- [ ] HUD一致
- [ ] touch coordinate一致
- [ ] Map Vote一致
- [ ] pause/resume一致
- [ ] orientation change一致
- [ ] repeated surface recreationでleakなし
- [ ] Vulkan選択時にhidden GLES contextなし
- [ ] GLES選択時にVulkan不要依存なし

---

## Phase 14: Backend切替

- [ ] OpenGL → Vulkan
- [ ] Vulkan → OpenGL
- [ ] OpenGL → D3D12
- [ ] D3D12 → OpenGL
- [ ] Vulkan → D3D12
- [ ] D3D12 → Vulkan
- [ ] OpenGL → Metal
- [ ] Metal → OpenGL
- [ ] Vulkan → Metal
- [ ] Metal → Vulkan
- [ ] D3D12 → Metal（対応プラットフォーム上で両方が存在する将来構成のみ）
- [ ] Metal → D3D12（同上）
- [ ] scene state維持
- [ ] network session維持
- [ ] GPU resources正常再構築
- [ ] UI resource正常再構築
- [ ] resource leakなし
- [ ] crashなし

Desktop runtime hot switchingをこのPhaseで実装しない場合でも、startup backend選択は必須。

AndroidについてはActivity/Surface lifecycleとの複雑性があるため、**startup時のGLES/Vulkan選択を必須**とし、実行中hot switchingは後回しにしてよい。

---

# 27. 描画一致テスト

各backendで同じ固定条件を描画する。

最低限:

```text
Launcher
Offline room
Hunter model
weapon
transparent surface
decal
particle
trail
fog
cel shading
cel outline
HUD
Pro HUD
Pause Menu
Map Vote
End Screen
fade
whiteout
HUD disruption
movie frame
```

## 26.1 Golden image

同じ、

```text
ROM
map
hunter
camera
frame
resolution
settings
```

で画像を取得する。

比較:

```text
Desktop OpenGL ↔ Vulkan
Desktop OpenGL ↔ D3D12
Desktop OpenGL ↔ Metal
Vulkan ↔ D3D12
Vulkan ↔ Metal
D3D12 ↔ Metal（同一環境で実行可能な場合のみ）
Android OpenGLES ↔ Vulkan
```

GPU/API差による数bit差は許容してよいが、

- missing texture
- wrong UV
- wrong winding
- depth inversion
- stencil failure
- alpha ordering failure
- color-space mismatch
- half-pixel offset
- UI position difference
- cel edge difference
- flipped framebuffer

は許容しない。

---

# 28. GPU診断

## OpenGL

- KHR_debug等が利用可能なら有効化
- GL errorを通常render pathの設計へ依存させない

## Vulkan

- Validation Layer
- Debug Utils
- RenderDoc

## D3D12

- D3D12 Debug Layer
- PIX
- 必要に応じてGPU-based validation

## Metal

- Metal API Validation
- Xcode GPU Frame Capture
- Shader Validation
- debug labels / debug groups

## Android Vulkan

- Vulkan Validation Layers（Debug）
- RenderDoc等のGPU capture
- Android GPU Inspectorを利用可能なら活用
- surface/swapchain lifecycle logging
- debug labels / object names

共通:

- backend objectへdebug nameを付与
- SceneColor
- SceneDepth
- pipelines
- textures
- model buffers

をcapture上で識別可能にする。

---

# 29. Resource leak / freeze検証

このプロジェクトでは長時間実行時のfreeze調査履歴があるため、backend追加でresource lifetime問題を持ち込まないこと。

最低限:

- [ ] match開始/終了を100回相当繰り返す
- [ ] map切替を繰り返す
- [ ] Launcher ↔ gameを繰り返す
- [ ] fullscreen切替を繰り返す
- [ ] resizeを繰り返す
- [ ] backend再生成を繰り返す
- [ ] Android surface destroy/recreateを繰り返す
- [ ] Android pause/resumeを繰り返す
- [ ] Android orientation changeを繰り返す
- [ ] GPU memory増加が継続しない
- [ ] descriptor count増加が継続しない
- [ ] command allocator/buffer増加が継続しない
- [ ] texture/buffer count増加が継続しない
- [ ] deleted resourceをGPUが参照しない

---

# 30. CI

最低限以下をcompile gateへする。

## Windows

```text
OpenGL
Vulkan
D3D12
```

## Linux

```text
OpenGL
Vulkan
```

## macOS

最低限:

```text
OpenGL
Metal
```

MetalをmacOSの第一級native backendとしてcompile/link gateへ含める。

可能であればApple Silicon runnerでMetal startup smoke testも実施する。

MoltenVKによるVulkanは別途有効化してよい。Vulkan-on-MetalのCIは有用だが、native Metal backendの代替にはしない。

## Android

最低限:

```text
OpenGLES
Vulkan
```

を別々のcompile/link gateにする。

可能なら、

```text
arm64-v8a GLES
arm64-v8a Vulkan
x86_64 GLES
x86_64 Vulkan
```

を確認する。

CIで実機GPU実行ができない場合でも、

```text
configure
compile
link
native tests
```

をbackend単位で分ける。

実機検証では少なくとも1台以上のVulkan対応Android端末でstartup、scene描画、UI、pause/resume、surface再生成まで確認する。

GLES buildをVulkan追加のために壊さない。

---

# 31. 静的監査

移行後は直接GL呼び出しの残存を機械的に監査する。

例:

```bash
rg -n "OpenTK::Graphics::OpenGL|GL::" src/MphRead.Native \
  -g "*.cpp" -g "*.hpp"
```

残存を以下に分類する。

```text
A. OpenGL RHI backend
B. 明示されたSkia/OpenGL interop
C. Android GLES専用
D. diagnostics
E. 不正残存
```

`E` はゼロにする。

同様に、

```bash
rg -n "Vk[A-Z]|vk[A-Z]" src/MphRead.Native
rg -n "ID3D12|D3D12_|DXGI_" src/MphRead.Native
rg -n "MTL[A-Z]|CAMetal|<Metal/|<QuartzCore/" src/MphRead.Native
rg -n "ANativeWindow|VK_KHR_android_surface|VkAndroidSurfaceCreateInfoKHR" src/MphRead.Native src/MphRead.Native.Android
```

を行い、backend外へのnative API leakがないことを確認する。

Metal native型は原則として `NativeRuntime/Rhi/Metal/` の `.mm` / private implementationへ閉じ込める。

---

# 32. 禁止事項

以下を行ってはならない。

- [ ] Scene内へ `if Vulkan` / `if D3D12` / `if Metal` 分岐を大量追加する
- [ ] OpenGL関数と1対1のRHI wrapperを作る
- [ ] Vulkan上でOpenGL immediate modeを再現する
- [ ] D3D12上でOpenGL state machineを再現する
- [ ] Metal上でOpenGL state machineを再現する
- [ ] Vulkan/D3D12/Metalだけ別Renderer.cppを複製する
- [ ] backend別にGameplay描画アルゴリズムを変える
- [ ] backend固有handleをMesh/Material/Entityへ埋め込む
- [ ] UIだけ隠しOpenGL contextで描画する
- [ ] Vulkan/D3D12/Metal失敗時に黙ってGLへfallbackする
- [ ] Android Vulkan失敗時に黙ってGLESへfallbackする（明示Vulkan選択時）
- [ ] Android Vulkan専用にRenderer.cppを複製する
- [ ] Vulkanモードでhidden EGL/GLES contextをUI用に作る
- [ ] CPU full-frame rendererを復活させる
- [ ] compileを通すためのdummy backendを作る
- [ ] Validation errorを無視する
- [ ] resource leakを既知問題として残したまま完成扱いする
- [ ] C#版へ変更を入れる
- [ ] force-pushする

---

# 33. 完成判定 Definition of Done

この作業は、単にVulkan/D3D12で三角形が表示された時点では完了ではない。

以下を全て満たして完了とする。

## Architecture

- [ ] Renderer FrontendがAPI非依存
- [ ] RHIがOpenGL APIの単純ラッパーではない
- [ ] OpenGL/OpenGLES/Vulkan/D3D12/Metalが同一RHI契約を使用
- [ ] WindowとSwapchainが分離
- [ ] Sceneがraw GPU API handleを所有しない
- [ ] Resource state/lifetimeが明示されている
- [ ] deferred destructionがある

## OpenGL

- [ ] RHI経由で動作
- [ ] immediate mode撤去
- [ ] display list撤去
- [ ] OpenGL既存描画と一致

## Vulkan

- [ ] game scene動作
- [ ] HUD動作
- [ ] cel/post process動作
- [ ] Launcher動作
- [ ] Pause動作
- [ ] Map Vote動作
- [ ] fullscreen/resize動作
- [ ] Validation重大エラーなし

## D3D12

- [ ] game scene動作
- [ ] HUD動作
- [ ] cel/post process動作
- [ ] Launcher動作
- [ ] Pause動作
- [ ] Map Vote動作
- [ ] fullscreen/resize動作
- [ ] Debug Layer重大エラーなし

## Metal

- [ ] game scene動作
- [ ] HUD動作
- [ ] cel/post process動作
- [ ] Launcher動作
- [ ] Pause動作
- [ ] Map Vote動作
- [ ] Retina/high-DPI描画一致
- [ ] fullscreen/resize動作
- [ ] Metal API Validation重大エラーなし
- [ ] hidden OpenGL contextなし

## Android Vulkan

- [ ] Android Vulkan起動
- [ ] game scene動作
- [ ] HUD動作
- [ ] cel/post process動作
- [ ] Map Vote/UI動作
- [ ] touch input座標一致
- [ ] orientation change動作
- [ ] pause/resume動作
- [ ] surface destroy/recreate動作
- [ ] Validation重大エラーなし
- [ ] hidden GLES contextなし
- [ ] GLES backendも継続動作

## Parity

- [ ] transparent描画一致
- [ ] stencil描画一致
- [ ] texture一致
- [ ] fog一致
- [ ] cel shading一致
- [ ] UI座標一致
- [ ] screenshot/readback動作
- [ ] scene unload/reload動作

## Stability

- [ ] 長時間実行でGPU resource増加なし
- [ ] backend切替を実装した場合、繰り返し切替でleak/crashなし
- [ ] Android既存buildを壊していない
- [ ] CI全対象green

---

# 34. 推奨コミット分割

巨大1commitへしない。

例:

```text
1. Introduce renderer backend selection
2. Separate window and presentation ownership
3. Add RHI core contracts
4. Add OpenGL RHI backend
5. Convert model display lists to vertex/index buffers
6. Move scene textures and render targets to RHI
7. Move pipeline state to RHI
8. Move shader bindings to backend-neutral layouts
9. Finish OpenGL RHI parity
10. Add Vulkan device and swapchain
11. Add Vulkan scene rendering
12. Add Vulkan UI composition
13. Add D3D12 device and swapchain
14. Add D3D12 scene rendering
15. Add D3D12 UI composition
16. Add Metal device and CAMetalLayer presentation
17. Add Metal scene rendering
18. Add Metal UI composition
19. Add Android Vulkan surface and lifecycle integration
20. Add Android Vulkan scene/UI rendering
21. Add Android GLES/Vulkan parity tests
22. Add backend switching
23. Add cross-backend parity tests and CI
```

各commitで可能な限りbuildableな状態を維持する。

---

# 35. Git運用

`develop2` を唯一の作業基準とする。

作業開始・各push直前に最新HEADを確認する。

並行コミットが入っていた場合、

```text
最新develop2取得
↓
自分の変更を最新HEADへ適用
↓
通常fast-forward push
```

とする。

**force-push禁止。**

他作業者の変更を巻き戻さない。

---

# 36. 実装担当への最終指示

この作業で最優先するものは、VulkanやD3D12のAPIコード量ではない。

優先順位は以下。

```text
1. 現行OpenGLの描画意味を把握
2. OpenGL依存をRendererから分離
3. Immediate Mode / Display ListをGPU buffer modelへ変換
4. RHIでOpenGLを完全再現
5. 同じRHIへDesktop Vulkanを実装
6. 同じRHIへD3D12を実装
7. 同じRHIへMetalを実装
8. 同じVulkan backendをAndroid surface/lifecycleへ接続
9. Android GLES/Vulkanを並存させる
10. Skia/UIを各backendへ正しく統合
11. parity / validation / lifetimeを閉じる
12. 必要ならbackend hot switching
13. その後に最適化
```

Vulkan/D3D12実装のためにOpenGL版と別の描画ロジックを作らない。

全backendが、

```text
同じScene
同じRenderItem
同じgeometry
同じmaterial
同じpass order
同じshader semantics
同じRHI contract
```

を通る状態を完成形とする。

最終目標:

```text
                    Fruity-Prime C++ Renderer

Game / Scene / HUD / Launcher
              │
              ▼
        Renderer Frontend
              │
              ▼
         Render Passes
              │
              ▼
             RHI
    ┌────────┬────────┬────────┬────────┬────────┐
    ▼        ▼        ▼        ▼        ▼
 OpenGL   OpenGLES  Vulkan   D3D12    Metal
 Desktop   Android  Desktop/ Windows   macOS
                    Android
    │        │        │        │        │
    └────────┴────────┴────────┴────────┴────────┘
                         │
                         ▼
                        GPU
```

**OpenGLは仕様ではない。**

**Renderer Frontendと描画結果が仕様であり、OpenGL/OpenGLES/Vulkan/D3D12/Metalはその実装である。**