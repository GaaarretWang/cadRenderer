# Renderer Knowledge Base

这份文档只记录当前渲染管线里已经踩过、而且后续很容易再次踩中的硬约束。修改
`custom_pbr`、`OffscreenRenderTarget`、`SSAOPass`、`vsgRendererServer`、
`OcclusionCullingPasses` 之前，先看这里。

## 1. 当前主线仍是 hybrid deferred

当前大致流程是：

1. 第一段 depth cull
2. 第一条主 render graph 写主 GBuffer / shadow history
3. 手动 resolve depth / mask / normal / worldPos / material
4. depth pyramid + 第二段 cull
5. 第二条 render graph 补画当前帧新露出的物体，以及 transparent / wire / text / receiver
6. SSAO
7. deferred opaque
8. real scene
9. fallback composite
10. deferred composite

不要误判成“只有第一条 graph 在负责 opaque”。第二条 graph 仍然是 second-pass
occlusion culling 的消费端。

## 2. 两段剔除的硬约束

- 第一段 pass 只保证画出“上一帧已知可见”的那一批。
- 视角变化后新露出来的 opaque 物体，依赖第二段 depth-pyramid culling +
  第二条 graph 补画。
- 在没有新的消费链完全替代之前，不能直接从第二条 graph 去掉
  `MASK_PBR_FULL`。

错误案例：

- 提交 `ffb65ec` 一度把第二条 graph 的 `MASK_PBR_FULL` 去掉，结果
  second-pass opaque cull 没有消费端，当前视角新露出的物体会少画。

## 3. GBuffer / MRT 输出顺序

`custom_pbr.frag` 当前输出位置固定为：

- `location 0` -> `outColor`
- `location 1` -> `outNormal`
- `location 2` -> `outWorldPos`
- `location 3` -> `outShadow`
- `location 4` -> `outMaterial`
- `location 5` -> `outMask`

对应约束：

- `OffscreenRenderTarget` 里 render pass attachment 顺序必须和这里严格一致。
- 尤其单采样路径不能把 `material` 和 `mask` 接反。

错误案例：

- 提交 `27f9831` 新增 `mask` 后，单采样 render pass 一度把
  `material / mask` 顺序写反。
- 结果场景 0（`msaa = 1`）里所有物体材质颜色都发红，因为 `outMask = 1`
  被错误写进了 material attachment。

回归检查：

- 场景 0 是单采样路径的第一检查点。
- 每次改 `custom_pbr.frag`、`OffscreenRenderTarget.cpp`、`shadow.frag` 后，
  都要先看场景 0 的材质颜色是否正常。

## 4. mask 的语义

`mask` 是独立语义附件，不再让 `alpha` 兼任类型标记。

当前约定：

- `0` -> 非 opaque fallback / 空 / transparent / wire / text
- `1` -> virtual opaque
- `2` -> shadow receiver

对应规则：

- `custom_pbr.frag` 只给真正 opaque 的片元写 `mask = 1`
- alpha blend 透明片元不能再写成 opaque mask
- `shadow receiver` 写 `mask = 2`
- 后续 deferred / composite 优先读 `mask`，不要再把 `alpha < 0` 之类旧编码接回来

## 5. resolve 必须共用同一个 sample 选择

MSAA 路径里，手动 resolve 不是“每个附件各自随便 resolve”。

当前硬约束是：

- depth resolve 负责选出 `max depth` 对应的 sample index
- mask / normal / worldPos / material 都必须跟同一个 sample index

不要把某个附件单独改回平均 resolve，也不要让不同附件按不同 sample index 取值。

## 6. composite 的职责边界

当前最终合成还不是纯 deferred 终线，仍然保留 fallback composite。

职责应该逐步收敛成：

- `deferredOpaque` 负责普通 virtual opaque 的最终光照
- `realScene` 负责 camera image / real depth occlusion / real-scene shadow
- `fallback composite` 只负责：
  - shadow receiver
  - real scene fallback
  - transparent / wire / text 等非 deferred 内容
- `deferredComposite` 再把 deferred opaque 覆盖到 fallback 结果上

不要让 `fallback composite` 长期给普通 opaque 做主着色，否则旧链会一直留着。

## 7. real depth 与 occlusion 的当前约束

- `realScene` 和 `OcclusionCullingPasses` 现在都优先读手动 resolved depth
- 旧的 hardware depth resolve attachment 已经删掉
- 后续如果继续动 real-depth occlusion 或 depth pyramid，默认以
  `resolvedEffectDepthTarget` 为准，不要把旧 `depthImageView` 又接回来

## 8. 推荐回归点

每次涉及渲染语义改动，至少看这几项：

1. 场景 0 单采样材质颜色是否正常
2. `MSAA=4` 时普通 opaque 边缘是否正常
3. transparent 是否被误判成 opaque
4. receiver 是否仍然只走 receiver 语义
5. 相机移动时，新露出的物体是否会漏画
6. occlusion culling 是否突然少画 / 多画
