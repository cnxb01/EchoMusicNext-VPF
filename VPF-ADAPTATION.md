# VPF 音效引擎适配（EchoMusicNext）

本仓基于 EchoMusicNext，附加了「把 EchoMusic 原版本播放器的 ViPER VPF 音效引擎（`EchoMusic-VPF-Provider`）
适配到 Next 播放器」的改动。上游引擎本身是合格的 DSP ABI v2 Provider，问题出在宿主的资源链路上。

## 原症状

- 导入 `echo_vpf_provider.dll` 后引擎可见、可激活，但没有任何听感变化。
- 音效广场中带 VPF 参数的条目始终显示「此音效需要支持 VPF 的音效引擎」。

## 三处断点

1. `AudioEffectCatalog::beginDownload()` 只要条目含 `vpf_urls` 就无条件拒绝，从不查询已安装引擎的能力。
2. `ImpulseResponseInstaller` 一次只能落一个文件，无法表达 IR + VPF 的原子双文件包。
3. `PlaybackAudioControls::applyEffect()` 从不写 `providerResources`，引擎 `create()` 收到空
   `resource_json`，只建出空处理链——所以「导入引擎也没效果」。

Rust 侧（`crates/echo-audio`）无需改动：`providerResources`、`ensure_provider_accepts_resources`、
`can_reconfigure_provider_in_place` 均已就绪。

## 改动

- `services/settings/SettingsKeys.def`：新增 Portable 前缀 `audio/downloadedEffectVpf/`，按音效
  identity 记录同包 VPF 文件。
- `services/playback/ImpulseResponseInstaller.{h,cpp}`：新增 `EffectResourceRequest`，`install()`
  支持多资源包（顺序下载、共用一把 `StorageFence` 写租约、整包进度、任一失败即整包失败）。
- `services/playback/DspProviderRegistry.h`：新增 `records()` 供能力枚举。
- `services/playback/PlaybackAudioControls.{h,cpp}`：
  - `supportsResource()` / `vpfEngineAvailable` / `identityNeedsEngine()` 引擎能力探测；
  - `registerEffectPackage()` 支持 IR+VPF 成对登记，纯 VPF 音效也能成为可选中条目；
  - `applyEffect()` 组装并下发 `providerResources`，`.vpf` 不再被当作脉冲响应；
    无引擎时自动选用声明支持 VPF 的引擎；
  - 停用/移除引擎、删除音效时同步释放 VPF 资源。
- `services/playback/AudioEffectCatalog.{h,cpp}`：双资源包下载；仅当确实没有支持 VPF 的引擎时
  才提示并拒绝；`decorate()` 输出 `engine_reason`。
- `modules/EchoMusic/Playback/PlaybackEffectCard.qml`：使用目录给出的 `engine_reason`。
- `modules/EchoMusic/Playback/PlaybackImpulseEffectPane.qml`：纯 VPF 条目引擎门控、缺引擎提示；
  「原声」按钮同时释放 VPF 音效。
- `.github/workflows/vpf-verify.yml`：本次新增的验证流水线（Linux 编译 + Windows 打包），
  不参与上游发布流程。

## 验证

`Actions → VPF verification build → Run workflow`：

- `Linux x64 compile`：快速验证 C++/QML 编译。
- `Windows x64 build and package`：产出 `EchoMusicNext-<version>-Windows-x64-Setup.exe` 工件。

运行态自检：装好引擎后，广场中的 VPF 条目应可下载并听出差异；未装引擎时仍提示需要引擎（正确行为）；
「原声」立即失效；删除音效后 `audio-effects/downloads` 下的 `.irs` 与 `.vpf` 成对消失。

## 已知限制

- 本地手动导入 `.vpf` 仍走音频探测流程（与合并前的上游一致），本次只打通市场下载的 VPF 音效。
- 原引擎用 `hound` 解码 IR，只接受 WAV；非 WAV 容器的市场 IR 会创建失败并回报错误。
