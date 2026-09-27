# C# 側の新規コミットを C++ へ移す作業表（2026-09-24 時点）

`origin/newest` の 212 コミットを develop2 に取り込んだ時点（[PR #1](https://github.com/Zection6V/Fruity-Prime/pull/1)）で、
`src/MphRead.Native/` は `src/MphRead/` より 302 ファイル分古い。
その差分を、下から（依存される側から）順に埋めるための一覧。

列の意味 — **S**: C# 側の変更種別 (A 追加 / M 変更 / D 削除)、
**+/-**: C# の追加・削除行数、**対**: 既にある C++ の対応物
（`-` は新規に書き起こすもの）。
**進捗**: `完了` / `一部完了` / `保留` / `未反映（作業中）`。`—` は、現在の `develop2` でこのバックログ差分の移植を確認できていない項目。共通ランタイム化など別目的の変更だけでは進捗扱いにしない。

作業の規則は `MphRead-Native-CSharp-to-Cpp-Basic-Policy.md` と
`MphRead-Native-CSharp-to-Cpp-Pitfalls.md` のとおり。1つのバッチを終える
たびにネイティブをビルドし、緑のままコミットする。

**番号は依存順ではない。** 実際の順序は次のとおり（着手して分かった分を反映）:

1 → 3（`Mods` 直下の葉）→ 6（入力）→ 7（描画）→ 8（チーム）→ 9（ネット）
→ 10（マップ生成）→ 11（ランチャー可搬部）→ 12（ランチャー GUI）
→ 4・5（更新・チャット）→ **2（Diagnostics）** → 13（エンジン）→ 14（Android）。

`Mods/Diagnostics` は葉に見えて、`WindowGeometry`・`Render::UiOverlay`・
`GuiLauncher`・`MapGen::CustomRooms` を呼ぶ**利用側**なので最後に近い。

## 進捗ログ

作業中に更新する。コミットは develop2。

- 済: 1 Platform helpers / 3 Mods leaves の大半 / 8 Multiplayer・teams
  （ea3398e9 まで）。
- 9 Network — 進行中:
  - 済 (f661cce0 ほか): NetProtocol（protocol 14）、NetSession・NetSessionLobby、
    SessionProtocol、LobbyRules、MatchDefinition、NetLifecycleTracker、
    NetPlayerLifecycle、ContinuousWeaponPhase、FormReconciliation、NetFaultQueue、
    NetMatchTimeSync、NetHealthSync、NetHudHealth、NetShotDiagnostics、
    NetTimingDiagnostics、NetSmoothing、NetHitClaims（C# と逐行照合済み）、
    NetHitPrediction、NetDamage、NetUnlagged、NetPlayerBridge、NetHooks、
    NetTransport＋NetLag、NetRoomChange、NetStatus、NetSlotManager、NetMatchSync、
    NetDiagnostics、NetFeatureCheck、DemoPlayback、MechanicsDump、MapRotation、
    MapAudit（＋Render/LockjawTrailProbe）、NetLog、ServerSim、ServerSimCheck、
    NetTestScript、HitRig、NetCheckClient、NetLaunch、
    Chat/NetChat、MapPick、PlayerEntityNetAim/NetHud の網関連分。
  - 付随: BeamProjectile / ItemSpawn / ItemInstance / PlayerEntity・Process・
    Collision・Draw の網関連差分、NativeRuntime に Guid・BinaryPrimitives・
    CharIsControl・StringSplit・StringReplaceOrdinalIgnoreCase・
    Console.KeyAvailable/ReadKeyInfo・EndPointEquals。
  - 済: DedicatedServer＋LobbyCommands（全面書き直し）、HostPool（新規）、
    NetMaster（HostCandidate・FindHosts・所有者トークン・CanHost フラグ）、
    NetHostSession、ModEntry の -server 部（-hostports・-affinityweapons）、
    HealthSimulationTest、NetHealthSyncTest、MapAuditTeams、SpireAltPoseCheck。
  - 残り: LocalServer、NetLobbyTest。
- 6 入力 前半: ゲームパッド層を実行時設定オブジェクト化（PadBindingState/GamepadOptionState/
  GamepadRuntimeConfig/GamepadManager/Profiles/Haptics/UiRouter ほか 29 ファイル）。NativeRuntime に
  Numerics(Vector2/3)・Event・ProcessExit・JsonWriteIndented・FileMove・EnvironmentTickCount64。
  GamepadProbe の `DesktopGlContext.PreserveWorkingDirectory` はSection 7の移植時に接続済み。PadRow は 12 まで暫定で新 API 呼び。
- 6 入力 後半: Stylus/Pointer/Pen/WeaponWheel/MouseFlick/AimAssist 一式と PlayerEntity 部分クラス
  （Haptics・MouseFlick・AimAssistWorld）、InputSettings、PlayerInput.cs 差分全部。保留だった
  TakeDamage の Telemetry/Feedback・着地フィードバック・ApplyGamepadAim も解消。
  Section 13で PlayerHud の `UpdateWeaponSelect`（WheelHeld/Absolute/Drag）と
  ModEntry の `-gamepadassisttelemetry` をC#と照合・配線済み。
- 6 完了: 検査系 6 本（PointerCheck は Scene/PlayerEntity に friend、GamepadUiChecks 呼び出しは 12 で）。
  NativeRuntime に DirectoryDelete・DirectoryGetFiles。
- 7 描画 着手: HunterPreview・ProHud・VoteHud・GlEs・Radar・TeamScoreboard・StylusHud と、それらが載る
  PlayerHud.cs 差分（13 の行）を移植。Scene に DrawFlat{Disc,Ring,Line,Square,Polygon}（Renderer.cs 分）。
- 7: MapThumbnail・PlayerEntityMapPick・EndScreen 部分クラス、EndScreen.cs（PanelUp・Tick・結果画面の
  パッド操作）。Scene::DrawHudTexture。呼び出し側（EndScreen::Tick・MapThumbnail::BeginFrame）は 13 の Renderer。
- 7: PreviewPass（ランチャー用プレビュー）・FrameTimingCheck・NoiseField・HunterShot。
  残り 6 本（LauncherPhoto・UiOverlay・LauncherHunter・LauncherNoise・AppIcon・DesktopGlContext）も
  12 と並行して `.cpp/.hpp` 化し、各 C# 原本との静的監査を完了。AppIcon は Renderer の生成経路、
  DesktopGlContext は RenderWindow・ThumbnailCapture・GamepadProbe から接続済み。
  LauncherPhoto・UiOverlay・LauncherHunter の描画呼び出しはSection 12のShell/Rendererへ接続済み。
- 10 マップ生成 前半: CollisionObj（新規）・MapDefinition（Collision・KeepItems・camelCase 出力）・
  MapPacker（ApplyCollision・面属性）・BuiltFace 属性・MapBundle・CustomRooms。
  NativeRuntime に JsonNamingPolicyCamelCase。
- 10: Q3Import（三角形法線・Clip クランプ・Weld 許容誤差・Pickups）・Q3Convert（-noitems・AddItems）・
  MapReport.ListItems。MapCheck（-mapcheck）・AltFormProbe（-altprobe）・MapReport.ListItems（-mapitems）は
  ModEntryから配線・監査済み。セクション10 完了。
- 11 Launcher portable: LaunchPlan（LobbyContext）、GameFiles（Root=AppPaths、RomWhitelist 照合）、
  TextLauncher（InputEnded・insane・StartupForced）、NativeFilePicker（NativeRuntime に
  ProcessRunCaptureOutput）完了。MatchStart は RenderWindow の1ウィンドウ API を使う形へ移植し、
  C#原本との静的監査とWindows Release buildを完了。
- 12 完了。C# は Avalonia headless + Skia CPU ラスタ → GL 転送（UiTopLevel/UiSurface/UiOverlay）。
  旧 NativeRuntime/Gui（Element ツリー + GL 直描画、グラデーション・楕円・パス・影なし）では足りないので、
  C# と同じ形で NativeRuntime に再現する:
  (A) NativeRuntime/Skia: CPU RGBA premul キャンバス（AA パス塗り、ストローク、線形/放射グラデーション、
      角丸、楕円、BoxShadow ぼかし、クリップ、変換、不透明度レイヤ、画像、FreeType 文字）。
  (B) NativeRuntime/Avalonia: Control/Panel/Grid/StackPanel/DockPanel/Border/Decorator/UserControl/
      TextBlock/ScrollViewer/Image 等のレイアウト・入力ルーティング・フォーカス・DrawingContext・
      Dispatcher/DispatcherTimer・TopLevel（UiTopLevelImpl 相当）。
  (C) Mods/Launcher/Gui の 50 新規ファイルを (B) の上に一対一移植、(D) 削除 9 ファイルと旧ホストを撤去。
  Tap・TapCheck 完了（GuiTheme に GuiSize）。
  (A) NativeRuntime/Skia 完了（Skia.hpp/.cpp・SkiaText.cpp、JPEG は libjpeg、Android は除外）。
  (B) NativeRuntime/Avalonia 新ツールキット完了: Base（幾何・AvaloniaProperty/StyledProperty/AvaloniaObject）、
      Media（ブラシ・ペン・FontFamily/Typeface・TextLayout/FormattedText・Geometry・Transform・BoxShadows・
      Bitmap・DrawingContext）、Platform（AssetLoader）、Input（Key 値は Avalonia と同一）、Controls
      （Visual/Layoutable/Interactive/InputElement/Control/TemplatedControl、ルーティング・フォーカス・キャプチャ）、
      Panels（Panel/StackPanel/Grid/DockPanel/Canvas/Decorator/Border/ContentControl/UserControl/
      LayoutTransformControl/Image）、Text（TextBlock/TextBox）、Scroll（ScrollViewer、Fluent のオーバーレイ
      スクロールバー込み）、Threading（Dispatcher/DispatcherTimer）、TopLevel（EmbeddableControlRoot・
      RenderTargetBitmap・ヒットテスト・ポインタオーバー）。単体描画テストで確認済み。
- Section 12 完了: NetLaunch::TickTerminalLobby を Renderer の全ビルド共通フレーム入口から呼び、
  HasScene/EndScene、persistent lobby の state reset、MatchStart::Begin、通信失敗時の終了を
  C# 原本の順序で接続。C# 原本・直接呼出し元との静的監査とWindows Release buildを完了。

### 2026-09-26 進捗監査

`develop2` のコミット済み状態を、バックログ作成コミット（`75f30297`）以降の
対象ファイル履歴と C# 側差分に突き合わせて再確認した。単なる
`NativeRuntime` 共通化・文字列処理・数値処理などの横断リファクタは、
対象バックログ差分そのものを移植していない限り進捗には数えない。

- 追加で **完了** を確認: `Mods/Render/LockjawTrailNoise.cs`、
  `Utility/Console.cs`、`Mods/DebugLog.cs`、`Entities/NodeDefenseEntity.cs`。
- **一部完了 → 完了** に訂正: `Entities/BombEntity.cs`、
  `Mods/Launcher/Portable/LauncherPrefs.cs`。
- `DedicatedServer.cs` はバックログ作成後に専用のネットワーク移植コミットがなく、
  変更履歴は共通ランタイム化のみ。`LobbyCommands.cs` と `HostPool.cs` は
  対応する C++ ファイル自体がまだ存在しない。この3件は、作業中という記録は残しつつ
  表では **未反映（作業中）** とする。
- 追記（同日）: 上の3件は 3f7e8047 で移植済み。NetMaster・NetHostSession・
  健全性テスト4件も完了。BeamProjectile・ItemSpawn・ItemInstance・
  PlayerProcess・PlayerCollision・PlayerDraw は C# 差分の全ハンクを反映済みのため完了。

### 2026-09-26 Section 12 継続作業

- GUI の表にある70項目中、**56項目を C++ 反映・C# 原本監査済み**として更新した。
  `Tap`、`TapCheck`、Deck 系、入力行、一覧・ナビゲーション、背景・テーマ、地理・地図表示など。
- 表外の既存依存ファイルでは `TrackedText`、`ProgressRow`、`CrosshairPreview` も反映・監査済み。
- `ServerBadge.cs` と `ServerRow.cs` を移植し、各 C# 原本と照合済み。
  `ServerRow` の名前末尾判定は C# の UTF-16 長、描画丸めは .NET の ties-to-even に合わせた。
  バッジが使う `System.Net.IPAddress.TryParse` / `IsLoopback` 相当を `NativeRuntime/System/Net`
  に集約し、IPv4 の旧式表記、IPv6 のスコープ、IPv4-mapped loopback を反映した。
- `GamepadGlyph.cs` は形状描画、PlayStation の記号、ファミリー選択、TrackedText のサイズと配置を
  C# と照合済み。
- `LobbyPlayerRow.cs` は roster 配列の読み順、null 名の連結、チーム・READY 表示、列割当、文字スタイルを
  C# と照合済み。配列アクセスは managed runtime の null / 範囲例外経路を使用する。
- `ConfirmScreen.cs` は本文と yes/no mark の構成、初回フォーカス、Escape の false 応答を照合済み。
- `GamepadMonitor.cs` は attach/detach timer、状態差分更新、各スティック・トリガー・文字列の座標と書式を照合済み。
  `UiSurface` の dirty 通知はその未移植ヘッダーと接続するため、Section 12 の統合作業時に再確認する。
- `CreateServerScreen.cs` と内包する `PickRow`・`HostPicker`・`MapRotationPicker` を移植・監査済み。
  12ゲームモードとハンター選択、ホスト探索・再問い合わせ、マップの選択順と16件上限、
  Hosted/Dedicated の起動・接続、失敗時のセッション停止範囲、設定保存と `LaunchPlan` の生成順を照合した。
  `StartScreen` からの接続は完了。`GuiLauncher`/`UiCapture` 統合は後続。
- `PlayScreen.cs` の `.cpp/.hpp` を移植し、全メソッドを C# 原本と照合した。
  PR #1 の修正例も確認し、サーバー行の一押しを選択だけにする動作、応答数と稼働数の分離、
  ディレクトリ進捗行の保持、選択した行からの明示的な JOIN、デスクトップの `NativeFilePicker` 分岐を反映。
  C# の `ToolTip.SetTip` 相当として NativeRuntime に非操作 Popup tooltip を追加した。
  `StartScreen` からの接続は完了。`GuiLauncher`/`UiCapture` 配線は後続。Section 12 の作業中バッチのためビルドはしていない。
- `UiDesigns.cs` を `.cpp/.hpp` へ移植し、6案×4画面の生成順、1280×720 の capture、出力パス・ログ・終了コード、
  12マップ・5サーバー・設定値・各案の配置を C# 原本と PR #1 の追加差分に照合した。
  監査中に D案の一覧スクロールと A案の一時停止列配置の差を直した。
  `UiDesignsAdapter` の `GuiLauncher`/`UiCapture` 実装接続は後続の配線作業。ビルドはしていない。
- `StartScreen.cs` を `.cpp/.hpp` へ移植し、メイン画面のレイアウトと幅別切替、ground の着脱、画面 stack、
  Play/Create/Settings/Setup/Confirm の遷移、persistent lobby の復帰・一時停止、pause 操作と map vote、
  preview の追いつき、version/update の状態表示・進捗・installer 完了経路を C# 原本と PR #1 の差分に照合した。
  更新確認は `shared_from_this()` が有効になる `Create` factory の直後に開始する。
  `GuiLauncher`/`UiCapture` からの接続は後続。Section 12 の作業中バッチのためビルドしていない。
- `LobbyScreen.cs` と内包 `CustomTeamPicker` を `.cpp/.hpp` へ移植し、PR #1 の追加差分と C# 原本に照合した。
  roster/session revision によるプレイヤー再構築、1秒ごとの ping 更新、最新12件のチャット表示、
  owner/team 操作、match 定義検証と数値の invariant parsing、250ms 後の自動反映、map/custom-team picker、
  thumbnail の生成・解放経路を照合した。ネイティブ timer は attach 時に接続して start し、
  close callback 中も timer dispatch が戻るまで画面を保持する。`GuiLauncher`/`UiCapture` 配線と統合ビルドは後続。
- `InGameMenu.cs` を `.cpp/.hpp` へ移植し、PR #1 の追加差分と C# 原本に照合した。
  pause/settings/map-vote の stack 操作、spectate/rejoin/record/leave/quit の各 callback、投票時の二段 pop、
  Escape の handled 条件を確認した。イベント中に menu が UiSurface から外れても処理が戻るまで親を保持する。
- `EndPanelView.cs` を `.cpp/.hpp` へ移植し、PR #1 の追加差分と C# 原本に照合した。
  ballot order の snapshot、投票数/leader の差分描画、hunter/suit の commit と再読込、ready 状態、
  hunter stand の ballot 面での非表示、右側 panel の構成を確認した。
- `Shell.cs` を `.cpp/.hpp` へ移植した。ローカル C# 原本は PR #1 掲載ファイルと同一 blob。
  match/lobby/menu の遷移、例外報告、pause menu、入力変換、window capture script を監査した。
  監査で shellshot の `shell-server-side` 撮影漏れを見つけて修正し、24枚の撮影名・39個の待機値・
  38個の script action の順序が C# と一致することを再確認した。RenderWindow/MatchStart/ScreenCapture と
  Diagnostics の呼び出し先は各依存セクションの移植時に接続する。Section 12 の作業中バッチのためビルドしていない。
- `UiCapture.cs` を新しい `UiTopLevelImpl`/NativeRuntime の画面層へ移植し、26画面の名前と順序、
  画面サイズ、`Deck.Phone`/`Deck.Still`/`PlayScreen.Sample` の更新位置、fleet/browser の全サンプル値、
  PNGと同名JSONのVisual bounds出力、失敗ログ・終了コードを C# 原本と PR #1 差分に照合した。
  監査では旧3行サンプルとサーバーヘッダー、旧画面名、旧 Avalonia adapter を除去した。
  `ModEntry` の実行入口と `UiDesigns` の共通 capture 接続は後続の統合作業。ビルドはしていない。
- `GamepadUiChecks.cs` を `UiTopLevelImpl` 上の同一 UI 操作・入力・設定画面検査へ移植した。
  C# の35個の assertion 名と順序、コントローラー action 数、キャプチャ競合/取消/切断、preset・keyboard rebind、
  pause/map vote と任意 PNG 3枚を照合した。ネイティブの `GamepadSettingsPanel::Reload` で旧 Visual が破棄されるため、
  再読込後に Advanced/monitor と対象 PadRow を現行ツリーから取り直す。`GamepadChecks::Run` の
  `CheckPersistence` 直後に shell build のみで呼ぶ接続を追加した。
  C# の assertion 一覧は35/35が一致。ビルド・実行は未実施。
- `GuiLauncher.cs` を新しい `Shell::Run` と NativeRuntime の process-wide setup に移した。
  一度だけの setup、display probe、Android 分岐、fallback 文言、Linux の fontconfig 案内を照合し、旧 AppBuilder/
  HomeWindow adapter・別窓ループを除去した。PauseMenu から `EnsureSetup` を直接呼ぶようにし、旧 helper と Dispatcher pump を削除した。
  C#例外時の `PlatformDiagnostics.Report` は Section 2 の移植後に二経路とも接続した。
  `PlatformDiagnostics.Start` とsmoketest/GLFW/window diagnostic dispatchはSection 2で接続し、Windows Release build済。
- `UiBench.cs` を `.cpp/.hpp` へ移植し、5解像度・6シナリオ、10回 warm-up / 61回計測、中央値、surface寸法と倍率、
  スクロール・pointer・wheel操作、PNG出力をC#原本と照合した。NativeRuntime には旧Avalonia `Window` と managed GC がないため、
  Slow計測は全画面copyを行うheadless rigとして明示し、GC欄は `n/a` とした。layout数はNativeRuntimeが通知するTopLevelの
  layout passを数える。`LauncherPhoto` はSection 7から延期された依存だがSection 12で接続済み。Windows Release build済、実画面計測は未実施。
- `LauncherNoise.cs` を `.cpp/.hpp` へ移植し、既存 `NoiseField` による形状変更検出・33ms upload cadence、固定texture名、
  RGB upload と全unpack state、nearest/clamp sampler、失敗時fallbackと context-current `Release` をC#と照合した。
  desktop GL wrapper に必要な `PixelStoreParameter` 値を追加した。再監査で、C#のstatic field初期化はLauncherNoise初回アクセス時だが、
  C++ namespace staticはプロセス起動時に時計を始める差を検出。初回アクセスでNoiseField→upload clockの順にlazy初期化するStateへ修正した。
  `Texture` getterと`Release`もC#の型初期化を起こす順に揃えた。ビルド・実行は未実施。
- `Shaders.cs` のSection 7依存部分を先行移植し、`BackdropVertexShader` / `BackdropFragmentShader` の本文を
  C#と改行正規化後の全204/429文字で完全一致照合した。残りのShaders差分はこの2プロパティのみ。ビルドは未実施。
- `LauncherPhoto.cs` を `.cpp/.hpp` へ移植し、GL shader compile/link・固定texture名・RGBA JPEG upload、全unpack状態、
  texture sampler、UniformToFillの中央crop、noise shader時の2 texture座標と固定機能状態復元、失敗時の静止画fallbackを
  C#原本と照合した。NativeRuntimeの既存AssetLoader/JPEG decoderを使い、GLに足りなかったprogram query/log/delete、
  multitexture座標、matrix stack、texture environment、base/max level APIを追加した。RendererからのDraw接続はSection 12
  統合作業に残る。native実行ファイルの出力先でAssetLoaderが写真を見つけられるよう、CMakeのpost-build資産コピーにも
  `launcher-bg.jpg` を追加した。ビルド・実行は未実施。
- `LauncherHunter.cs` を `.cpp/.hpp` へ移植し、wanted/drawn と hunter/suit/矩形の状態、side scene の process lifetime、
  preview scene の初期化、Scene preview state 設定、実際に描けた場合だけ穴を開ける判定、失敗のsticky停止・ログを
  C#原本と照合した。`RenderWindow.HasScene`/`NewSideScene` はSection 13側で接続済み。Windows Release build済、画面runtimeは未実施。
- `UiOverlay.cs` を `.cpp/.hpp` へ移植し、固定texture名、同寸法でのTexSubImage2D、変更時のみTexImage2D、RGBA premul blend、
  unit-1無効化、window viewport / identity matrix、clear→photo→overlay→hunter の単独画面描画順、Release後の状態を
  C#原本と照合した。OpenTK薄いAPIに `BlendingFactor::One` と `TexCoord2` を加え、Renderer/ModEntryからSection 12で接続した。
  `LauncherHunter` の `HasScene`/`NewSideScene` もSection 13で解決済み。Windows Release build済、画面runtimeは未実施。
- `AppIcon.cs` を `.cpp/.hpp` へ移植し、埋め込みPNGの取得に対応するnative配布asset、RGBA decode、16/32/48/originalの順、
  alpha-weighted box filter、once-only cache、失敗時ログをC#原本と照合した。GLFW `Window::SetIcon` を追加しRendererから接続。
  Renderer呼び出し順も再監査し、C#と同じく未対応機能callbackを設定してからwindowサイズを問い合わせる順へ修正した。
  ビルド・実行は未実施。
- `ModEntry.cs` のSection 12入口を静的監査し、`-uishot`/`-uibench`/`-uidesign`/`-shellshot`/`-frametimingcheck`/
  `-tapcheck` をゲームファイル確認より前に配線した。`-uibenchscale` はC#の `NumberStyles.Float` に合わせて
  thousands separator を許さず解析する。`-uinativeres` と fullscreen/windowed の起動前適用も移した。
  `UiCapture::Run` の旧adapter呼び出しを現行公開APIに合わせ、CMake desktop target にC#相当の `MPHREAD_SHELL` を定義した。
  C#の `RunUi*` no-inline helper、例外文、返却値、コマンド順を確認した。ビルド・実行は未実施。
- 同じ `ModEntry.cs` の早期入口を追加照合し、`-pointercheck`、AimAssist debug/telemetry の設定、
  `-gamepadcheck`、`-gamepad`（`verbose` を含む）、`-frametimingcheck` を `TryHandleHeadless` に接続した。
  `GamepadChecks.cs` は C# と同じ `CheckPersistence` 直後に、desktop shell で `GamepadUiChecks::Run(shots)` を呼ぶ。
  ビルド・実行は未実施。
- `DesktopGlContext.cs` を `.cpp/.hpp` へ移し、GLFW error callback、macOSのworking-directory/menu-bar init hint、
  macOS 2.1 Any profile / 他desktop 3.2 Compatability settingsをC#と照合した。callback登録はGLFW初期化を早めない薄いadapterにし、
  `RenderWindow::Settings`、`ThumbnailCapture`、`GamepadProbe` の各C#呼び出しへ接続した。Android compileではGLFW APIを参照しない。
  ビルド・実行は未実施。
- C# 側で削除された旧 GUI 9 ファイル（`DemoPickerView`、`HomeView`、`HomeWindow`、`MapPickerView`、
  `MenuEntry`、`PauseMenuWindow`、`SettingsWindow`、`SplashView`、`UpdateBadge`）を1つずつ原本パスで確認した。
  すべて C# 原本がなく、Native 側の対応物も削除済みで、残存ソース参照もない。
- `Mods/PauseMenu.cs` をC#原本に合わせて再配線した。旧ウィンドウ矩形、追従、別窓pumpを削除し、
  `GuiLauncher::EnsureSetup` と `Shell::OpenPauseMenu`/`CloseMenu`/`Quit`/`LeaveMatch` を接続した。
  refocus、fullscreen toggle、topmost同期、leave/quit flag の順序を照合した。
  per-frame `Shell::TickUi` とシェル画面の overlay 描画は `Renderer.cs` と照合して接続した。
  `MatchStart.Begin` は既存窓へのシーン構築へ移行済み。ビルドはしていない。
- `HunterStand.cs` は7ハンターの色・84個の箱と順序、Suit再着色、描画投影・面順・ライト、33msタイマー、
  回転ドラッグ、エンジン描画との切替、スクリーンショットの寸法丸め・BGRA→RGBA変換・失効判定を照合済み。
  箱データ84件は原本から抽出した値を全件比較した。奥行きソートは C# `List.Sort` の比較順と
  `ManagedSort` を使い、同値時の並びも .NET と一致させた。`MPHREAD_SHELL` 分岐の描画接続と
  `NewSideScene` を追加し、`Scene.SideScene` の初期化抑止をC#と照合した。
- `Renderer.cs` のシェル依存部を静的監査し、`RenderWindow(bool shell)`、`HasScene`、
  `NewSideScene`/`BeginScene`/`LoadScene`/`EndScene`、シェル時の画面寿命・入力経路・
  フレーム描画合成・ウィンドウ位置保存をC++へ接続した。GLFW adapter はキー解放と横ホイールも渡す。
  C# の `SideScene` が抑止するプレイヤー初期化と端末プロンプト、描画設定ログも照合して反映した。
  続けて `MatchStart.cs` と `NetLaunch.cs` の保留差分を移植し、各C#原本と直接呼出し元を静的監査した。
  `NetLaunch.cs::TickTerminalLobby` は persistent lobby の scene 終了→match state reset→terminal message、
  scene の有無による早期 return、pump/input、timeout/refusal/leave 時の close、`ShouldLoadMatch` 後の定義取得と
  `MatchStart::Begin` の例外処理を照合した。`Renderer::OnRenderFrame` では frame-rate 設定直後に呼び、
  C#と同じ clear/swap/base-frame/return の順にした。`git diff --check` は通過。
  `Renderer.cs` の Windows pen / pointer、AimAssist debug、gamepad/focus、frame/draw更新順はnativeへ接続し、
  監査で見つかった保存・resize・maximize差分も修正済み。Section 12 完了後のWindows Release buildで確認した。
- Section 12 は全画面の差し替えと `ModEntry`/`PauseMenu` の再配線まで完了。後続のSection 13修正を含む
  Windows Release `ninja -k 0` も成功。画面runtimeは未実施。
- `GamepadSetupPanel.cs` は入力行の順序、Android で隠す項目、timer の開始・停止、mapping の neutral 待ち、
  20段階の wizard 呼び出し、校正の計測時間、取消・適用・例外表示を原本と照合済み。
  `DispatcherTimer` が native では callback 付き構築時に自動開始するため、C# と同じく未開始で生成し、
  `Start` 時だけ動くようにした。
- `GamepadProfilePanel.cs` は初期化と profile 名一覧、選択・入力欄、6操作の順序、操作ごとの変更通知、
  成功・例外ステータスを C# と照合済み。native の `InvalidDataException` は `IOException` と別型なので、
  両方を個別に捕捉する。
- `GamepadSettingsPanel.cs` は timer の可視性・revision 条件、フォーカス復元、端末選択、詳細設定の並びと値域、
  controller family / curve の表示名、変更後の再読込を照合済み。初期 slider 値は C# の double `Math.Round`
  と同じ計算精度・ties-to-even を使う。
- `ControllerKeyboard.cs` はキー配列・文字 ID・初期フォーカス、Shift / Delete / Done / Cancel、`MaxLength` と
  1024 UTF-16 code unit 上限、Popup の親・対象・閉じる順序、対象切断時の取消、確定後の focus と callback を照合済み。
  native 側は TextBox と最寄り Panel の参照を保持し、確定値を Popup を閉じる前に反映する。
  `Media::ToUtf32` / `ToUtf8` は UI 内 managed string の WTF-8 を往復させ、補助キーの Delete が補助文字の
  UTF-16 code unit 1つを消した場合も lone surrogate を保持する。
- `GamepadNavigation.cs` は root 変更時の router reset、PadRow への同一 snapshot 注入、KeyRow の controller press、
  Changed の発火位置、Accept / Back / tab / page scroll / 矢印キーの分岐順を照合済み。native 側では C# の
  管理参照寿命に合わせ、イベント中の root・focused control・tab・ScrollViewer を保持する。
- `UiTopLevel.cs` は Avalonia 実装の pixel buffer・Drawn/Painted・client size・mouse/touch/key/text input を
  `NativeRuntime::Avalonia::EmbeddableControlRoot` へ接続する薄い wrapper として移植・照合済み。
  `UiRenderTimer` はゲームフレームから native TopLevel を明示 pump し、TouchBegin 前にも同じ pump を行う。
  KeyEventArgs が `PhysicalKey` を保持するよう native Avalonia 入力型も補った。
- `UiSurface.cs` は生成・表示・非表示・resize/raster cap・scale bake・pending frame の実行順・dirty redraw の
  50/250/3000/16/66 ms 条件・計測ログ・pointer/button/wheel/key/text 入力・ClickOn/HoverOn の座標変換を
  C# と照合済み。`FrameClock` は C# の型初期化と同じく最初の static API 呼び出しで開始する。
  `UiOverlay` 呼び出しは、対応する Render ファイルを移植する Section 12 内で解決する。
- `UiScaleHost.cs` は decorator/transform の構成、screen の get/set、measure 前の factor 更新、DIP→point の
  `160/96` 換算、BakeScale に掛ける RenderScaling、factor 不変時の早期 return と変更ログを照合済み。
  `FactorFor` は C# の Infinity・非正値条件を保持し、`NaN` も .NET の算術結果どおり伝播させる。
- `PauseMenuView.cs` は menu の項目条件と順番、200ms の vote timer、Accept/Deny の即時応答、表示中の項目数に
  基づく NeededHeight、0.5〜1.0 の縮尺、Escape と初回 Resume focus を C# と照合済み。
  `UiCapture.cs` の移植で新しい capture 経路から直接生成するよう更新済み。`ModEntry` の入口配線は後続。
- `MapCardPicker.cs` は Factory の room code/metadata、OrdinalIgnoreCase の初期選択、空 room の診断、候補選択と
  use/back/Escape、選択名の表示を照合済み。native Avalonia は親を子より先に attach するため、初回 focus は
  grid の子が attach された後に dispatcher へ送る。`LobbyScreen` / `PlayScreen` からの呼び出しは接続済み。
- `SettingsView.cs` は旧 adapter を外し、現行 C# と同じ5ページの `UserControl` に置き換えた。Display/Audio/Controls/
  Profile/Credits の構成、FOV のライブ変更と Cancel 復元、保存時の設定適用、controller reset、touch/stylus の条件、
  debug log の共有・endpoint 検証を照合済み。`HunterStand` の移植と `UiCapture`・各画面の呼び出し接続は後続。
  Section 12 の作業中バッチなので、ビルドは行っていない。
- `SetupScreen.cs` は初回フォーカス、Escape/back 条件、ROM 選択、抽出中のログと進捗、成功後のプレビュー生成、再描画操作、
  末尾8行保持を C# と照合済み。native headless desktop の file picker を既存 `NativeFilePicker` に対応させた。
  `StartScreen` からの接続は完了。`GuiLauncher`/`UiCapture` 配線は後続。Section 12 の作業中バッチなので、ビルドは行っていない。
- `ThumbnailHost.cs` の native 対応を、削除される Avalonia Host adapter 経由から直接の C++ `shared_future` API に更新した。
  空一覧・登録済み host・batch 不可・`Task.Run` 相当の分岐、結果と例外を照合済み。`SetupScreen.cs` と `StartScreen.cs`
  が使う前提として追加した。Section 12 の作業中バッチなので、ビルドは行っていない。
- 全体の残り: Section 12 は一括交換が未完了で、作業ツリーはビルド不可。現状の未コミット差分を保持し、
  残りのGUIファイルをC#ごとに照合して完了させる。Androidは全Androidファイルの完了後にビルドする。
  このスナップショットは統合前の記録。Section 12は2026-09-27に完了し、その後Section 2・13・14と延期呼び出し元も完了した。

### 2026-09-27 再開後監査

- `UpdateCheck.cs` と `.cpp/.hpp`、直接呼び出し元を一ファイル単位で全体照合。HTTP設定・status/error分類・JSON/asset選択・RID/名前・version比較・`LastReason` の更新順を確認し、`CancellationToken` がnative HTTPで捨てられていた差を修正。`std::stop_token` を libcurl progress callback へ渡し、事前キャンセルと転送中断を `TaskCanceledException` 相当へ対応させた。Section 12一括交換が未完了のためbuild・実行確認は保留。
- `Updater.cs` と `.cpp/.hpp` を全体照合。Disabled時・例外時を含む`Available`/`Checked`更新、background `found`/`done` と例外境界、50ms wait・DateTime範囲、HTTPS限定とplatform/browser起動、Describe出力を確認した。`ModEntry -update`、StartScreen、TextLauncher、SettingsView、ServerUpdateの直接呼び出しも照合し、追加修正は不要。Section 12一括交換が未完了のためbuild・実行確認は保留。
- `DesktopUpdate.cs` と `.cpp/.hpp`、UpdateInstall/ServerUpdate/ModEntryの直接接続を照合。stage・展開・launch/apply引数・30秒終了待ち・400ms猶予・上書きcopy/retry・clean・実行属性を確認した。C#の失敗ログは例外型も含むためnativeのstage/launchログを `ExceptionToString` に合わせた。ModEntryのparse失敗値 `-1` をPOSIX `kill(-1, 0)` に渡していたため、非正PIDは無効PIDとして400ms後に続ける。download tokenはUpdateDownload監査で送信中・body読込中のC#相当pollを追加。Section 12一括交換が未完了のためbuild・実行確認は保留。
- `BuildVersion.cs` と `.cpp/.hpp`、直接参照元を照合。entry/fallback stamp選択、Lazyの一度だけ評価と例外保持、`+sha`切除、空白・v接頭辞・prerelease・2〜4成分・Int32境界、normalise/比較/表示を確認した。local buildはC# projectとnative compile-adapter双方で未stampをlocal扱いし、コード差はなかった。C++ release stampはcompile adapter defineで渡す設計だが、現行release workflowはC++ targetを作らないため未実ビルド。
- `NetChat.cs` と `.cpp/.hpp` を照合。64件の履歴更新、system/team行、Receive/Remember順、空白送信抑止と `NetSession`/`NetSessionLobby`/`LobbyScreen` の直接接続は一致。C#の `Revision++` はunchecked wrapなのでnativeの符号付きoverflowを `UncheckedAdd` に変更した。build・実行確認はSection 12統合後。
- `ThumbnailBatch.cs` と `.cpp/.hpp` を一ファイル単位で照合し、Macの並列数、既存キャッシュの再確認、batch/worker失敗状態、5分timeout、非0終了のリトライ抑止、worker終了処理、stdout/stderr排出、64行・2048 UTF-16 code unit制限、成功/失敗報告を反映した。直接呼び出し元は既定timeoutを使うためAPI変更の追加配線は不要。現在の Section 12 一括交換が未完了という引継ぎ指示に従い、build・実行確認は保留。
- `ScreenCapture.cs` と `.cpp/.hpp` を全体比較し、window back-buffer capture、GLバージョンによるcontext flags/profile取得、KHR_debug拡張とprocの可用性確認、Androidでの診断停止、再有効化時のmessage数reset、GL callback例外の遮断を反映した。`NativeRuntime/OpenTK/GLFW` に対応する2つの薄いAPIを追加。直接呼び出し元との引数・capture時点も確認し、`git diff --check` は通過。Section 12一括交換が未完了のためbuild・実行確認は保留。
- `ThumbnailCapture.cs` と `.cpp/.hpp` を一行ずつ照合し、診断開始/scene読込ログと実際の `ClientSize` を反映した。独自の手動ループを `RendererPlatform::Window::Run(WindowEvents&)` に置き換え、GLFW event polling、frame callback、close時のcleanupがC# `GameWindow.Run()` と同じライフサイクルを通るよう修正。`ModEntry`/`ThumbnailBatch` の呼び出し引数と公開 `WindowSettings` の診断呼び出しも照合。現在のSection 12作業中バッチではbuild・実行確認を保留。
- `ThumbnailLog.cs` と `.cpp/.hpp` を照合し、開始時に欠けていた informational source version と executable path を追加。並列workerの追記はC# `File.AppendAllText` 相当の共有モード・失敗分類を持つファイル内 `WriteBytes` に接続し、既存の5回/20ms再試行、パス再評価、UTF-8 no-BOM出力と例外時の無効化を確認した。build・実行確認はSection 12統合後。
- `ChatBox.cs` と `.cpp/.hpp` を全体比較。受信fallback・system/team表現・64件上限・10秒表示/1秒fade・送信・80文字ASCII入力・開閉とキー処理を確認した。Rendererのdesktop入力、Android `GameView` のkey/Unicode入力、NetChat/NetSession/NetCheckClientと直接呼び出しも照合し、挙動差と追加修正は不要。typed inputがASCII限定のためnativeのASCII space trim・byte countはC# `Trim`/UTF-16 countと同じ結果になる。Section 12統合前なのでbuild・実行確認は保留。
- `UpdateDownload.cs` と `.cpp/.hpp` を全体照合。URL許可、HTTP status/headers、redirect/cookie、10分のheader timeout、content length・progress、部分ファイル作成/cleanup・置換順を確認。C#の`Send(..., cancel)`と各read後の`ThrowIfCancellationRequested()`がnativeで無視されていたため、header転送中のlibcurl中断とbody chunk読込後の書込み前pollを実装。`DesktopUpdate.Stage`と`LocalServer.Install`の同期呼び出しも照合した。build・実通信確認はSection 12統合後。
- `ServerBadge.cs` と `.cpp/.hpp` を原本から照合し、`ServerRow`からの描画引数、IPv4/IPv6のendpoint切出し、loopback/LAN/Internet/Silent分類、country fallbackとFlags描画、色・pixel座標を確認。今回追加されたNativeRuntimeの `IPAddressTryParse`/`IPAddressIsLoopback`/`IPAddressGetAddressBytes` 部分だけも .NET 10 `System.Net.IPAddress` と照合し、IPv4-mapped loopbackを含め一致。差分修正は不要。Section 12統合前なのでbuild・描画確認は保留。
- `ConfirmScreen.cs` と `.cpp/.hpp` を再監査。questionとyes/noの配置、modal/nav scope、cancelを初期focusにするdispatcher順、Escapeでfalseを返してHandledにする動作、StartScreen/UiCapture/GamepadUiChecksの直接接続は一致。追加修正なし。
- `PlayScreen.cs` と `.cpp/.hpp` を全メソッド再監査。モード/ハンター順、5画面の再構築、オンライン一覧と非同期応答、endpoint解析、設定保存と `LaunchPlan`、story/demo/picker/vote/preview、StartScreen/InGameMenu/UiCaptureの接続を照合した。C# `Math.Round` の ties-to-even と同じにするためBodyGrid/BarRowの3箇所を `MathRoundToInt32` に変更し、非Online面の初回 focus はC#同様に同期実行、Onlineだけ dispatcher 経由に修正。Section 12一括交換のためbuild・実行確認は保留。
- `SettingsView.cs` と `.cpp/.hpp` を全体再監査。5ページ/Controlsの3 sub-page、FOVの即時反映とCancel復元、Display・Audio・入力・Profileの保存順、FPS stop、crosshair/radar表示、Android touch項目、stylus placement、ログ共有/endpoint検証を照合。StartScreen・InGameMenu・UiCapture・GamepadUiChecksの直接接続も一致し、今回の追加修正はなし。Section 12一括交換のためbuild・実行確認は保留。
- `PauseMenuView.cs` と `.cpp/.hpp` を全体再監査。項目の条件/順序、vote timerとAccept/Deny後のResume、200ms refresh、fullscreen label、Escape/初回focus、host高さに対する0.5〜1.0縮尺を照合。AndroidのStartScreen、desktopのInGameMenu、UiCaptureの呼び出しも一致し、追加修正はなし。Section 12一括交換のためbuild・実行確認は保留。
- `EndPanelView.cs` と `.cpp/.hpp` を全体再監査。ballot snapshot/order、カード生成と選択、leader/tally差分描画、hunter/suit commit・再読込、ready/eligible表示、Shellの表示/refresh/hide順とUiCaptureを照合。空ballotの説明文にC#指定の水平・垂直中央揃えが欠けていたためnativeへ追加。Section 12一括交換のためbuild・描画確認は保留。
- `LobbyScreen.cs` と内包 `CustomTeamPicker` をC#全体と `.hpp/.cpp` で再監査した。roster/session revision、ping更新、最新12件のチャット、owner/team操作、ゲーム形式・数値入力の検証、250ms後の自動反映、map/custom-team picker、thumbnailの生成・解放、timerのattach/detachと閉鎖経路を照合。`StartScreen` の生成・MatchRequested/Closed配線も確認し、差分なし（verified no-op）。統合ビルドはSection 12完了後。
- `InGameMenu.cs` と `.cpp/.hpp` をC#全体および直接呼び出し元 `Shell` と照合した。pause/settings/map-vote stack、Resume/Fullscreen/Spectate/Rejoin/Record/Leave/Quit、vote時の二段pop、EscapeのHandled、閉鎖中のshared lifetimeを確認し、差分なし（verified no-op）。Section 12統合buildは保留。
- `PauseMenu.cs` と `.cpp/.hpp` をC#全体、RendererのEscape/Poll呼び出し、ShellのTickUi/Open/Close/Quit/Leave接続と照合した。フラグと順序は一致。`window.Focus()`の保護がnativeだけcatch-allだったため、C# `catch (Exception)`相当の `std::exception` 捕捉へ限定した。Section 12統合build前の静的監査。
- Section 12統合後のWindows Release Ninja buildを再実行し、Avalonia/Skia・GUI・PauseMenuを含む全コンパイルと `FruityPrime.exe` のリンクを確認。PowerShell起動時に `mingw64/bin` がPATHに無く `cc1plus.exe` のDLL解決に失敗した初回診断を除き、MSYS2 binをPATHへ追加した実行ではエラーなし。増分確認は exit 0 / `ninja: no work to do`。画面runtimeは未実施。Android buildはユーザー指示どおり未実施。
- `AndroidGamepadProfile.cs` を `.cpp/.hpp` へ移し、`InputDevice.GetMotionRange` の有無で右スティック・trigger軸を選ぶ順、左スティックの両軸判定、未対応軸のゼロ値を照合した。`GamepadBridge::HandleMotion` へ接続し、C++旧実装がフレーム値0を根拠に別軸へ切り替えていた差を修正（対応軸が0を報告してもC#同様その軸を読む）。Android build・端末実行は未実施。
- `AndroidThumbnails.cs` と `.cpp/.hpp` を全体照合した。host がApplication時点で登録されActivityをrender時に引くこと、worker上限が `min(core数, heapMB/24)` であること、進捗ごとに60秒stall時計を戻すこと、全画像完了時にmarkerを待たないことを反映した。`MainActivity.RenderPreviews` のMovingBackdrop停止・keep-screen-on・fallback進捗/所要時間報告を照合し、存在しない `Thumbnail*Ref` 型をC#のrooms/reportと共通 `IThumbnailHost` に一致するnative vector/callback/shared_futureへ置換。Android ABI buildで検証中。
- `AndroidGamepadHaptics.cs` のnative対応をC#原本と照合。デバイス/vibrator不在時のno-op、`HasVibrator`、1〜500ms、API 26以上のone-shot/amplitude-control、未対応時のdefault amplitude、旧APIの`vibrate(ms)`、`Stop()`の`Cancel()`を対応させ、Android manifestへVIBRATE permissionを追加。JNI descriptorとAPI-level取得の例外確認も監査した。
- `GamepadBridge.cs` をPR #1と作業ツリーのC#原本に照らして監査し、端末ごとの状態/ID/name/family、Gamepad/Joystick source判定、profile/capabilities、haptics登録、key repeat、motion/hat、clear/remove処理を実装した。C++側がグローバル状態を更新していた差と、DPADだけのdeviceをgamepad扱いする差を修正し、trigger button判定はC#同様`GamepadManager`の設定依存thresholdへ任せた。`MainActivity.cs`の起動/終了/フォーカスと`InputManager`通知をActivity owner経由で接続した。
- `MainActivity.cs` と native の lifecycle/render-preview/start-wait/pause/end を全体監査。`StartMatch`のin-process preview停止待ちとhunter-shot retire、100ms end-panel tick/Hide、pause中のend-panel抑止、UI surface dispatcher/render pump、lobby保持終了と `NetSession.ResetMatchState`/`ResumeLobby` を反映し、`AtomicSharedPtr` をAndroid libc++対応へ修正。TouchOverlayViewからend-panelへのタッチ配送もC#原本どおり接続。C#にない `MainActivity.cpp::CustomizeAppBuilder` は `MainApplication.cs` のbuilder処理を担うnative host seamとして別途照合した。
- `AndroidHunterShot.cs` を `AndroidHunterShot.cpp/.hpp` に移し、PR #1 の追加差分とC#原本を照合した。最新要求だけを処理するqueue、semaphore相当のpermit、4秒のRetire待ち、pbuffer/Sceneの再利用とresize再生成、match中のnull応答、最大3回のpreview draw、RGB readbackから上下反転BGRA変換、例外ログと失敗後の無効化を反映。ビルド時に検出したConsole出力のnamespace誤りもC# `Console.WriteLine` 対応先へ修正。`MainApplication` installと`MainActivity` Retire接続済み。Android ABI build継続中。
- `AndroidWebLink.cs` を `.cpp/.hpp` 化し、Application Context保持、`MainActivity.Instance` 優先、ACTION_VIEW Intent、Activity以外のContextにだけ `FLAG_ACTIVITY_NEW_TASK` を付ける条件、例外時の `[android] could not open ...` とfalse応答をC#原本/PR #1と照合した。build時に見つけたConsole出力先namespace typoも修正。Application Contextをnative hostから渡し、`MainApplication`登録済み。Android ABI build継続中。
- `AndroidUiSurface.cs` と `.cpp/.hpp` を一ファイル単位で全体監査した。rootの透明背景・透明レベル・Dark theme、scale/density、focus post、hide時の状態順、premultiplied RGBA frameのcopy/version、touch routingとpointer IDを照合し、透明/theme設定とunchecked long相当のwrapを反映した。`std::atomic<std::shared_ptr>` は Android libc++ 対応の `AtomicSharedPtr` に置換。dispatcher/render timer pump、MainActivity/GameViewのUI tick・描画合成、およびTouchOverlayView経由の入力配線まで接続済み。Android build待ち。
- `AndroidUiOverlay.cs` と `.cpp/.hpp` をshader sourceからReleaseまで全体監査した。uploadの寸法/byte数判定、ES 3.0 shader link、quad/texture setup、上下反転UV、premultiplied blend、depth/state復帰の範囲、失敗時の無効化とcontext-current前提の解放を照合。C++のcatch-allはC# `catch (Exception)` にない捕捉を増やしていたため除去。GameView配線済み。Android build待ち。
- `GameView.cs` と `.cpp/.hpp` を全体監査し、固定更新後のsession拒否/timeout/lobby遷移、load成功/失敗通知、render後のUI texture合成とhunter preview hole、`B|Start`によるchat cancel、`EndMatchToLobby`へのUI thread dispatchを接続した。C#の`catch (Exception)`を越えるcatch-allを除き、同時に`Exception.ToString()`相当のnative診断へ修正。C#更新順に合わせるため`MainActivity::EndMatchCore(keepSession)`を追加。UI surfaceのUI thread tick・描画・タッチ配線まで接続済み。Android build待ち。
- `TouchOverlayView.cs` と `.cpp/.hpp` を全体監査し、色・Paint設定・描画・密度によるレイアウト・touch actionを照合。結果パネル表示中はタッチボタンを描かず、pointer down/upは消費し、それ以外のpointer 0をAndroidUiSurfaceへ渡すC#の経路を反映。Android build待ち。
- `MainApplication.cs` のbuilder処理はnativeで `MainActivity.cpp::CustomizeAppBuilder` に実装されているため、その対応範囲を照合。`WebLink` と `AndroidHunterShot` の登録漏れを追加し、ログ共有/WebLinkにはActivityではなく `getApplicationContext()` の結果を渡すよう修正。`AndroidUpdateInstaller.cs` も別途照合し、C#同様に構築時はActivityを保持せず、各操作時に最新の `MainActivity.Instance` を取得するよう変更。Android build待ち。
- Android NDK 27.2.12479018 / compile API 24で `arm64-v8a` と `x86_64` をconfigure済み。最初のcompileはMSYS2 include root全体を渡してNDK libc++の `stdlib.h` を隠したため失敗し、curl/libarchiveの必要ヘッダーだけを隔離したinclude rootへコピーして両ABIを再configureした。ユーザー指示により、Android全ファイルのC#照合完了までAndroid buildは実行しない。
- この監査後にWindows Release `ninja -k 0` を再実行し、終了コード0・`ninja: no work to do` を確認。対象にAndroid専用コードは含まれないため、Android buildは未確認。
- 上記の監査中にWindows Release Ninja buildをMSYS2 shellで再実行し、Section 12を含む全ターゲットが成功した。PowerShellからの直接実行ではMSYS2環境が設定されず診断なしで失敗したため、正しいMSYS2環境で再確認した。Android SDK/NDKは後続調査で存在を確認済み。Android buildは全Androidファイルの監査完了後に実施する。
- Section 12 の一括切替、`MatchStart`、RenderWindow/GL の依存を含む Windows Release Ninja build が
  `ninja -k 0` で成功した（ログ: `%TEMP%\fruity-prime-native-ninja-20260927-retry4.log`）。
  POST_BUILD の背景 JPEG コピー元だけ実在する C# asset path へ直した。これは build 成功の記録であり、
  画面実行・runtime 検証は未実施。Section 12 の依存接続を含むWindows Release buildは成功。
- 11 `MatchStart` も同 build に含めてコンパイル・リンク確認済み。

- `NetLaunch.cs::TickTerminalLobby` の移植後監査を完了。`Renderer::OnRenderFrame` のフレームレート設定直後へ接続し、
  persistent lobby 復帰・scene 有無・通信終了・match load と clear/swap/base frame の順を照合した。
  後続の Section 12 Release build でもコンパイル・リンクを確認した。
- `KeyRow.cs`、`PadRow.cs`、`ServerRow.cs`、`ServerBadge.cs` と各C++対応物を原本から再監査した。
  キー変換・pointer tap・pad capture/conflict・server status/selection・badgeの住所分類と描画で修正差分はなかった。
  対象C++の `git diff --check` は通過。
- `AndroidMatch.cs` と `.cpp/.hpp` を全体照合し、オンラインmatch開始時の `DisableCheatsForMatch()` 呼び出し漏れ、bot skill上限の `3` 対 `2`、build時に見つかった `RoomPlayerCount()` 呼出漏れを修正。demo/adventure/local/network各経路の初期化順、slot/team割当、map/mode選択、bot level、save確定も照合した。Android ABI build継続中。
- `AndroidApp.cs` と `.cpp/.hpp` を全体照合し、Activity lifetimeの遅延 `MainViewFactory`、SingleView fallback、両経路の `UiScaleHost`、`CrashReport` 初期化/Android unhandled exception報告、`StartScreen` のDone/MatchRequestedを接続。削除済み `HomeView` adapter参照を現行native `StartScreen` へ置換し、MainActivityのlifecycle参照とhost interfaceも追随させた。Android buildは全Androidファイルの監査完了後に実施する。
- `AndroidLogShare.cs` と `.cpp/.hpp` を比較監査し、cache staging pathと古いzipの best-effort 削除、FileProvider authority/URI、ZIP MIME、stream/subject extras、read grant、chooser title、Application Context用NEW_TASK、例外時のログとerror返却が一致することを確認。修正差分なし。Android buildは全Androidファイルの監査完了後に実施する。
- `TouchControls.cs` と `.cpp/.hpp` を全体照合し、button配置・visibility・touch/hudターゲット・aim/stick pointer処理・swipe boost/double tap・pad hide/force-visible状態を監査。`TakeAimDelta()` に欠けていた `AimInputSourceTracker::Pointer(x, y, true, TickCount64())` 呼び出しを追加。Android buildは全Androidファイルの監査完了後に実施する。
- Androidの全ファイル監査後に両ABIをbuildし、最初の共通コード障害として `SfxMixer.cpp` のNativeRuntime/OpenTK `using` 宣言が最終行にあり先行参照から見えない問題を検出。`SfxMixer.cs` のMath演算/通常unchecked算術との対応を確認して宣言をinclude直後へ移動。両ABIのbuildを再実行する。
- Android ABI buildの再実行で `HashCode.cpp` のentropy fallback lambdaに `std::uint64_t` と `0ULL` の推論型不一致が出たため、値を変えず `std::uint64_t{0}` に型を揃えた。標準 `HashCode.Combine` のno-entropy fallback動作と同じ。両ABIのbuildを再実行する。
- 次のAndroid ABI compile errorは `Stopwatch.hpp` の defaulted `<=>` が `std::strong_ordering` を必要とするのに `<compare>` が未includeだったこと。C# `Stopwatch`/`TimeSpan` の意味は変えず、標準依存ヘッダーを追加して再実行する。
- Android ABI buildで `GameView.cpp` のコンストラクター定義だけが `TouchControls*` になっていたため、C# `GameView`、native宣言、`RenderLoop::Create` の `TouchControls&` と照合して参照へ修正。両ABI buildを再実行する。
- 続くAndroid NDK compile errorを受け、既存 `PreviewService.cs` と `.cpp/.hpp` を照合。service intent、空room時の完了/StopSelf、workerの例外・finally、ゲームファイル初期化、offscreen render、10 worker宣言が対応していることを確認し、`AttachCurrentThread` だけをNDKの `JNIEnv**` signatureに合わせた。両ABI buildを再実行する。
- Android対象17ファイルのC#監査完了後、NDK target `fruity_mphread_native_android` を arm64-v8a / x86_64 の両方でbuildし、2 ABIとも exit 0・static library link成功を確認した。ログは `build/native-android-arm64-v8a/build-retry10.log` と `build/native-android-x86_64/build-retry10.log`。端末起動・実機動作は未確認。
- `CompatibilityCheck.cs` と `ModEntry.cs` の bundled OpenAL resolver をC++側で再監査した。Appleでは system frameworkへ直結せず、`AL.cpp` が C# と同じ executable directory の `libopenal.1.dylib` を初回呼出し時に読み、`CompatibilityCheck` の symbol-address照合でも同一handleを使うようにした。CMakeもAppleでの system OpenAL linkを外し、Windows/Linuxの直接linkは維持。
- 上記 `CMakeLists.txt` / `AL.cpp` の変更後、MSYS2 MinGW64 Release `ninja -k 0` が exit 0、全50 build stepを完了。ログ: `/tmp/fruity-prime-native-section2-openal-20260927.log`。macOS native buildと実行時smokeはこのWindows環境では未実施。
- Section 12統合後のWindows Release build成功を反映し、Section 3の `ThumbnailBatch` / `ScreenCapture` / `ThumbnailCapture` / `ThumbnailLog` をbuild待ちから完了へ更新した。
- `GuiLauncher.cs` と `.cpp/.hpp` の統合監査で `TryRun` / `EnsureSetup` / Android・display分岐 / Shell接続を照合した。
  二つのcatchで `PlatformDiagnostics.Report` を呼ぶよう接続し、C#例外時の診断内容を保持した。
  `ModEntry` の `PlatformDiagnostics.Start` と smoke/GLFW/window diagnostic dispatch も後続で接続し、
  Windows Release buildで確認した。

- `LauncherWindowCheck.cs` と `.cpp/.hpp` を原典と照合し、geometry設定のtry/finally復元、20/40 frame判定、
  4組のshader compile/link、back bufferのpixel thresholdとGL error、1100x740 resize後の再描画、
  callback内例外のcloseを確認した。`Shell::AfterDraw` のshot処理より前に接続した。Windows Release build済。
- `ThumbnailWindowCheck.cs` と `.cpp/.hpp` を照合し、workerと同じwindow settings、legacy GL 2.1条件、
  context診断、FBO/textureの生成・解放、quads描画、中央pixel判定を確認した。
  `ThumbnailCapture.WindowSettings` のnative可視性をC# `internal` に合わせ、OpenTK薄い層に
  `Rgba8`・`DrawBuffer`・`Vertex2` を同じGL定数/entry pointで追加した。
  `GlfwWindow`生成時にC#の`MakeCurrent`と同じcontext-current処理があることも確認した。
  外側のcatchをC# `catch (Exception)`相当へ修正し、Windows Release build済。`ModEntry`引数配線済。
- `GlfwPathCheck.cs` と `.cpp/.hpp` を一ファイル単位で移植した。temp fixture、working directory、
  `GameFiles.Root`、paths.txt、GLFW context policy、GLFW後のcwd/paths再確認、finally復元・recursive deleteを
  C#の順で実装し静的監査した。新たに必要だった `Directory.CreateTempSubdirectory`、
  `Directory.SetCurrentDirectory`、`Directory.Delete(path, true)` はNativeRuntime/System/IOへ追加した。
  外側のcatchをC# `catch (Exception)`相当へ修正し、Windows Release build・`ModEntry`配線済。
- `PlatformDiagnostics.cs` と `.cpp/.hpp` を比較し、起動環境の行・macOSのdylib一覧と永続化、
  platform別library名、native exception記録、`file -b` の2秒制限、IO/権限エラー時のログ通知を実装した。
  `GuiLauncher.cs` の二つの失敗経路も比較し、GLFW/Skiaの `PlatformDiagnostics.Report` 呼出しを接続した。
  `ModEntry` 起動時の `Start` と診断flagsもC#の分岐順で接続した。標準C++例外がC# stack traceを保持しない差は
  NativeRuntimeの例外仕様に従う。Appleのbundled OpenAL解決はC# `OpenALLibraryNameContainer.OverridePath` と同様に
  executable directoryの`libopenal.1.dylib`をnative bindingが遅延ロードするようにし、system OpenALへのlinkを外した。
- Section 2の診断pairと依存APIを含む Windows Release Ninja build が成功した
  （`ninja -k 0`、ログ: `%TEMP%\fruity-prime-native-section2-diagnostics-20260927-retry1.log`）。
  `CompatibilityCheck`、`GlfwPathCheck`、`PlatformDiagnostics`、GLFW/OpenAL binding、
  NativeLibrary、再帰ファイル判定の追加分をコンパイル・リンクした。smoketestや画面実行はしていない。
- 5つのDiagnostics原典を再監査し、C#が捕捉する通常例外とC++の`catch (...)`の範囲差を修正した。
  `CompatibilityCheck`、`LauncherWindowCheck` callback、`PlatformDiagnostics`の記録callback、
  `ThumbnailWindowCheck`、`GlfwPathCheck`の各catchを`std::exception`へ対応させた。
  例外後のGL資源解放・finally相当処理のcatch-allは維持した。Windows Release `ninja -k 0`は8 stepでexit 0
  （ログ: `%TEMP%\fruity-prime-section2-diagnostics-reaudit-20260927.log`）。既存の`offsetof`警告のみで、
  smoke test・画面実行・macOS buildは未実施。
- `Sfx.cs` と `Music.cs` の PR #1 差分を一ファイルずつ原本と照合し、診断呼出し、catch/filter、fallback 文言と
  状態更新順を確認した。`Sfx::Load` と MusicPlayer 初期化失敗の両native経路へ `PlatformDiagnostics::Report` を
  接続した。Windows Release の `ninja -k 0` が成功（ログ: `%TEMP%\fruity-prime-native-diagnostics-audio-20260927.log`）。
  `offsetof` の既存警告のみ。macOS build・起動は未実施。
- `ModEntry.cs` の Section 2 診断分岐は `smoketest` と shell 限定の GLFW/thumbnail/window checks を含め
  native側へ配線し、`PlatformDiagnostics::Start` の呼出し位置も `DebugLog::Attach` 後と照合した。
  同 build でコンパイル・リンク済み。runtime check は未実施。
- `ModEntry.cs` と `ModEntry.cpp` を引数・分岐・順序で全体比較した。`-teamprobe` の設定漏れを
  `MapAudit::TeamProbe` へ接続し、`-hosts` の20秒待機を超えて応答が返る場合も callback の参照先が残るよう
  共有状態へ変更した。render overrides、network controls、MapGen probes、server install/host、map vote/HUD capture
  を原本と照合し、CLI option inventory は完全一致。Windows Release `ninja -k 0` 成功
  （ログ: `%TEMP%\fruity-prime-modentry-audit-20260927-r2.log`）。GUI/runtime/macOSは未確認。

- `Shell.cs` を再開後に再監査した。Run/finally の停止・破棄、match/lobby/end-panel の遷移、settings再読込、pause menuの開閉、framebuffer resizeとpointer基準、入力変換、window capture の抑止とscript順をC#と照合し、追加修正はなかった。C#の `Exception.StackTrace` に相当する値は標準C++例外に保持されないため、match-start例外は既存の `DebugLog::Exception` がnative stackをログへ記録する。Section 12の一括交換が未完了なのでビルドはしていない。

- `UiCapture.cs` と `UiCapture.cpp` を一ファイル単位で照合した。26画面の順番・名前・寸法、固定server/host data、phone/sampleの設定時点、dispatcher jobとlayoutの順、bounds JSONの階層・丸め・escape、PNG保存、失敗時のreturnとconsole文を確認し、修正差分はなかった。C++はC#の非表示Windowをheadless `EmbeddableControlRoot`に置き換えるが、同じサイズで実画面を開かず描く役割を保つ。`ModEntry`の `-uishot` 呼び出しも引数・終了コードを照合済み。ビルド・撮影実行はしていない。

- `GamepadUiChecks.cs` と `.cpp` を一ファイル単位で再監査し、35 assertion の文言・順序、focus/navigation、keyboard/pad capture、preset変更、pause/map vote、optional capture と `GamepadChecks::Run` の呼び出し位置を照合した。SettingsView は attach 時に選択中タブへ focus する job を post するため、C#と同じく `ShowSection("Controls", 1)` を job pump より先に行う必要がある。native helper が attach 後すぐ job を実行して初期タブへ focus していた順序を修正した。残りの pump 箇所はC#の明示した `RunJobs` と同じ操作順で、他の attach callbackにも同型のずれは確認されなかった。静的監査のみで、Section 12 一括交換前のbuild・実行はしていない。
- 上記 `GamepadUiChecks` のSection 12 統合buildで、C#の `SliderRow` 参照に相当する `First<SliderRow>` のnative参照を `FocusNavigator::Focus(Control*)` に渡す際のポインタ変換漏れを修正した。C#と同じ要素をfocusする。
- `SettingsView.cs` と `.cpp/.hpp` の再監査で、`UiWord` のforward declaration欠落と `RequireReference(_settings)` が既に参照を返すのに再度dereferenceしていた点を修正した。保存対象はC#と同じ `_settings` の `MenuSettings` instance。
- `PauseMenuView.cs` と `.cpp/.hpp` のcompile再監査で、native版 `Deck::Face` factory の呼び出し括弧漏れを直し、Escape key handler のAvalonia型を `Av::Input` に明示した。C#と同じFace色とEscape時のResumeイベントを維持する。
- `CreateServerScreen.cs` と `.cpp/.hpp` の該当箇所を再照合し、ProgressRowをControlとして使うtranslation unitのinclude欠落と、`PickRow`・画面3種のpointer/key eventがゲーム入力namespaceへ誤解決される箇所を修正した。C#同様、Avalonia input event型を使い、PickRowはEnter/Space/Right、Escapeは各画面のclose/cancel経路へ入る。
- `HunterStand.cs` と `.cpp/.hpp` の入力箇所を比較し、cursorとpointer handlerのAvalonia型を明示した。ドラッグ開始・座標・capture・spin更新・release/capture loss時の状態解除はC#どおり。
- 再buildで `KeyRow`/`PadRow` にnativeには存在しないAvalonia `IsFocusedProperty` を登録していた誤りを修正した。C#のfocus renderingは両native controlのGotFocus/LostFocus overrideが同じく `InvalidateVisual()` し、Enabledはnative `InputElement` 本体がrender登録するため、専用static登録は不要。
- `UiSurface.cs` と `.cpp/.hpp` のbuild差分を監査し、GamepadNavigation event登録をnative Event APIの `+=` に合わせ、C#同様のinstance methodである `Covers` に誤った `const` を付けていた点を除いた。
- `Shell.cs` と `.cpp/.hpp` のWindows owner HWND、window title、`-shellshot` を照合し、GLFW native includeの順序、C#継承プロパティに当たる `RenderWindow::Title` forwarding API、およびsceneがないwindow captureの欠落を補った。追加 `ScreenCapture::SaveWindow(width,height,path)` はC#と同じback buffer RGB readback・pack alignment・black-frame判定経路を通す。
- `UiDesigns.cs` の既定実行adapter欠落をlink errorから追跡し、C#と同じ `GuiLauncher.EnsureSetup` → directory作成 → UI dispatcher → `UiCapture.Capture` → console出力のlive adapterを接続した。`UiCapture::Capture` はC#のinternal相当としてheader内公開。
- compile/link後のPOST_BUILDで背景画像だけnative Assetsに存在せずcopy失敗したため、CMakeのcopy元を実在するC# asset `src/MphRead/Assets/Backgrounds/launcher-bg.jpg` に修正した。native launcher resource URIはflat filename fallbackも持ち、ロゴ/markと同じく実行ファイル横へ配置される。
- `UiDesigns.cs` と `.cpp/.hpp` の該当構築箇所を比較し、`SliderRow` includeと `Opened` 戻り値を修正した。C#側の `Control` と同様、GridをBorderに包んだ開閉行も共通Controlとして親へ渡す。
- `UiCapture.cs` と `.cpp/.hpp` のcapture失敗経路を再照合し、ログ用 basenameを取得する際に `string_view` を `PathGetFileName(const std::string&)` へ渡していた型変換漏れを修正した。C#と同じく例外を画面名付きのcapture失敗ログへ変換する。
- `LobbyScreen.cs` と `.cpp/.hpp` のchat entry handlerを比較し、Enter eventをAvalonia `KeyEventArgs`/`Key` として扱うようnamespaceを修正した。Enter送信と `Handled` 設定は維持した。
- 再buildでnative routed eventのsender引数も必要と分かったため、LobbyScreenのC# `(_, e)` と同じ2引数callbackに揃えた。
- `PlayScreen.cs` と `.cpp/.hpp` のbuild露出箇所を照合した。BarRowのC# switch expressionをnativeのDock分岐へ正しく翻訳し、CornerRadius型のshadowing、Deck Face factory、Avalonia key/focus eventのnamespaceを修正した。各Dock位置の左右gapとC#のFace色・キー動作は同じ。
- Section 12のShellが使う `Renderer.cs::Scene.UnloadGl` をC#原本と照合し、one-window lifecycleに必要なGL解放をSection 13から先行移植した。palette texture map、cached model display listとcache、frame/render buffers、3 scene textures、4 shader programsを同じ順で解放しIDを0へ戻し、Headless時はreturnする。両model cacheを列挙する `Read::CachedModels` とGL `DeleteRenderbuffer` entry pointもC#/OpenTK APIに合わせて追加した。build未確認。
- Section 12のbuildエラーを `GamepadProfilePanel.cs` と対応pairで照合し、`LauncherPrefs.hpp` の相対includeを実ファイル位置に合わせて修正した。プロフィール操作・例外表示はC#と一致。
- 再buildで `GamepadProfilePanel` の `Path.Combine` に当たる `PathCombine` includeが抜けていたため、C#のファイル名・保存先と同じnative `System.IO` APIをincludeした。
- `UiScaleHost.cs` と `.cpp/.hpp` をbuildエラーと合わせて再監査した。Android dp→desktop pointの換算、factor・bake scale・measure順は一致。依存する `UiLayout.Factor` の `Math.Round` と `std::round` の中間値規則をmidpoint-to-evenへ修正し、Android native toolkitの `TopLevel.RenderScaling()==1` 固定を補うため、接続済みActivityのdisplay densityをbake scaleへ使うようにした。`DebugLog.hpp` の相対includeも配置に合わせて修正した。
- `UiSurface.cs` と `.cpp/.hpp` は先のC#比較記録に対し、初回buildで露出したnative includeを再点検した。`UiOverlay`・`DebugLog`・`Mods/Input` の相対パスを実配置へ修正した。入力・surfaceの動作順差分はなかった。
- `Shell.cs` と `.cpp` のbuild依存も監査した。`AfterDraw` の `LauncherWindowCheck` 呼出しはSection 2の未移植クラスに依存するため、順序表どおり同Sectionで実装・接続する記録を残し、Section 12中のinclude/callは除去した。shellshot経路は別の既存呼出しで維持。
- `StartScreen.cs` と `.cpp/.hpp` を全体照合した。Backdrop/wordmark/responsive bar、update/hint/gamepad timerのlifecycle、初回Setup、stack push/popとfocus、Play/Create/Settings/Lobby/Pause/Vote遷移、終了イベント、update installのpermission/progress/error/exit動作を確認し、差分修正はなかった。以前のbuildエラーに沿った `InputPrompt` / `InputSourceTracker` include修正と未使用 `GamepadUiRouter` include除去も維持した。Section 12一括交換が終わるまでbuildは保留。
- `ConfirmScreen.cs` と `.cpp/.hpp` をbuild診断と合わせて照合し、表示font accessorの呼出し漏れと親namespaceのInput名衝突を修正した。modal/no markの初期focus、Escapeのfalse応答、event順は一致。
- `FocusNavigator.cs` と `.cpp/.hpp` を再照合した。Avalonia `InputElement`・`NavigationMethod`・`Key`・key eventsが親のgame input namespaceに隠れる名前衝突を修正し、C#のfocus eligibility、default順、neighbor優先、方向距離とwrapの計算を確認した。
- `DeckTile.cs` と `.cpp/.hpp` のhash phase処理をbuildエラーに沿って再確認し、`Math.Abs(int.MinValue)` parity用 `OverflowException` をRuntimeの実際のglobal `System` namespaceへ修正して例外宣言をincludeした。
- `NativeRuntime/System/Net.cpp` のIPv6 named-scope変換をWindows SDKと照合し、MinGWで `if_nametoindex` を宣言する `netioapi.h` をWindows分岐へ追加した。リンク先 `iphlpapi` は既存設定済み。
- `LauncherHunter.cs` と `.cpp/.hpp` のrenderer dependencyを再確認し、forward declarationでは不足するScene state/preview呼出しの完全型として `Scene.hpp` を追加includeした。draw/fallback動作差分はなし。
- NativeRuntimeのAvalonia `Popup` はVisual baseの `enable_shared_from_this` と二重継承で曖昧になっていたため、独自の二重継承を除き、共通AvaloniaObject所有参照をControlへdynamic castしてoverlay layerへ渡すよう修正した。Panel親/overlay親の分岐は維持。
- `LobbyPlayerRow.cs` と `.cpp/.hpp` をbuild diagnosticsと照合し、`GuiTheme::Display` をfont-family accessorとして呼ぶ形に揃えた。名前・team・hunter/suit/pingの表示順と列構成は一致。
- `KeyRow.cs` と `.cpp/.hpp` をC#原本から再監査した。クリック後releaseでlisten開始、tap/drag判定、mouse/wheel/key binding、GLFW key変換、gamepadからPadRowへの移動、focus解除時のlisten解除、表示文言と描画を照合。Focused変更はTopLevelが対象Visualをinvalidateし、Enabled変更はInputElementの共通AffectsRender登録でC#の描画更新と一致。rounded clip typeはAvalonia root namespaceを参照する。
- `ServerRow.cs` と `.cpp/.hpp` を原本から全体照合した。列幅とnarrow判定、status/metadata/players/ping、hover・tap・focus・keyboard、map crop/scrim、名前tailのUTF-16長、各描画座標のties-to-even丸めとgetterを確認。buildエラー行のpointer position Visual引数とAvalonia Matrixも明示し、hover傾きとscale transformの数式はC#どおり。依存する変更済みNativeRuntimeのタッチ経路は `TouchBegin` が常に `ClickCount=1` だったため `DoubleTapped` が発火しない差を修正し、Avaloniaのdouble-tap時間・領域に沿ったtouch click countと、double tap後の `Tapped` 抑制を追加。Android固有の `ViewConfiguration` 値との一致はAndroid側の残ファイル監査で確認する。section 12統合前のためbuildは保留。
- `PadRow.cs` と `.cpp/.hpp` をC#原本から再監査した。button列挙順、held-button baseline、device/focus失敗、30ms capture timer、clear/picker/chord conflict解決、終了/visual-tree離脱時のcleanup、表示を照合。Avalonia `DispatcherTimer(interval, priority, callback)` は即時開始し、NativeRuntime版も同じ。Focused変更はTopLevel、Enabled変更はInputElement共通のrender invalidationが担う。既出のInput namespace/Managed helper/`RoundedRect` build修正も確認した。
- `Renderer.cs` の shell描画呼出し箇所をC++側と再照合した。シーンなしの `UiOverlay.DrawAlone → Shell.AfterDraw` と、マッチ中の `UiOverlay.Draw → LauncherHunter.Draw → Shell.AfterDraw`、寸法引数、swap前後の順は一致した。C#の `PixelSize` は `FramebufferSize` の別名。当時保留だったSection 13差分は後続監査・修正を完了し、Windows Release build済み。
- `LauncherHunter.cs` と `.cpp/.hpp` を全体照合した。window scene と専用 side scene の選択、side sceneの作成/ロード/resize、hunter/suit/正規化boundsの設定、preview drawの結果による `Drawn`、寸法/矩形不正時と例外時のフォールバック状態、初回成功ログを比較し、差分修正は不要だった。`NewSideScene` のnative実装はC#同様、windowのscene slotを変更しない。
- `UiOverlay.cs` と `.cpp/.hpp` を全体比較した。reserved texture ID、unit 0 upload、resize時のTexImage/SubImage、premultiplied blend、unit 1解除、固定機能quad/texture座標、GL stateの復元範囲、シーンなしのclear/photo/overlay/hunter順、releaseを照合し修正不要。build・画面実行はSection 12未完了のため未実施。
- `[PR #1](https://github.com/Zection6V/Fruity-Prime/pull/1/changes)` のC#追加差分も原本参照として確認した。`LauncherNoise`/`NoiseField`の宣言順と時計開始時点を照らし合わせ、nativeの初回アクセスlazy initializationを修正した。
- `LauncherPhoto.cs` と `.cpp/.hpp` を再照合した。アスペクト比crop、static/movingの切替、noise shader compile/linkと失敗時static背景へのfallback、uniform・multi-texture座標、embedded JPEGのdecode/upload、全unpack state、sampler level/filter/wrapを比較し、nativeのAvalonia asset/JPEG decoder利用は不透明JPEGの同じRGBA画素を渡すruntime adapterとして一致した。差分修正なし。build・描画実行は保留。
- `MapCardPicker.cs` と `.cpp/.hpp` を全体比較し、factoryのコード分割・metadata、選択のordinal-ignore-case、空リスト時の説明/無効化、選択表示、Done/Cancelled、Escapeを照合した。`LobbyScreen::OpenMapPicker` の呼出しとイベント後のdraft更新・preview・dirty通知・page復帰もC#と一致。nativeは子タイルのattach後に初期focusをdispatcherへ送るが、親 `OpenPage` のfocus jobとの順序もC#側の「タイルfocus後に親focus job」と同じ。修正なし。build・実行は保留。
- Section 12 の接続待ち記録をソース参照と照合し直した。Play/CreateServer/Setup は StartScreen と UiCapture、Lobby は StartScreen、Settings は StartScreen・InGameMenu・UiCapture、PauseMenuView は StartScreen・InGameMenu・UiCapture から参照されている。これらの画面接続は現ツリーで解消済み。GuiLauncher の PlatformDiagnostics 依存と Section 13 待ちは残す。

- `Renderer.cs` とnative実装を一ファイル単位で再監査した。60 Hz simulation内のfreeze/network/gamepad/spectator/hit-claim順、draw内のEndScreen/thumbnail/AimAssist/FOV/wireframe、one-window shell描画順、pen pointer/focus、gamepad cancelを照合。監査で見つけたnativeベクトル演算API差、`WindowGeometry::Flush`漏れ、0寸法framebufferの未ガード、最大化状態callback欠落を修正した。GLFW maximize callbackからshell geometryを記録し、C#と同じ復元動作にした。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-renderer-audit-20260927-r3.log`）。画面runtimeは未実施。
- `PlayerHud.cs` の残件メモを再確認し、`UpdateWeaponSelect` の `WheelHeld → Absolute → Drag` 優先順、使用可能武器の配列・selection marker更新、weapon switch音、`UpdateWeaponDrag` のstepと選択更新、`UpdateWeaponArc` のpointer入力がnativeに反映済みと確認。`ModEntry.cs` の `-gamepadassisttelemetry` も `AimAssistTelemetry.Configure(ValueAfter(...))` が両実装で一致。コード修正は不要。
- `GameState.cs` のPR #1差分をhunkごとにnativeと比較した。TeamCount/固定DamageLevel、チーム検証、正の残り時間のみのtempo/alarm、tie時winner camera抑止、survival集計・結果standings、BountyTeams comparator、resetを照合。唯一抜けていた `PlayPickedMap()` を追加し、offline/shell/選択roomの条件成立時だけ `Shell::PlayAnother` を呼び、対象外では従来fade/exitへ戻すようC#と同じ順序で接続。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-gamestate-audit-20260927.log`）。runtime未実施。
- `PlayerEntity.cs` のPR #1追加差分をnative部分クラス全体と照合した。boost-aim lockの保持・reset・入力/更新での消費、spawn lifecycle/continuous phase reset/bridge通知、予測damage scope、claim damage二重乗算防止、allied-team判定、projectile launch-frame付きdamage/hit prediction、AimAssist telemetry、haptic feedbackを確認し全て反映済み。差分修正不要。GameState修正後のWindows Release buildで当該native translation unitも再リンク済み。
- `PlayerSound.cs` の追加1行を `.cpp/.hpp` と照合した。`_timeBeforeLanding > 30` のときだけ `Landing` feedbackを先行させ、元のterrain landing SFX ID・volume式を続ける順序まで一致。変更不要で、入力待ち表記を完了へ更新。
- `PlayerEntityNetAim.cs` をC#全文と `.cpp/.hpp` で比較し、位置履歴、remote aim、camera補正、NodeRef解決、spawn/facing、form・weapon・ammo・shot state、status/affliction、vector修復と各aim経路を監査した。`PlayerInput`・`NetPlayerBridge` に加え、`NetHooks` の記録、`NetDamage` の銃口位置、`NetUnlagged` の退避/復元呼び出しも順序と引数が対応。修正不要で、入力待ち表記を完了へ更新。`git diff --check` 通過。
- `PlayerEntityNetHud.cs` 全体を `.cpp/.hpp` と照合した。ネット対戦時のhealth参照、score列のsolo/network座標とEndPanel幅に応じたclamp、ping列の表示条件・`--`/999表示・色境界（80/160 ms）・描画引数が一致。`PlayerHud`、pro HUD、team scoreboardの全直接呼び出し位置も対応し、変更不要。`git diff --check` 通過。
- `PlayerAi.cs` のPR #1差分をhunkごとにnative実装と比較した。Insane difficultyの乱数表・clamp、personality分岐、照準dot/偏差、Tracked Speedを60 Hz tickへ換算した反復lead、発射遅延、Judicator距離、health spawn-aware探索を照合してnativeへ反映。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-playerai-audit-20260927.log`）。runtime未実施。
- `Formats.cs` のPR #1差分をnative側の `Paths`/`CollectionExtensions` と比較した。未設定時の `Export` 空文字、`SetPath`/`paths.txt` 読込値の絶対化（rooted/空値維持、不正値は保持）、可変spanの `Slice(uint)` を反映。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-formats-audit-20260927.log`）。
- `Utility/Archive.cs` のPR #1差分をC#実装と照合し、path入力をbyte span overloadへ委譲する形と検証/展開処理をnativeに追加。Readのメモリ展開が使うAPIを用意した。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-archive-audit-20260927.log`）。
- `Read.cs` のPR #1各hunkをnativeと照合した。`CachedModels` はScene unload用に先行移植済み。Androidストレージ上の一時ファイル競合を避けるLZ10メモリ展開→Archiver byte span抽出、0件エラー、アーカイブ名/例外内容付き診断を移植。仮Archiver宣言を実ヘッダーへ置換してWindows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-read-audit-20260927.log`）。runtime未実施。
- `Features.cs` の追加設定3項目をC#と照合し、RadarEnabled/ShowBackground/ShowOutlinesのLoad/CommitをRadarの既定値・Boolean parse・lowercase保存まで反映。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-features-audit-20260927.log`）。
- `Platform/AppPaths.cs` 全文とnative peer、`ConsoleSetup.Run`ほかの直接呼び出しを照合した。既定root、macOS `.app/Contents/MacOS` のResources選択、UserProfile下のApplication Support、`paths.txt`、ディレクトリ作成条件は対応。nativeがmacOS判定前にParentを解決していた評価順をC#の短絡条件に合わせて修正。`git diff --check` と Windows Release `ninja -k 0` 成功。
- `Platform/WebLink.cs` 全文と `.hpp/.cpp`、`Updater.OpenUrl`、Android側のadapter登録を照合した。interfaceのURL引数とbool応答、static Currentのnull初期値・set/get、HTTPS検証後のadapter優先と拒否時の戻り、desktop fallback順が対応し、修正不要。Android両ABI buildは既存のSection 14証跡を確認。
- `MapPick.cs` 全文を `.hpp/.cpp` と照合し、label重複解消、票のserver順、case-insensitive比較、packet適用/close、offline・online選択、cursor/scroll境界、resend、hit-layoutを確認した。`EndScreen`・`NetSession`・`Renderer`・`GameState`・描画側の直接呼び出しも引数と順序が対応。`NoteLayout` のnativeコピーとmanaged配列参照は現在の唯一の呼び出しが描画後に同じ内容を公開し、次描画ではhover読取後に更新されるため観測差なし。修正不要。
- `WindowGeometry.cs` 全文をnative peerと照合し、保存/復元・fullscreen/minimized/maximized・monitor-fit・debounce・直接呼び出し順が対応することを確認。C# `MonitorInfo.ClientArea` に対してnativeがGLFW WorkAreaを使っていたため、全モニターと現在モニターのClientAreaをビデオモード寸法に修正し、`FitToScreen`用WorkAreaを分離。OpenTKのlargest-intersection monitor選択と外枠基準`Location`もnative adapterに反映。Fit境界演算はC# unchecked int32に一致。`WindowMode`・`Renderer`の直接呼び出しを監査し、`git diff --check` とWindows Release `ninja -k 0` 成功。
- `CrashReport.cs` 全文とnative peer、`Program.Main`/`AppDomain`/`ConsoleWindow`の接続を照合した。初回のみの登録・報告、起動例外とunhandled経路、書込み先の順序とfallback、Windows console表示/所有時pause、終了コードの対応を確認。修正不要（Windows Release build済）。
- `EndScreen.cs` 全文とnative peerを照合し、Available/Ready、ルーム表示、遷移時のMapPick・preview cleanup、hit box境界、クリック/キー/パッド選択、respawn choice保存を確認。`Renderer`のframe/input順、`Shell`のPanelUp、`RespawnChoice`・NetPlayerBridge・HUD描画側の呼び出しも対応し、修正不要（Windows Release build済）。
- `WindowMode.cs` 全文とnative peerを照合し、startup/force・保存したborder/location/size・F11/Alt+Enter・fullscreen遷移順・topmost同期・設定値parseを確認。`Renderer`・`PauseMenu`・Settings/Shell・`WindowGeometry`の直接呼び出しも対応。OpenTK 4.9.4の`GetMonitorFromWindow`がfullscreen monitor優先、通常時はClientAreaとの最大交差で選ぶ点と、`NativeWindow.Location`が外枠座標である点をnative adapterへ反映。Windows Release `ninja -k 0` 成功。
- `InputSettings.cs` 全文と `.hpp/.cpp`、`PlayerControls`・`SettingsView`・`ModEntry`・`CompatibilityCheck`・`Renderer`・`ChatBox`・入力診断側の直接呼び出しを照合。35 bindingの順序、OpenTK 4.9.4のenum名/別名、Load/Save/Reset・既定値・Apply順を確認。`Load`のPath再評価とmouse enum表示のunchecked int32加算を修正。Windows Release `ninja -k 0` 成功。
- `RenderOptions.cs` 全文と `.hpp/.cpp`、`GameSettings`・`ModEntry`・`Renderer/Scene`・`SettingsView`・`DebugLog`の直接呼び出しを照合。既定値、clamp境界、FOV倍率、整数スケールのunchecked積、parse/fallback/`%`処理と呼び出し順が対応し、修正不要（verified no-op、Windows Release build済）。
- `SceneSetup.cs` のC#差分をnativeと比較し、roomごとのNetHealthSync reset、offline/single-player/active sessionからのresource profile選択、MapResourceRulesによるentity listとItemSpawnDataの解決を同じタイミングで接続。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-scenesetup-audit-20260927.log`）。
- `Metadata.cs` の全差分をhunk単位で照合した。multiplayer entity layerの3人/4人以上選択とTeamColorsの青・紫を含む4要素化を反映。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-metadata-audit-20260927.log`）。
- `Utility/Extract.cs` のC#差分を比較し、生成ROM rootを `Paths.SetPath` に絶対化させるため `Extract.Setup` は相対Combine値を渡し、エラー終了時のキー待ちはredirect/no-console安全な `ConsoleSetup::PauseIfInteractive` に統一。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-extract-audit-20260927.log`）。
- `Menu.cs` の唯一の追加 `FieldOfView = "78"` をC#と照合。native `MenuSettings::FieldOfView` に同名・同じ既定値がすでにあり、変更不要。直前のWindows Release build済み。
- `Scene.cs` のC#変更を照合し、`GetFlagBaseEntities` が誤って `FhBomb` リストを参照していたnative実装を `FlagBase` に修正。Windows Release `ninja -k 0` 成功（`%TEMP%\fruity-prime-scene-audit-20260927.log`）。

## 1. Platform helpers — 2 ファイル (新規 2), C# +73 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +40/-0 | `Mods/Platform/AppPaths.cs` | — 新規 | 完了（C#全文・直接呼び出し監査済、短絡評価順修正、Windows Release build済） |
| A | +33/-0 | `Mods/Platform/WebLink.cs` | — 新規 | 完了（C#全文・呼び出し元監査済） |

## 2. Diagnostics — 5 ファイル (新規 5), C# +497 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +149/-0 | `Mods/Diagnostics/CompatibilityCheck.cs` | .cpp,.hpp | 完了（C#監査・ModEntry配線・Windows Release build済、bundled OpenAL pathをnative bindingへ反映。macOS build/smoke未実施） |
| A | +103/-0 | `Mods/Diagnostics/LauncherWindowCheck.cs` | .cpp,.hpp | 完了（C#監査・Shell/ModEntry接続・Windows Release build済、runtime check未実施） |
| A | +101/-0 | `Mods/Diagnostics/PlatformDiagnostics.cs` | .cpp,.hpp | 完了（C#監査・GuiLauncher/ModEntry接続・Windows Release build済、macOS runtime未実施） |
| A | +83/-0 | `Mods/Diagnostics/ThumbnailWindowCheck.cs` | .cpp,.hpp | 完了（C#監査・ModEntry接続・Windows Release build済、runtime check未実施） |
| A | +61/-0 | `Mods/Diagnostics/GlfwPathCheck.cs` | .cpp,.hpp | 完了（C#監査・ModEntry接続・Windows Release build済、runtime check未実施） |

## 3. Mods leaves — 13 ファイル (新規 3), C# +1597 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +516/-0 | `Mods/MapPick.cs` | — 新規 | 完了（C#全文・直接呼び出し監査済） |
| A | +339/-0 | `Mods/WindowGeometry.cs` | — 新規 | 完了（C#全文・WindowMode/Renderer直接呼び出し監査済、monitor選択/ClientArea/WorkArea/外枠Location/unchecked演算を修正、Windows Release build済） |
| M | +164/-89 | `Mods/ThumbnailBatch.cs` | .cpp,.hpp | 完了（C#原本監査済み、Section 12統合後のWindows Release build済） |
| A | +150/-0 | `Mods/CrashReport.cs` | — 新規 | 完了（C#全文・Program/AppDomain/ConsoleWindow呼び出し監査済、Windows Release build済） |
| M | +135/-28 | `Mods/EndScreen.cs` | .cpp,.hpp | 完了（C#全文・Renderer/Shell/RespawnChoice/NetPlayerBridge/HUD直接呼び出し監査済、Windows Release build済） |
| M | +76/-5 | `Mods/WindowMode.cs` | .cpp,.hpp | 完了（C#全文・Renderer/PauseMenu/Settings/Shell/WindowGeometry直接呼び出し監査済、monitor選択と外枠Location adapterを修正、Windows Release build済） |
| M | +63/-6 | `Mods/ScreenCapture.cs` | .cpp,.hpp | 完了（C#原本監査済み、Section 12統合後のWindows Release build済） |
| M | +47/-15 | `Mods/InputSettings.cs` | .cpp,.hpp | 完了（C#全文・PlayerControls/SettingsView/ModEntry/CompatibilityCheck/Renderer/ChatBox/入力診断の直接呼び出し監査済、保存パス再評価とunchecked演算を修正、Windows Release build済） |
| M | +43/-0 | `Mods/RenderOptions.cs` | .cpp,.hpp | 完了（C#全文・GameSettings/ModEntry/Renderer/Scene/SettingsView/DebugLog直接呼び出し監査済、verified no-op、Windows Release build済） |
| M | +40/-0 | `Mods/SpectatorMode.cs` | .cpp,.hpp | 完了（C#全文・Renderer/NetHooks/NetSession/NetSessionLobby/NetCheckClient/入力・画面の直接呼び出し監査済、verified no-op、Windows Release build済） |
| M | +10/-18 | `Mods/ThumbnailCapture.cs` | .cpp,.hpp | 完了（C#全文・ModEntry/ThumbnailBatch/GlfwPathCheck/ThumbnailWindowCheck直接呼び出し監査済、撮影待機・再試行・camera override・cleanup一致、verified no-op、Windows Release build済） |
| M | +8/-7 | `Mods/GameSettings.cs` | .cpp,.hpp | 完了（C#全文・Shell/SettingsView/TextLauncher/Renderer/RenderOptions/FrameTiming直接呼び出し監査済、verified no-op、Windows Release build済） |
| M | +6/-1 | `Mods/ThumbnailLog.cs` | .cpp,.hpp | 完了（C#原本監査済、Section 12統合後のWindows Release build済） |

## 4. Update — 4 ファイル (新規 0), C# +152 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| M | +139/-3 | `Mods/Update/UpdateCheck.cs` | .cpp,.hpp | 完了（C#原本・直接呼び出し監査済、CancellationTokenをlibcurl中断まで接続。buildはSection 12統合後） |
| M | +7/-0 | `Mods/Update/Updater.cs` | .cpp,.hpp | 完了（C#原本・直接呼び出し監査済、追加修正なし。buildはSection 12統合後） |
| M | +4/-1 | `Mods/Update/DesktopUpdate.cs` | .cpp,.hpp | 完了（C#原本・直接接続監査済、例外ログと無効PIDを修正。UpdateDownloadのcancel伝播も別途監査・修正。buildはSection 12統合後） |
| M | +2/-10 | `Mods/Update/BuildVersion.cs` | .cpp,.hpp | 完了（C#原本・直接参照元監査済、parser/stamp選択一致。C++ release stampはbuild adapter経由、buildはSection 12統合後） |

## 5. Chat — 2 ファイル (新規 1), C# +36 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +32/-0 | `Mods/Chat/NetChat.cs` | — 新規 | 完了（C#原本・直接呼び出し監査済、Revisionのunchecked wrapを修正。buildはSection 12統合後） |
| M | +4/-1 | `Mods/Chat/ChatBox.cs` | .cpp,.hpp | 完了（C#全文・Renderer/Android/NetChat/NetSession/NetCheckClient直接呼び出し監査済、差分なし。buildはSection 12統合後） |

## 6. Input and gamepad — 52 ファイル (新規 44), C# +4665 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +350/-0 | `Mods/Input/PointerCheck.cs` | — 新規 | 完了（C#全文監査済、Scene fixture/finallyを修正） |
| A | +309/-0 | `Mods/Input/PadBindingState.cs` | — 新規 | 完了（C#全文監査済、bounds/revisionを修正） |
| A | +301/-0 | `Mods/Input/GamepadChecks.cs` | .cpp,.hpp | 完了（shell build で GamepadUiChecks を C# と同じ位置で実行） |
| A | +264/-0 | `Mods/Input/WindowsPenInput.cs` | — 新規 | 完了（C#全文監査済、Win32 fallback/exception parity修正） |
| M | +261/-34 | `Mods/Input/StylusZone.cs` | .cpp,.hpp | 完了（C#全文監査済、NaN時のMathMin/Maxを修正） |
| A | +258/-0 | `Mods/Input/MouseFlick.cs` | — 新規 | 完了（C#全文監査済、Firedのunchecked加算を修正） |
| A | +210/-0 | `Mods/Input/GamepadManager.cs` | — 新規 | 完了（C#全文監査済、revision wrap/event valueを修正） |
| A | +199/-0 | `Mods/Input/GamepadProfiles.cs` | — 新規 | 完了（C#全文監査済、UTF-16/JSON/revision parity修正） |
| A | +158/-0 | `Mods/Input/GamepadEnhancementChecks.cs` | — 新規 | 完了（C#全文監査済、チェック順・条件・例外を照合、Windows Release build済） |
| A | +141/-0 | `Mods/Input/WeaponWheel.cs` | — 新規 | 完了（C#全文・HUD呼出元監査済、drag/availability境界を照合、Windows Release build済） |
| A | +117/-0 | `Mods/Input/GamepadOptionState.cs` | — 新規 | 完了（C#全文監査済、defaults/Load/Write/Clone/Resetを照合、Windows Release build済） |
| A | +116/-0 | `Mods/Input/GamepadUiRouter.cs` | — 新規 | 完了（C#全文監査済、repeat timerのunchecked long演算を修正、Windows Release build済） |
| A | +115/-0 | `Mods/Input/AimAssist/AimAssistWorld.cs` | — 新規 | 完了（C#全文・PlayerEntityNetAim呼出元監査済、default target/tick取得順を修正、Windows Release build済） |
| A | +111/-0 | `Mods/Input/PointerDevice.cs` | — 新規 | 完了（C#全文・Renderer/PlayerInput呼出元監査済、device/contact/primary/delta parity確認） |
| A | +101/-0 | `Mods/Input/ControllerRuntimeChecks.cs` | — 新規 | 完了（C#全文監査済、event timeoutの非block性を修正、Windows Release build済） |
| M | +99/-11 | `Mods/Input/GamepadMappings.cs` | .cpp,.hpp | 完了（C#全文監査済、mapping/GUIDのUTF-16長とunchecked countを修正、Windows Release build済） |
| A | +87/-0 | `Mods/Input/AimAssist/AimAssistTelemetry.cs` | — 新規 | 完了（C#全文・Hit/Shot/Record呼出元監査済、unchecked集計を修正、Windows Release build済） |
| A | +85/-0 | `Mods/Input/GamepadMappingWizard.cs` | — 新規 | 完了（C#全文・Desktop/Setup/Checks呼出元監査済、nameのUnicode control/UTF-16切詰めを修正、Windows Release build済） |
| M | +82/-117 | `Mods/Input/GamepadInput.cs` | .cpp,.hpp | 完了（C#全文・desktop Renderer呼出順監査済、native差分なし。Android GameViewのBeginFrame呼出有無は別途確認対象） |
| M | +79/-274 | `Mods/Input/GamepadDesktop.cs` | .cpp,.hpp | 完了（C#全文・Renderer/UiSurface/GamepadProbe呼出元監査済、GLFW接続通知とgeneration wrapを修正、Windows Release build済） |
| A | +77/-0 | `Mods/Input/WindowsGamepadHaptics.cs` | — 新規 | 完了（C#全文・GamepadHaptics/GamepadDesktop呼出元監査済、欠落XInputSetState export時の例外を修正、Windows Release build済） |
| A | +76/-0 | `Mods/Input/AimAssist/AimAssistChecks.cs` | — 新規 | 完了（C#全文・GamepadChecksからの接続監査済、全assertionと順序が一致、Windows Release build済） |
| A | +76/-0 | `Mods/Input/GamepadPlatformChecks.cs` | — 新規 | 完了（C#全文・GamepadChecks二箇所の接続監査済、全34assertion一致、Windows Release build済） |
| A | +72/-0 | `Mods/Input/AimAssist/AimAssist.cs` | — 新規 | 完了（C#全文・AimAssistWorld直結呼出監査済、native同式を確認、Windows Release build済） |
| M | +71/-47 | `Mods/Input/GamepadLayout.cs` | .cpp,.hpp | 完了（C#全文・GamepadDesktop/GamepadMappings呼出元監査済、layout定義とraw readが一致、Windows Release build済） |
| A | +69/-0 | `Mods/Input/GamepadAnalog.cs` | — 新規 | 完了（C#全文・Input/Manager/Layout/Calibration/Haptics/Checks呼出元監査済、演算と状態合成が一致、Windows Release build済） |
| A | +62/-0 | `Mods/Input/GamepadCalibration.cs` | — 新規 | 完了（C#全文・GamepadSetupPanel/Manager/EnhancementChecks呼出元監査済、sample・percentile・apply条件一致、Windows Release build済） |
| A | +56/-0 | `Mods/Input/GamepadGlyphs.cs` | — 新規 | 完了（C#全文・Desktop/Manager/PadBinding/InputPrompt/UI呼出元監査済、family判定とglyph優先順一致、Windows Release build済） |
| A | +55/-0 | `Mods/Input/AimAssist/AimAssistDebug.cs` | — 新規 | 完了（C#全文・Renderer/AimAssistWorld/Telemetry/ModEntry呼出元監査済、診断行のTargetSlotをcurrent-culture書式に修正、Windows Release build済） |
| A | +54/-0 | `Mods/Input/PadAction.cs` | — 新規 | 完了（C#全文・GamepadProfiles/PadBindingState/GamepadActions呼出元監査済、23値とenum名/Parse/ToString一致、Windows Release build済） |
| A | +54/-0 | `Mods/Input/PlayerEntityMouseFlick.cs` | — 新規 | 完了（C#全文・PlayerInput.ProcessAlt呼出順とnative実装を照合済、main/bot/aim/boost/sequence gatesとReset/Check/出力一致、Windows Release build済） |
| A | +51/-0 | `Mods/Input/GamepadHaptics.cs` | — 新規 | 完了（C#全文・Desktop/Android bridge/Renderer/settings/context/player feedback呼出元監査済、null backend呼出時をNullReferenceExceptionに修正。Windows Release build済、Android ABI buildはAndroid全ファイル完了後） |
| A | +40/-0 | `Mods/Input/GamepadOptions.cs` | — 新規 | 完了（C#全propertyとState/RuntimeConfig委譲を照合済、WheelOrderを設定所有者付きで返しC#配列参照の寿命を保持、Windows Release build済。Android ABI buildはAndroid全ファイル完了後） |
| A | +39/-0 | `Mods/Input/GamepadActions.cs` | — 新規 | 完了（C#全文・GamepadInput/GamepadEnhancementChecks呼出元監査済、invalid PadActionのshift countをC#の6bit maskに修正、Windows Release build済） |
| A | +32/-0 | `Mods/Input/WeaponSelectionDirection.cs` | — 新規 | 完了（C#全文・GamepadInput/GamepadEnhancementChecks/GamepadChecks呼出元監査済、float→intを.NET 9+互換変換にしてNaN時のC++未定義動作を除去、Windows Release build済） |
| A | +30/-0 | `Mods/Input/PlayerEntityHaptics.cs` | — 新規 | 完了（C#全文・PlayerInput/PlayerSound/PlayerHud/PlayerEntity呼出元監査済、feedback gates・weapon-selection slot/null/available-array処理一致、Windows Release build済） |
| A | +27/-0 | `Mods/Input/AimInputSourceTracker.cs` | — 新規 | 完了（C#全文・AimAssistWorld/GamepadInput/GamepadUiRouter/AimAssistChecks/AimAssistTelemetry/AimAssistDebug呼出元監査済、pointer判定・stick deadzone・120ms takeover・reset/revision semantics一致、Windows Release build済） |
| A | +25/-0 | `Mods/Input/AimAssist/AimAssistTuning.cs` | — 新規 | 完了（C#全文・AimAssist/AimAssistWorld呼出元監査済、定数/enum/profile factoryの値と順序一致、readonly record semanticsとfloat NaN等値比較をnativeへ反映、Windows Release build済） |
| M | +25/-18 | `Mods/Input/GamepadProbe.cs` | .cpp,.hpp | 完了（C#全文・ModEntry/GamepadChecks/GamepadEnhancementChecks/GamepadMonitor呼出元監査済、probe表示/終了判定一致、seconds表示のdouble精度と.NET UTF-16幅揃えを修正。Windows native library compile/link済、実行中FruityPrime.exeが出力先をロック中のためexe再リンク未確認） |
| M | +25/-213 | `Mods/Input/PadBindings.cs` | .cpp,.hpp | 完了（C#全文・全API委譲とInputSettings/GamepadManager/GamepadInput/Launcher/Probe呼出元監査済、Current.Bindings選択・action順・各委譲一致。native差分なし） |
| A | +24/-0 | `Mods/Input/GamepadRuntimeConfig.cs` | — 新規 | 完了（C#全文・GamepadManager/GamepadInput/GamepadOptions/PadBindings/GamepadProfiles/ControllerRuntimeChecks呼出元監査済、default/clone/Fallback/Selected/ThreadStatic Frame lifetimeとCurrent publication一致。AtomicSharedPtrはvolatile reference相当として確認、native差分なし） |
| A | +24/-0 | `Mods/Input/SpectatorInput.cs` | — 新規 | 完了（C#全文・Renderer呼出元監査済、focus/context gate・press消費順・deadzone/buttons/trigger/camera値一致、readonly record semanticsとfloat NaN等値比較をnativeへ反映。Windows native library compile/link済、実行中FruityPrime.exeのロックでexe再リンク未確認） |
| A | +23/-0 | `Mods/Input/GamepadDeviceSnapshot.cs` | — 新規 | 完了（C#全文・GamepadManager/GamepadProbe/Profiles/Launcher UI/直接呼出元監査済、init-only相当のreadonly APIとrecord/GamepadState/VectorのNaN等値比較を反映。Windows Release native library build済） |
| A | +20/-0 | `Mods/Input/HapticScheduler.cs` | — 新規 | 完了（C#全文・GamepadHaptics直接呼出監査済、undefined feedbackの範囲外アクセスをIndexOutOfRangeExceptionへ修正、Windows Release build済） |
| A | +19/-0 | `Mods/Input/InputPrompt.cs` | — 新規 | 完了（C#原本と呼び出し元を監査済、UiAction fallback/secondary slot/glyph/ToString一致、readonly/default Label nullをnativeに反映） |
| A | +19/-0 | `Mods/Input/InputSourceTracker.cs` | — 新規 | 完了（C#全文・Renderer/GamepadManager/HUD呼び出し元監査済、180ms切替とResetを照合、unchecked long差分を修正） |
| A | +17/-0 | `Mods/Input/AimAssist/AimAssistMath.cs` | — 新規 | 完了（C#全文・AimAssist/AimAssistWorld呼び出し元監査済、Smooth/Finite/Opposition/ScoreとNaN/Inf挙動一致、差分なし） |
| A | +15/-0 | `Mods/Input/ControllerLayoutState.cs` | — 新規 | 完了（C#全文・GamepadRuntimeConfig/GamepadOptions/PadBindings呼び出し元監査済、Bindings/Name/Southpaw/Applyと更新順一致、差分なし） |
| A | +13/-0 | `Mods/Input/StickCalibration.cs` | — 新規 | 完了（C#全文・GamepadCalibration/OptionState/GamepadMonitor呼び出し元監査済、readonly/NaN等値/Math.Maxを修正、Windows Release build済） |
| A | +12/-0 | `Mods/Input/AimAssist/AimAssistState.cs` | — 新規 | 完了 |
| A | +10/-0 | `Mods/Input/AimAssist/AimAssistTarget.cs` | — 新規 | 完了 |
| M | +10/-77 | `Mods/Input/PointerInput.cs` | .cpp,.hpp | 完了 |

## 7. Render — 22 ファイル (新規 14), C# +2658 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +416/-0 | `Mods/Render/LauncherPhoto.cs` | .cpp,.hpp | 完了（C#原本監査済、Renderer呼び出しをSection 12で接続） |
| A | +277/-0 | `Mods/Render/PlayerEntityMapPick.cs` | — 新規 | 完了 |
| A | +256/-0 | `Mods/Render/MapThumbnail.cs` | — 新規 | 完了 |
| A | +244/-0 | `Mods/Render/UiOverlay.cs` | .cpp,.hpp | 完了（C#原本監査済、Renderer呼び出しをSection 12で接続） |
| A | +226/-0 | `Mods/Render/NoiseField.cs` | — 新規 | 完了 |
| A | +174/-0 | `Mods/Render/LauncherHunter.cs` | .cpp,.hpp | 完了（C#原本監査済、RenderWindow APIと描画呼出しを接続） |
| A | +160/-0 | `Mods/Render/LauncherNoise.cs` | .cpp,.hpp | 完了（C#原本監査済、GL unpack enum を追加） |
| M | +138/-3 | `Mods/Render/PreviewPass.cs` | .cpp,.hpp | 完了 |
| A | +132/-0 | `Mods/Render/AppIcon.cs` | .cpp,.hpp | 完了（C#原本監査済、GLFW Window::SetIcon 経由で接続） |
| A | +116/-0 | `Mods/Render/Radar.cs` | — 新規 | 完了 |
| A | +97/-0 | `Mods/Render/HunterShot.cs` | — 新規 | 完了 |
| M | +77/-0 | `Mods/Render/FrameTimingCheck.cs` | .cpp,.hpp | 完了 |
| A | +70/-0 | `Mods/Render/PlayerEntityTeamScoreboard.cs` | — 新規 | 完了 |
| A | +62/-0 | `Mods/Render/LockjawTrailProbe.cs` | — 新規 | 完了 |
| M | +51/-0 | `Mods/Render/PlayerEntityStylusHud.cs` | .cpp,.hpp | 完了 |
| A | +43/-0 | `Mods/Render/DesktopGlContext.cs` | .cpp,.hpp | 完了（C#原本監査済、Renderer経由で接続） |
| M | +31/-0 | `Mods/Render/GlEs.cs` | .cpp,.hpp | 完了 |
| A | +28/-0 | `Mods/Render/LockjawTrailNoise.cs` | — 新規 | 完了 |
| M | +26/-0 | `Mods/Render/HunterPreview.cs` | .cpp,.hpp | 完了 |
| M | +17/-1 | `Mods/Render/PlayerEntityEndScreen.cs` | .cpp,.hpp | 完了 |
| M | +12/-9 | `Mods/Render/PlayerEntityProHud.cs` | .cpp,.hpp | 完了 |
| M | +5/-0 | `Mods/Render/PlayerEntityVoteHud.cs` | .cpp,.hpp | 完了 |

## 8. Multiplayer and teams — 7 ファイル (新規 7), C# +503 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +118/-0 | `Mods/Multiplayer/ResourceAudit.cs` | — 新規 | 完了（C#全文監査済、Math.Min/Maxを修正） |
| A | +99/-0 | `Mods/Multiplayer/MapResourceRules.cs` | — 新規 | 完了（C#全文監査済、評価順/null例外を修正） |
| A | +95/-0 | `Mods/Multiplayer/TeamGameplayTest.cs` | — 新規 | 完了（C#全文監査済） |
| A | +89/-0 | `Mods/Multiplayer/GameStateTeams.cs` | — 新規 | 完了（C#全文監査済、span boundsを修正） |
| A | +46/-0 | `Mods/Multiplayer/TeamLayout.cs` | — 新規 | 完了（C#全文監査済、span bounds/unchecked積を修正） |
| A | +37/-0 | `Mods/Multiplayer/TeamVisuals.cs` | — 新規 | 完了（C#全文監査済、Windows Release build済） |
| A | +19/-0 | `Mods/Multiplayer/MatchWorldProfile.cs` | — 新規 | 完了（C#全文監査済、Windows Release build済） |

## 9. Network — 56 ファイル (新規 26), C# +12807 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +1952/-0 | `Mods/Network/NetHitClaims.cs` | — 新規 | 完了 |
| M | +934/-33 | `Mods/Network/NetProtocol.cs` | .cpp,.hpp | 完了 |
| M | +883/-78 | `Mods/Network/NetHitPrediction.cs` | .cpp,.hpp | 完了 |
| M | +704/-105 | `Mods/Network/DedicatedServer.cs` | .cpp,.hpp | 完了 |
| A | +609/-0 | `Mods/Network/HitRig.cs` | — 新規 | 完了 |
| A | +558/-0 | `Mods/Network/NetSmoothing.cs` | — 新規 | 完了 |
| A | +536/-0 | `Mods/Network/LocalServer.cs` | — 新規 | 完了 |
| A | +536/-0 | `Mods/Network/NetLobbyTest.cs` | — 新規 | 完了 |
| M | +489/-116 | `Mods/Network/NetSession.cs` | .cpp,.hpp | 完了 |
| M | +481/-22 | `Mods/Network/NetUnlagged.cs` | .cpp,.hpp | 完了 |
| M | +384/-12 | `Mods/Network/NetMaster.cs` | .cpp,.hpp | 完了 |
| M | +366/-389 | `Mods/Network/NetPlayerBridge.cs` | .cpp,.hpp | 完了 |
| M | +307/-47 | `Mods/Network/NetDamage.cs` | .cpp,.hpp | 完了 |
| A | +296/-0 | `Mods/Network/LobbyCommands.cs` | — 新規 | 完了 |
| A | +284/-0 | `Mods/Network/NetCombatCheck.cs` | — 新規 | 完了 |
| A | +264/-0 | `Mods/Network/HostPool.cs` | — 新規 | 完了 |
| A | +200/-0 | `Mods/Network/MapAuditTeams.cs` | — 新規 | 完了 |
| M | +198/-11 | `Mods/Network/PlayerEntityNetAim.cs` | .cpp,.hpp | 完了（C#全文・直接呼び出し元監査済） |
| A | +194/-0 | `Mods/Network/NetSessionLobby.cs` | — 新規 | 完了 |
| A | +180/-0 | `Mods/Network/NetPlayerLifecycle.cs` | — 新規 | 完了 |
| A | +180/-0 | `Mods/Network/SessionProtocol.cs` | — 新規 | 完了 |
| M | +160/-15 | `Mods/Network/NetHooks.cs` | .cpp,.hpp | 完了 |
| M | +150/-2 | `Mods/Network/NetCheckClient.cs` | .cpp,.hpp | 完了 |
| A | +138/-0 | `Mods/Network/SpireAltPoseCheck.cs` | — 新規 | 完了 |
| M | +122/-2 | `Mods/Network/NetTestScript.cs` | .cpp,.hpp | 完了 |
| A | +100/-0 | `Mods/Network/ContinuousWeaponPhase.cs` | — 新規 | 完了 |
| A | +98/-0 | `Mods/Network/NetHealthSync.cs` | — 新規 | 完了 |
| A | +95/-0 | `Mods/Network/FormReconciliation.cs` | — 新規 | 完了 |
| A | +91/-0 | `Mods/Network/NetShotDiagnostics.cs` | — 新規 | 完了 |
| A | +90/-0 | `Mods/Network/HealthSimulationTest.cs` | — 新規 | 完了 |
| M | +90/-35 | `Mods/Network/NetLaunch.cs` | .cpp,.hpp | 完了（静的監査済み、Section 12 後にビルド） |
| A | +82/-0 | `Mods/Network/NetLifecycleTracker.cs` | — 新規 | 完了 |
| M | +82/-12 | `Mods/Network/PlayerEntityNetHud.cs` | .cpp,.hpp | 完了（C#全文・HUD呼び出し元監査済） |
| M | +80/-2 | `Mods/Network/ServerSimCheck.cs` | .cpp,.hpp | 完了 |
| M | +77/-1 | `Mods/Network/NetLog.cs` | .cpp,.hpp | 完了 |
| A | +76/-0 | `Mods/Network/LobbyRules.cs` | — 新規 | 完了 |
| M | +75/-0 | `Mods/Network/MapRotation.cs` | .cpp,.hpp | 完了 |
| A | +70/-0 | `Mods/Network/NetFaultQueue.cs` | — 新規 | 完了 |
| A | +69/-0 | `Mods/Network/NetTimingDiagnostics.cs` | — 新規 | 完了 |
| M | +66/-2 | `Mods/Network/ServerSim.cs` | .cpp,.hpp | 完了 |
| A | +61/-0 | `Mods/Network/MatchDefinition.cs` | — 新規 | 完了 |
| A | +58/-0 | `Mods/Network/NetHealthSyncTest.cs` | — 新規 | 完了 |
| M | +41/-5 | `Mods/Network/MapAudit.cs` | .cpp,.hpp | 完了 |
| M | +39/-30 | `Mods/Network/NetLag.cs` | .cpp,.hpp | 完了 |
| M | +37/-3 | `Mods/Network/NetMatchSync.cs` | .cpp,.hpp | 完了 |
| A | +37/-0 | `Mods/Network/NetMatchTimeSync.cs` | — 新規 | 完了 |
| A | +31/-0 | `Mods/Network/NetHudHealth.cs` | — 新規 | 完了 |
| M | +27/-0 | `Mods/Network/NetFeatureCheck.cs` | .cpp,.hpp | 完了 |
| M | +23/-40 | `Mods/Network/NetTransport.cs` | .cpp,.hpp | 完了 |
| M | +21/-17 | `Mods/Network/NetRoomChange.cs` | .cpp,.hpp | 完了 |
| M | +21/-29 | `Mods/Network/NetSlotManager.cs` | .cpp,.hpp | 完了 |
| M | +21/-4 | `Mods/Network/NetStatus.cs` | .cpp,.hpp | 完了 |
| M | +20/-0 | `Mods/Network/NetDiagnostics.cs` | .cpp,.hpp | 完了 |
| M | +16/-1 | `Mods/Network/NetHostSession.cs` | .cpp,.hpp | 完了 |
| M | +5/-1 | `Mods/Network/DemoPlayback.cs` | .cpp,.hpp | 完了 |
| M | +3/-2 | `Mods/Network/MechanicsDump.cs` | .cpp,.hpp | 完了 |

## 10. MapGen — 11 ファイル (新規 3), C# +2034 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +609/-0 | `Mods/MapGen/MapCheck.cs` | — 新規 | 完了 |
| A | +430/-0 | `Mods/MapGen/CollisionObj.cs` | — 新規 | 完了 |
| A | +369/-0 | `Mods/MapGen/AltFormProbe.cs` | — 新規 | 完了 |
| M | +213/-20 | `Mods/MapGen/Q3Import.cs` | .cpp,.hpp | 完了 |
| M | +120/-0 | `Mods/MapGen/MapDefinition.cs` | .cpp,.hpp | 完了 |
| M | +113/-2 | `Mods/MapGen/MapReport.cs` | .cpp,.hpp | 完了 |
| M | +91/-8 | `Mods/MapGen/Q3Convert.cs` | .cpp,.hpp | 完了 |
| M | +48/-1 | `Mods/MapGen/MapPacker.cs` | .cpp,.hpp | 完了 |
| M | +19/-0 | `Mods/MapGen/BuiltMap.cs` | .cpp,.hpp | 完了 |
| M | +19/-0 | `Mods/MapGen/MapBundle.cs` | .cpp,.hpp | 完了 |
| M | +3/-2 | `Mods/MapGen/CustomRooms.cs` | .cpp,.hpp | 完了 |

## 11. Launcher portable — 7 ファイル (新規 2), C# +615 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +320/-0 | `Mods/Launcher/Portable/NativeFilePicker.cs` | — 新規 | 完了 |
| M | +98/-42 | `Mods/Launcher/Portable/MatchStart.cs` | .cpp,.hpp | 完了（静的監査済み、Section 12 後にビルド） |
| M | +87/-8 | `Mods/Launcher/Portable/LauncherPrefs.cs` | .cpp,.hpp | 完了 |
| A | +67/-0 | `Mods/Launcher/Portable/RomWhitelist.cs` | — 新規 | 完了 |
| M | +24/-7 | `Mods/Launcher/Portable/TextLauncher.cs` | .cpp,.hpp | 完了 |
| M | +15/-3 | `Mods/Launcher/Portable/GameFiles.cs` | .cpp,.hpp | 完了 |
| M | +4/-0 | `Mods/Launcher/Portable/LaunchPlan.cs` | .cpp,.hpp | 完了 |

## 12. Launcher GUI — 70 ファイル (新規 50), C# +22132 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +1941/-0 | `Mods/Launcher/Gui/PlayScreen.cs` | .cpp,.hpp | 完了（C#全メソッド再監査・rounding/focus差を修正、StartScreen/InGameMenu/UiCapture接続済。Windows Release build済（2026-09-27）） |
| A | +1529/-0 | `Mods/Launcher/Gui/UiDesigns.cs` | .cpp,.hpp | 完了（C# 原本監査済、`-uidesign`入口をModEntryへ接続） |
| A | +1126/-0 | `Mods/Launcher/Gui/CreateServerScreen.cs` | .cpp,.hpp | 完了（C#監査済、StartScreen・UiCapture接続済） |
| A | +1077/-0 | `Mods/Launcher/Gui/Shell.cs` | .cpp,.hpp | 完了（C# / PR #1原本監査済、`-shellshot`とSection 13 scene API接続済、Windows Release build済） |
| A | +1031/-0 | `Mods/Launcher/Gui/UiSurface.cs` | .cpp,.hpp | 完了（C#監査済） |
| A | +988/-0 | `Mods/Launcher/Gui/StartScreen.cs` | .cpp,.hpp | 完了（C#全文監査済、LobbyScreen・GuiLauncher・UiCapture接続済） |
| A | +921/-0 | `Mods/Launcher/Gui/LobbyScreen.cs` | .cpp,.hpp | 完了（C#全体監査済・verified no-op、StartScreen接続済。Windows Release build済（2026-09-27）） |
| A | +889/-0 | `Mods/Launcher/Gui/DeckTile.cs` | — 新規 | 完了（C#監査済） |
| A | +838/-0 | `Mods/Launcher/Gui/DeckButton.cs` | — 新規 | 完了（C#監査済） |
| A | +819/-0 | `Mods/Launcher/Gui/UiLayout.cs` | — 新規 | 完了（C#監査済） |
| A | +786/-0 | `Mods/Launcher/Gui/HunterStand.cs` | .cpp,.hpp | 完了（C#監査済、LauncherHunter状態・描画APIをSection 13で接続、Windows Release build済） |
| A | +665/-0 | `Mods/Launcher/Gui/UiBench.cs` | .cpp,.hpp | 完了（C#原本監査済、旧Window/GCはnative headlessでの測定差を明記、`-uibench`入口接続） |
| A | +574/-0 | `Mods/Launcher/Gui/UiList.cs` | — 新規 | 完了（C#監査済） |
| A | +535/-0 | `Mods/Launcher/Gui/UiTopLevel.cs` | — 新規 | 完了（C#監査済） |
| M | +493/-387 | `Mods/Launcher/Gui/SettingsView.cs` | .cpp,.hpp | 完了（C#監査済、HunterStand・StartScreen・InGameMenu・UiCapture接続済） |
| M | +491/-142 | `Mods/Launcher/Gui/ServerRow.cs` | .cpp,.hpp | 完了（C#監査済） |
| A | +466/-0 | `Mods/Launcher/Gui/DeckChip.cs` | — 新規 | 完了（C#監査済） |
| A | +409/-0 | `Mods/Launcher/Gui/Deck.cs` | — 新規 | 完了（C#監査済） |
| A | +339/-0 | `Mods/Launcher/Gui/SetupScreen.cs` | .cpp,.hpp | 完了（C#監査済、StartScreen・UiCapture接続済） |
| A | +309/-0 | `Mods/Launcher/Gui/EndPanelView.cs` | .cpp,.hpp | 完了（C#全体監査済、空ballotのempty note alignmentを修正。Shell/UiCapture接続済。Windows Release build済（2026-09-27）） |
| A | +270/-0 | `Mods/Launcher/Gui/MovingBackdrop.cs` | — 新規 | 完了（C#監査済） |
| M | +268/-64 | `Mods/Launcher/Gui/UiCapture.cs` | .cpp,.hpp | 完了（C# / PR #1差分監査済、`-uishot`入口をModEntryへ接続） |
| A | +239/-0 | `Mods/Launcher/Gui/GamepadUiChecks.cs` | .cpp,.hpp | 完了（35 assertion を C# と順序照合、GamepadChecks から shell build で接続） |
| A | +227/-0 | `Mods/Launcher/Gui/Flags.cs` | — 新規 | 完了（C#監査済） |
| A | +222/-0 | `Mods/Launcher/Gui/TapCheck.cs` | — 新規 | 完了 |
| M | +220/-26 | `Mods/Launcher/Gui/Rows.cs` | .cpp,.hpp | 完了（C#監査済） |
| A | +202/-0 | `Mods/Launcher/Gui/InGameMenu.cs` | .cpp,.hpp | 完了（C#全体・Shell直接接続監査済、verified no-op。Windows Release build済（2026-09-27）） |
| A | +197/-0 | `Mods/Launcher/Gui/UiWord.cs` | — 新規 | 完了（C#監査済） |
| A | +196/-0 | `Mods/Launcher/Gui/BakedBackdrop.cs` | — 新規 | 完了（C#監査済） |
| A | +195/-0 | `Mods/Launcher/Gui/DeckText.cs` | — 新規 | 完了（C#監査済） |
| A | +193/-0 | `Mods/Launcher/Gui/GamepadSettingsPanel.cs` | — 新規 | 完了（C#監査済） |
| A | +181/-0 | `Mods/Launcher/Gui/DeckSide.cs` | — 新規 | 完了（C#監査済） |
| M | +181/-41 | `Mods/Launcher/Gui/PadRow.cs` | .cpp,.hpp | 完了（C#監査済） |
| M | +178/-123 | `Mods/Launcher/Gui/PauseMenuView.cs` | .cpp,.hpp | 完了（C#監査済、StartScreen・InGameMenu・UiCapture接続済） |
| A | +166/-0 | `Mods/Launcher/Gui/Tap.cs` | — 新規 | 完了 |
| A | +165/-0 | `Mods/Launcher/Gui/UiMark.cs` | — 新規 | 完了（C#監査済） |
| A | +162/-0 | `Mods/Launcher/Gui/UiScaleHost.cs` | .cpp,.hpp | 完了（C#監査済） |
| A | +159/-0 | `Mods/Launcher/Gui/ServerBadge.cs` | — 新規 | 完了（C#原本・ServerRow/GeoCountry/Flags直接接続監査済、変更したIPAddress runtime APIも.NET 10と一致。Windows Release build済（2026-09-27）） |
| A | +158/-0 | `Mods/Launcher/Gui/DeckCard.cs` | — 新規 | 完了（C#監査済） |
| A | +155/-0 | `Mods/Launcher/Gui/DeckField.cs` | — 新規 | 完了（C#監査済） |
| M | +149/-21 | `Mods/Launcher/Gui/GuiTheme.cs` | .cpp,.hpp | 完了（C#監査済） |
| A | +147/-0 | `Mods/Launcher/Gui/DeckSheet.cs` | — 新規 | 完了（C#監査済） |
| A | +146/-0 | `Mods/Launcher/Gui/UiTabs.cs` | — 新規 | 完了（C#監査済） |
| A | +143/-0 | `Mods/Launcher/Gui/DeckWordmark.cs` | — 新規 | 完了（C#監査済） |
| A | +143/-0 | `Mods/Launcher/Gui/GeoCountry.cs` | — 新規 | 完了（C#監査済） |
| A | +142/-0 | `Mods/Launcher/Gui/MapCardPicker.cs` | .cpp,.hpp | 完了（C#監査済、LobbyScreenから接続済） |
| A | +126/-0 | `Mods/Launcher/Gui/GamepadSetupPanel.cs` | — 新規 | 完了（C#監査済） |
| M | +116/-10 | `Mods/Launcher/Gui/KeyRow.cs` | .cpp,.hpp | 完了（C#監査済） |
| A | +93/-0 | `Mods/Launcher/Gui/FocusNavigator.cs` | — 新規 | 完了（C#監査済） |
| A | +91/-0 | `Mods/Launcher/Gui/GamepadNavigation.cs` | — 新規 | 完了（C#監査済） |
| A | +89/-0 | `Mods/Launcher/Gui/ControllerKeyboard.cs` | — 新規 | 完了（C#監査済） |
| A | +88/-0 | `Mods/Launcher/Gui/MapShot.cs` | — 新規 | 完了（C#監査済） |
| A | +80/-0 | `Mods/Launcher/Gui/GamepadMonitor.cs` | — 新規 | 完了（C#監査済） |
| A | +78/-0 | `Mods/Launcher/Gui/ConfirmScreen.cs` | — 新規 | 完了（C#全文・StartScreen/UiCapture/GamepadUiChecks接続監査済、追加修正なし。Windows Release build済（2026-09-27）） |
| M | +70/-157 | `Mods/Launcher/Gui/GuiLauncher.cs` | .cpp,.hpp | 完了（C#監査・Launcher/PauseMenu・PlatformDiagnostics.Report接続・Windows Release build済） |
| M | +61/-10 | `Mods/Launcher/Gui/SliderRow.cs` | .cpp,.hpp | 完了（C#監査済） |
| A | +58/-0 | `Mods/Launcher/Gui/LobbyPlayerRow.cs` | — 新規 | 完了（C#監査済） |
| A | +51/-0 | `Mods/Launcher/Gui/GamepadProfilePanel.cs` | — 新規 | 完了（C#監査済） |
| A | +44/-0 | `Mods/Launcher/Gui/ControllerNav.cs` | — 新規 | 完了（C#監査済） |
| A | +37/-0 | `Mods/Launcher/Gui/GamepadGlyph.cs` | — 新規 | 完了（C#監査済） |
| D | +0/-151 | `Mods/Launcher/Gui/DemoPickerView.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| D | +0/-2235 | `Mods/Launcher/Gui/HomeView.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| D | +0/-41 | `Mods/Launcher/Gui/HomeWindow.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| D | +0/-280 | `Mods/Launcher/Gui/MapPickerView.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| D | +0/-317 | `Mods/Launcher/Gui/MenuEntry.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| D | +0/-372 | `Mods/Launcher/Gui/PauseMenuWindow.cs` | .cpp,.hpp | 完了（C#削除を確認、旧PauseMenu参照も除去） |
| D | +0/-78 | `Mods/Launcher/Gui/SettingsWindow.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| D | +0/-239 | `Mods/Launcher/Gui/SplashView.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| D | +0/-157 | `Mods/Launcher/Gui/UpdateBadge.cs` | .cpp,.hpp | 完了（C#削除・native参照なしを確認） |
| M | +20/-62 | `Mods/PauseMenu.cs` | .cpp,.hpp | 完了（C#全体・Renderer/Shell直接接続監査済、catch範囲を修正。Windows Release build済（2026-09-27）） |

## 13. Engine and entities — 34 ファイル (新規 0), C# +3279 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| M | +1050/-99 | `Renderer.cs` | .cpp,.hpp | 完了（C#全文・直接呼出元再照合済、Scene.ShowCursorのWeaponWheel.Absolute条件漏れを修正してマウスホイール中のcursor captureを一致。Windows Release native library build済、runtime未実施） |
| M | +872/-49 | `Mods/ModEntry.cs` | .cpp,.hpp | 完了（C#全分岐・引数照合、`teamprobe`/network/MapGen/server/launcher配線、Windows Release build済） |
| M | +376/-94 | `Entities/Players/PlayerHud.cs` | .cpp,.hpp | 完了 |
| M | +185/-40 | `Entities/Players/PlayerInput.cs` | .cpp,.hpp | 完了 |
| M | +102/-126 | `GameState.cs` | .cpp,.hpp | 完了（C# PR #1差分監査・PlayPickedMap接続・Windows Release build済、runtime未実施） |
| M | +95/-11 | `Entities/Players/PlayerAi.cs` | .cpp,.hpp | 完了（C# PR #1差分監査・Insane AI移植・Windows Release build済、runtime未実施） |
| M | +71/-3 | `Formats/Formats.cs` | .cpp,.hpp | 完了（C# PR #1差分監査・Paths/Span対応・Windows Release build済） |
| M | +66/-33 | `Entities/BeamProjectileEntity.cs` | .cpp,.hpp | 完了 |
| M | +54/-0 | `Shaders.cs` | .cpp,.hpp | 完了（LauncherPhoto依存の2 shaderをC#と完全一致照合） |
| M | +48/-6 | `Entities/Players/PlayerEntity.cs` | .cpp,.hpp | 完了（C# PR #1差分監査済、Windows Release build済） |
| M | +45/-12 | `Read.cs` | .cpp,.hpp | 完了（C# PR #1差分監査・memory archive展開/診断・Windows Release build済、runtime未実施） |
| M | +40/-6 | `Entities/Players/PlayerProcess.cs` | .cpp,.hpp | 完了 |
| M | +39/-1 | `Entities/ItemSpawnEntity.cs` | .cpp,.hpp | 完了 |
| M | +37/-8 | `Entities/BombEntity.cs` | .cpp,.hpp | 完了 |
| M | +36/-2 | `Program.cs` | .cpp,.hpp | 完了 |
| M | +36/-2 | `Utility/Console.cs` | .cpp,.hpp | 完了 |
| M | +30/-1 | `Entities/Players/PlayerCollision.cs` | .cpp,.hpp | 完了 |
| M | +26/-20 | `Entities/NodeDefenseEntity.cs` | .cpp,.hpp | 完了 |
| M | +19/-1 | `Features.cs` | .cpp,.hpp | 完了（C#差分監査・Radar設定のLoad/Commit・Windows Release build済） |
| M | +11/-3 | `SceneSetup.cs` | .cpp,.hpp | 完了（C#差分監査・resource profile/health reset接続・Windows Release build済） |
| M | +6/-2 | `Mods/Credits.cs` | .cpp,.hpp | 完了 |
| M | +6/-1 | `Mods/DebugLog.cs` | .cpp,.hpp | 完了 |
| M | +6/-2 | `Utility/Archive.cs` | .cpp,.hpp | 完了（C#差分監査・byte span Extract overload・Windows Release build済） |
| M | +5/-3 | `Metadata/Metadata.cs` | .cpp,.hpp | 完了（C#差分監査・entity layer/4チーム色・Windows Release build済） |
| M | +4/-3 | `Utility/Extract.cs` | .cpp,.hpp | 完了（C#差分監査・相対root/interactive pause・Windows Release build済） |
| M | +3/-1 | `Sound/Sfx.cs` | .cpp,.hpp | 完了（C#差分監査・PlatformDiagnostics.Report接続・Windows Release build済） |
| M | +2/-2 | `Entities/Enemies/18_AlimbicTurret.cs` | .cpp,.hpp | 完了 |
| M | +2/-2 | `Entities/ItemInstanceEntity.cs` | .cpp,.hpp | 完了 |
| M | +2/-1 | `Entities/Players/HalfturretEntity.cs` | .cpp,.hpp | 完了 |
| M | +1/-9 | `Entities/Players/PlayerDraw.cs` | .cpp,.hpp | 完了 |
| M | +1/-0 | `Entities/Players/PlayerSound.cs` | .cpp,.hpp | 完了（C#差分監査済、着地feedbackの条件・種別・SFX順が一致） |
| M | +1/-0 | `Menu.cs` | .cpp,.hpp | 完了（C#差分監査・FieldOfView既定値の既反映を確認） |
| M | +1/-1 | `Scene.cs` | .cpp,.hpp | 完了（C#差分監査・FlagBase iterator修正・Windows Release build済） |
| M | +1/-0 | `Sound/Music.cs` | .cpp,.hpp | 完了（C#差分監査・PlatformDiagnostics.Report接続・Windows Release build済） |

## 14. Android head — 17 ファイル (新規 7), C# +1735 行

| S | +/- | C# | C++ | 進捗 |
|---|---|---|---|---|
| A | +299/-0 | `(android) AndroidHunterShot.cs` | `.cpp,.hpp` | 完了（C# / PR #1監査済。install・MainActivity Retire配線。arm64-v8a / x86_64 build済） |
| A | +271/-0 | `(android) AndroidUiSurface.cs` | `.cpp,.hpp` | 完了（C#原本監査済。dispatcher・MainActivity UI tick・GameView描画・TouchOverlayView入力を接続。両ABI build済） |
| A | +257/-0 | `(android) AndroidUiOverlay.cs` | `.cpp,.hpp` | 完了（C#原本監査済。GameView render/preview配線済、両ABI build済） |
| M | +246/-131 | `(android) MainActivity.cs` | .cpp,.hpp | 完了（C#原本監査済。preview停止、hunter retire、end-panel/lobby保持、StartScreen lifecycle参照、UI tickとTouchOverlayView touch routing接続済。両ABI build済） |
| A | +167/-0 | `(android) MainApplication.cs` | `MainActivity.cpp` builder seam | 完了（C#原本監査済。WebLink/HunterShot登録、Application Context、root選択を反映。両ABI build済） |
| M | +112/-2 | `(android) GameView.cs` | .cpp,.hpp | 完了（C#原本監査済。session/load/ui合成/chat cancel差分を修正。MainActivity UI tick/touch配線済。両ABI build済） |
| M | +98/-127 | `(android) GamepadBridge.cs` | .cpp,.hpp | 完了（C#原本監査・MainActivity listener/lifecycle配線済。両ABI build済） |
| M | +55/-22 | `(android) AndroidThumbnails.cs` | .cpp,.hpp | 完了（C#原本監査済。Activity遅延参照、worker上限と60秒進捗watchを修正。MainApplication登録・両ABI build済） |
| M | +54/-0 | `(android) TouchOverlayView.cs` | `.cpp,.hpp` | 完了（C#原本/PR #1監査済。end-panel描画とpointer 0のUI touch routingを修正。両ABI build済） |
| A | +52/-0 | `(android) AndroidWebLink.cs` | `.cpp,.hpp` | 完了（C# / PR #1監査済。MainApplication登録、両ABI build済） |
| M | +47/-9 | `(android) AndroidApp.cs` | .cpp,.hpp | 完了（C#原本監査済。Activity/SingleView lifetime、UiScaleHost、crash handler、Done/MatchRequestedを接続。両ABI build済） |
| A | +39/-0 | `(android) AndroidGamepadHaptics.cs` | .cpp,.hpp | 完了（C#原本監査・GamepadBridge登録接続済。両ABI build済） |
| A | +22/-0 | `(android) AndroidGamepadProfile.cs` | .cpp,.hpp | 完了（C#原本監査済、GamepadBridgeへaxis選択を接続、両ABI build済） |
| M | +11/-14 | `(android) AndroidUpdateInstaller.cs` | `.cpp,.hpp` | 完了（C#原本監査済。Activityを保持せず各操作時に `MainActivity.Instance` を再取得。両ABI build済） |
| M | +2/-3 | `(android) AndroidLogShare.cs` | .cpp,.hpp | 完了（C#原本監査済。cache cleanup・FileProvider chooser・exception報告を照合、修正なし。両ABI build済） |
| M | +2/-1 | `(android) AndroidMatch.cs` | .cpp,.hpp | 完了（C#原本監査済。cheat無効化・bot skill上限差を修正。両ABI build済） |
| M | +1/-0 | `(android) TouchControls.cs` | .cpp,.hpp | 完了（C#原本監査済。TakeAimDeltaのTouch入力源通知を追加。両ABI build済） |

### 2026-09-27 checkpoint gates

- Section 12 launcher GUI は commit `12a7433b`、Section 2 diagnostics は `a535e260` で `develop2` にpush済み。
- Section 13 の34ファイルとSection 12から延期したRenderer/side-scene呼び出しはC#原本との監査・配線を完了。
  Windows Release buildで全native targetをcompile/link済み。画面runtimeは未実施。
- Androidは17ファイルすべてのC#監査・MainActivity/AppBuilder/Renderer呼び出し配線後にarm64-v8aとx86_64をbuildした。
  両ABIともstatic library link成功（`build/native-android-arm64-v8a/build-retry10.log`、
  `build/native-android-x86_64/build-retry10.log`）。端末・実機runtimeは未実施。
- この時点でAndroid側のソース更新は両ABI buildの後にない。Windows/macOSの画面runtimeとmacOS native buildは未検証。

### 2026-09-27 complete-only audit

- `Mods/Multiplayer/MatchWorldProfile.cs` をC#全文とnative `.cpp/.hpp` で照合。enum byte値、default struct値、
  `IsValid` のplayer/resource組合せとEnum.IsDefined相当、configured player数の2〜8 clamp、entity layer上限4、
  Low/Standard/Highの境界を確認した。差分修正なし。
- `Mods/Multiplayer/TeamVisuals.cs` をC#全文とnative `.cpp/.hpp` で照合。4 teamとneutralのlabel/RGBA/radar color/
  model team/recolor、負値・範囲外indexのneutral fallback、Apply時のteam設定と既存recolor保持/解除条件が一致。
  nativeの返却参照はstatic const tableへ限られ、呼び出し側はC# record structの値と同じ観測値。差分修正なし。
- `Mods/Multiplayer/TeamLayout.cs` をC#全文とnative `.cpp/.hpp` および`NetLobbyTest`/serverの直接呼び出しで照合。
  layout field/default、capacity、validation、ToString、allies、normalized occupancy/tie-break順は一致。
  span長不足時のC#例外をnativeでも`ManagedAt`で再現し、C#通常unchecked積を`UncheckedMultiply`へ変更した。
- `Mods/Multiplayer/TeamGameplayTest.cs` をC#全文とnative `.cpp/.hpp` で比較。初期化/全assertionの順、team standingsとtie、
  Survival勝者・FFA、PlayerEntity全slotの準備とfinally時の全state復元が一致。nativeのcatch/rethrowはfinally cleanup相当。
  差分修正なし。
- `Mods/Multiplayer/GameStateTeams.cs` をC#全文とnative `.cpp/.hpp` で照合。tie判定、active/team範囲filter、team/member sortの
  tie-break、represented teamからの順位計算、FFA rankと空配列初期化が一致。C# `stackalloc bool[4]` はinitializerなしで内容が
  未規定のため、nativeは未初期化読取を避けてゼロ初期化を維持。`represented`の両アクセスを`ManagedAt`にし範囲外例外も対応。
- `Mods/Multiplayer/MapResourceRules.cs` をC#全文とnative `.cpp/.hpp` で照合。27部屋の候補IDと順序、Health判定、profile別の
  Transfer Lock例外、元list複製、既存ID/親/Enabled/重複距離の条件、source順追加、spawn intervalの300上限と72-byte書換を確認。
  RoomMetadataの早期条件後に辞書lookupするC#の評価順へnativeを変更。適用対象でnullのoriginalはC# `List<Entity>` と同じ
  `ArgumentNullException("collection")` にし、未定義動作を除去。差分修正以外はなし。
- `Mods/Multiplayer/ResourceAudit.cs` をC#全文とnative `.cpp/.hpp` で照合。18シナリオのlabel/mode/player数/順、room/layer走査、healthと
  objective集計、重複・安定fingerprint検査、列出力と最終summaryが一致。距離集計のC# `Math.Min/Max` はNaNを伝播させるため、
  nativeの`std::min/max`を既存のC#互換`MathMin/MathMax`へ変更。差分以外の修正なし。
- `Mods/Input/PointerCheck.cs` をC#全文とnative `.cpp/.hpp` で照合。binding/movement/zone/player-input/settings/Win32 signatureの
  検査順とassertion内容は一致。C#が`RuntimeHelpers.GetUninitializedObject`で作るSceneに合わせ、通常Scene ctorのcache/GameState/Music初期化を
  行わないprivate test fixture ctorを追加し、Movie indexはC#の`-1`状態を保った。`Run`のcatch-all error formattingを`ExceptionToString`へ、
  PointerDevice cleanupをRAIIへ変更してC# catch/finally相当を合わせた。nativeの例外文字列はtype/messageを出し、CLR stack traceは持たない。
- `Mods/Input/PadBindingState.cs` をC#全文とnative `.cpp/.hpp` および直接呼び出しで照合。23個のdefault bindingsと23個のActionOrder、
  Set/SetSlot/LoadSlots、modifier chord、Evaluate/ChordButtons、conflict resolution、preset/clone/reset、text/setting keyの順序と値が一致。
  invalid action/slotのnative例外をC#配列と同じ`IndexOutOfRangeException`にし、SetSlotの引数検査順を修正。`Revision++`をC#のunchecked
  `long` wrapと同じ`IncrementInPlace`に変更。直接呼び出しのslotは0または1。
- `Mods/Input/WindowsPenInput.cs` をC#全文とnative `.cpp/.hpp`、`Renderer.cs`/`Renderer.cpp`のAttach/Read呼び出しで照合。
  Win32 message分岐、promoted mouse判定、接触/hover/release状態、座標scale、pen pressure/tilt、GLFW fallbackの内容と順序が一致。
  C# P/Invokeで欠落APIが例外になる箇所をnativeのnull関数ポインター呼出しにせず、`DllNotFoundException`/
  `EntryPointNotFoundException`相当のログ/fallbackへ変更。window callbackのcatchをcatch-allにし、現在例外のMessageを記録する。
  差分修正以外はなし。実機ペン入力runtimeは未実施。
- `Mods/Input/StylusZone.cs` をC#全文とnative `.cpp/.hpp`、renderer/settings/player-input/HUDの使用箇所で照合。
  状態遷移、ボタン定義と順序、配置ドラッグ、nudge/resize、capture/aim条件、遷移ログが一致。
  `SetRect`/`PlacementDrag`/`Nudge`の上限計算とドラッグ始点計算をC# `Math.Max/Min`同様のNaN伝播をする既存helperへ変更。
  差分修正以外はなし。実機タブレット入力runtimeは未実施。
- `Mods/Input/MouseFlick.cs` と対応する`PlayerEntityMouseFlick.cs`呼び出しをC#全文/native `.cpp/.hpp`で照合。
  sample ring、frame gap reset、rest arm、backward coherent burst、sensitivity閾値、重み付き方向、cooldownとlog値、およびmain-player/bot/input gatesと出力代入が一致。
  `Fired++`をC#既定unchecked時のwrapと同じnative `IncrementInPlace`に変更。差分修正以外はなし。
- `Mods/Input/GamepadManager.cs` をC#全文とnative `.cpp/.hpp`、状態・通知の直接使用箇所で照合。lock範囲とevent順、device追加/削除、
  selection fallback、active切替、profile publish、raw/calibrated state、trigger hysteresis、activity検出、snapshot/device revisionの更新順が一致。
  C# `long` revisionsの加算をunchecked wrapに合わせ、`Action<GamepadDeviceSnapshot>`相当の追加/削除eventを値渡しに変更。
  差分修正以外はなし。実機コントローラーruntimeは未実施。
- `Mods/Input/GamepadProfiles.cs` をC#全文とnative `.cpp/.hpp`、settings/profile UI・managerの直接使用箇所で照合。
  file size/count制限、profile validation、runtime構築、適用/保存/読込/import/export、assign/unassign、device key、atomic writeの順序を確認。
  .NET `string.Length` と `char.IsControl`に合わせ、name/line/keyをUTF-16単位で検査し、C1制御文字も拒否、import名の40-unit切詰めを修正。
  JSON `Version`はInt32範囲・整数表現で読み、revisionをunchecked wrapにした。差分修正以外はなし。
- `Mods/Input/GamepadEnhancementChecks.cs` をC#全文とnative `.cpp/.hpp`で照合。assertionの順序・条件・対象・メッセージ、synthetic calibration/mapping、実際のPlayerControls keybind、profile import/assign/cleanupの流れが一致。
  `GamepadProbe.Actions`は入力bindingを読むだけの処理であり、C#の2回評価とnativeの1回キャッシュによる結果差はない。native差分修正なし。Windows Release build済、check harness自体は未実行。
- `Mods/Input/WeaponWheel.cs` をC#全文とnative `.cpp/.hpp`、HUDの直接呼出元で照合。absolute-device判定、drag開始/close時の初期化、step既定値、累積移動と離散step、未所持武器のskip、端での停止、範囲外availabilityの拒否が一致。
  呼出元のcurrent slotは`-1..5`、availabilityは6要素配列で、native spanの受け渡しも対応。native差分修正なし。Windows Release build済。
- `Mods/Input/GamepadOptionState.cs` をC#全文とnative `.cpp/.hpp`で照合。field defaults、重複keyの後勝ち、culture/invariant parseの使い分け、finite/range fallback、legacy keys、enum/wheel-order validation、全fieldのWrite順、Clone/Resetを確認。
  native差分修正なし。Windows Release build済。
- `Mods/Input/GamepadUiRouter.cs` をC#全文とnative `.cpp/.hpp`で照合。context flags/revision、押下edge、analog trigger hysteresis、context切替・未接続時のneutral barrier、direction優先順とrepeat cadence、Accept/Back/tab/page action順を確認。
  repeat開始/次回時刻の加算と長押し時間差をC#既定unchecked `long`演算に合わせ、native signed overflowの未定義動作を解消。Windows Release build済。
- `Mods/Input/AimAssist/AimAssistWorld.cs` をC#全文とnative `.cpp/.hpp`、`PlayerEntityNetAim`直接呼出元で照合。state reset条件、eligibility/observation、武器profile、対象slot順/絞込/LOS、body/head geometry、assist・debug・telemetry出力を確認。
  C#の`default(AimAssistTarget)`は全field zeroであるため、nativeのmember defaultsによる`Eligible=true`/`UpperChest`を明示的なzero stateへ修正。PointerとStickに渡すtick countもC#同様に個別取得。Windows Release build済。
- `Mods/Input/PointerDevice.cs` をC#全文とnative `.cpp/.hpp`、Renderer/PlayerInputの直接呼出元で照合。active/accepting遷移、device identity/contact切替、aspect/座標正規化、StylusZone通知、primary/capture判定、pointer delta蓄積/消費とmouse fallback、Mouse Left binding edgesが一致。
  `PointerSample` defaultsとCurrent/PrimaryDownの利用方法も確認。native差分修正なし。Windows Release build済。
- `Mods/Input/ControllerRuntimeChecks.cs` をC#全文とnative `.cpp/.hpp`で照合。device-specific calibration/runtime切替、frame snapshot隔離、manager event再入、profile validation/library保護、haptic arbitration、mapping置換、UI trigger hysteresis、layout identity/promptのassertion順と条件が一致。
  event再入確認をC#のtimeout後も戻る`Task.Wait(1000)`に合わせ、timeout時にfuture破棄でblockする`std::async`をdetached packaged taskへ変更。Windows Release build済、check harness自体は未実行。
- `Mods/Input/GamepadMappings.cs` をC#全文とnative `.cpp/.hpp`で照合。resource/settings/environmentの読込優先順、override置換、platform filter、GLFWへの一括適用、capability解析、summaryとsuggestion出力を確認。
  C# `string.Length` とnative UTF-8 byte長の差が出るmapping上限/GUID判定を既存`Utf16Length`へ合わせ、C# unchecked `int` のfiles/lines集計をwrap演算へ変更。差分修正以外はなし。Windows Release build済。
- `Mods/Input/AimAssist/AimAssistTelemetry.cs` をC#全文とnative `.cpp/.hpp`、`AimAssistWorld`/`PlayerEntityHaptics`/`PlayerEntity`/`ModEntry`の呼出元で照合。opt-in・authority/player/spectator filter、weapon/input/range bucket、shot/hit対応、全統計値、process-exit保存、JSON項目と配列順を確認。
  `Shots`/`HitEvents`/`Samples`/`TargetSamples`/`Switches`と`ObservedDamage`の加算をC# unchecked wrapに合わせ、native signed overflowを解消。Windows Release build済。
- `Mods/Input/GamepadMappingWizard.cs` をC#全文とnative `.cpp/.hpp`、`GamepadDesktop`/`GamepadSetupPanel`/`GamepadEnhancementChecks`の呼出元で照合。20-step順、device/shape検証、release-to-rest、button/hat/axisの検出優先と閾値、重複排除、GUID/platformとmapping形式を確認。
  C# `char.IsControl`/`Take(100)`がUTF-16 code unit単位である点に合わせ、Unicode control除外とname切詰めをUTF-16経由へ修正。Windows Release build済。
- `Mods/Input/GamepadInput.cs` をC#全文とnative `.cpp/.hpp`、desktopの`Renderer`入出力呼出順で照合。snapshot/runtime反映、edge/reset/block処理、focus/disconnect、aim、press消費、移動・全binding合成、weapon wheel/last-weapon、keybind統合順が一致し、native差分修正なし。
  Android側は`GameView`から`TakePress`/`TakeMenuPress`を呼ぶ一方、`src/MphRead.Android`内に`GamepadInput.BeginFrame`呼出しが見つからなかったため、Android統合の別監査項目として記録。Android buildは実行していない。
- `Mods/Input/GamepadDesktop.cs` をC#全文とnative `.cpp/.hpp`、`Renderer`/`UiSurface`/`GamepadProbe`呼出元で照合。GLFW slot走査、mapped/raw read、軸変換、button indices、capability/name/family、wizard snapshot、haptics同期が一致。
  C# `OnJoystickConnected`がconnect/disconnect両通知で`DeviceChanged`を呼ぶ点に対しnativeの通知配線がなかったため、対象のGLFW callback APIだけを追加して同じcleanupを接続。slot generationの加算もC# unchecked wrapに合わせた。Windows Release build済。
- `Mods/Input/WindowsGamepadHaptics.cs` をC#全文とnative `.cpp/.hpp`、`GamepadHaptics`/`GamepadDesktop`の呼出元で照合。XInput struct layout、接続indexの一意判定、登録・解除・dispose順、finite振幅、1〜500 msのoneshot停止とtimer競合時の順序を確認。
  `XInputGetState`のみ存在して`XInputSetState`が欠落する環境でC# P/Invokeは例外になるがnativeが黙って振動を捨てていた差を、`System::EntryPointNotFoundException`で合わせた。Windows Release build済。
- `Mods/Input/AimAssist/AimAssistChecks.cs` をC#全文とnative `.cpp/.hpp`、`GamepadChecks`からの接続で照合。23個の判定、状態の準備・変更順、30/120 Hz比較、入力ソース切替の時刻と期待値が一致し、差分修正なし。C#のGC割当計測はmanaged heap専用のためnativeでは実行できないが、`AimAssist::Apply`と呼出先を見てspan/value演算だけで割当経路がないことを確認した。Windows Release build済。
- `Mods/Input/GamepadPlatformChecks.cs` をC#全文とnative `.cpp/.hpp`、`GamepadChecks.Run`内の二箇所の接続順で照合。macOS Xbox Bluetooth fixtureの軸/トリガー/10物理button/diagonal hat、mapping許可・拒否、Linux/generic fallback、4 preset、secondary slotを含むconflict swapの全34 assertionとreset順が一致。修正なし。Windows Release build済、check harness未実行。
- `Mods/Input/AimAssist/AimAssist.cs` をC#全文とnative `.cpp/.hpp`、`PlayerEntity::ApplyControllerAssist`/`AimAssistWorld`の直接呼出元で照合。invalid入力/eligibility時のreset、intent閾値、target走査順・retain/challenger hysteresis、角速度補償、head delay/blend、距離/inner cone friction、opposition、rotation clamp、result scoreまで同式・同順序で、差分修正なし。Windows Release build済。
- `Mods/Input/GamepadLayout.cs` をC#全文とnative `.cpp/.hpp`、`GamepadDesktop` raw-readおよび`GamepadMappings`の選択/compatibility呼出元で照合。Xbox/flat/macOS Bluetooth各index、GUID/形状条件、axis-capability、finite軸/trigger floor、Y反転、button・hat bit mappingと境界処理が一致。修正なし。Windows Release build済。
- `Mods/Input/GamepadAnalog.cs` をC#全文とnative `.cpp/.hpp`、`GamepadInput`/Manager/Layout/Calibration/Haptics/Checks呼出元で照合。finite clamp、radial deadzone、4 response curve、trigger hysteresis、8方向quantize、curve enum parse/format、key＋motion button合成が一致。quantizeのnearbyintはC#のties-to-evenと同じ既定rounding modeで、repoにmode変更がないことも確認。修正なし。Windows Release build済、gamepadcheck未実行。
- `Mods/Input/GamepadCalibration.cs` をC#全文とnative `.cpp/.hpp`、`GamepadSetupPanel`のRawState採取・`GamepadManager`のtrigger変換・`GamepadEnhancementChecks`の接続で照合。2048件上限、dirty cache、rest/range各10件条件、NaN先頭のfloat percentile、左右stick range/center判定、deadzone、trigger min/maxの0.4幅条件、Apply順、Summaryのcurrent-culture書式が一致。修正なし。Windows Release build済、check harness未実行。
- `Mods/Input/GamepadGlyphs.cs` をC#全文とnative `.cpp/.hpp`、`GamepadDesktop`/`GamepadManager`/`PadBindingState`/`InputPrompt`/launcher glyph viewの呼出元で照合。vendor ID優先順、GUID offset、name token順、設定family→device family→genericの選択、PlayStation/Nintendo remap、fallback labelsとflags `ToString`が一致。修正なし。Windows Release build済。
- `Mods/Input/InputPrompt.cs` をC#全文とnative `.cpp/.hpp`、`GamepadUiRouter::ToString`/`PadBindings`およびStartScreen/ControllerRuntimeChecksの呼出元で照合。UiActionのbutton mappingと未知値の数値Label、PadActionのprimary未割当時のsecondary選択、modifier/glyph/ToStringが一致。C# `readonly record struct` に対してnativeが公開可変fieldだった点と、default structの`Label == null`をprivate getter/optionalへ修正し、文字列連結時はC#同様空文字として扱う。
- `Mods/Input/InputSourceTracker.cs` をC#全文とnative `.cpp/.hpp`、Rendererのmouse/key入力・GamepadManagerのactivity通知・menu/HUDの読み取り元で照合。初期値、Reset、180msの切替抑制、同一source時の早期returnと通知位置が一致。C#既定uncheckedの`milliseconds - _changed`をnativeで直接計算していたため、境界値での符号付きoverflow未定義動作を`UncheckedSubtract`へ置換した。
- `Mods/Input/AimAssist/AimAssistMath.cs` をC#全文とnative inline `.hpp`、`AimAssist`/`AimAssistWorld`の呼び出し元で照合。Smoothの割算・clamp・式順、Vector2 finite判定、Oppositionの符号判定、Scoreの係数とclamp順が一致。NaN/InfもC# `Math.Clamp`とnative `std::clamp`で伝播・飽和が一致し、差分修正なし。
- `Mods/Input/ControllerLayoutState.cs` をC#全文とnative `.cpp/.hpp`、GamepadRuntimeConfigの生成・GamepadOptions/PadBindingsの委譲・ControllerRuntimeChecksの参照で照合。Bindingsの共有参照、Preset名、Southpaw setterのoptions→Custom preset順、Apply時のpreset→Southpaw更新と`Custom`保持条件が一致。差分修正なし。
- `Mods/Input/StickCalibration.cs` をC#全文とnative inline `.hpp`、GamepadCalibration/OptionState/GamepadMonitor/EnhancementChecksの直接使用箇所で照合。6値の順序/default、readonly record、Normalizeの方向ごとの分母/clamp、等値とNaNを確認。nativeの公開可変fieldと既定float比較はC# recordと異なるためprivate getter化・NaN同士を等値化し、`std::max`ではNaNを捨てるため正規化をC# `Math.Max`互換helperへ変更した。全callerを読み取りproperty相当に更新。Windows Release build成功。

### 2026-09-27 native launcher regression audit

- `NativeRuntime/System/Tasks.cpp` の `TaskRun` を `Tasks.hpp` とC# `Task.Run`、全native直接呼出元（特に `NetMasterClient::FindHosts` のサーバー行ごとの並列問い合わせ）で照合。C#は共有ThreadPoolへ投入するがnativeは呼出しごとにOSスレッドをdetachしていた。hardware concurrencyまで遅延拡張する共有キューに変更し、行数に比例するスレッド・スタック生成を止めた。Windows Releaseのnative static library compile/link成功。ネットワーク実サーバーでの応答確認は未実施。
- Launcher CPU/memory診断: `-uibench maps -uibenchsize 1280x720 -uibenchonly Paint` はrender 97.09 ms、約10 fps。併せたプロセスメモリ採取はprivate bytes最大57.5 MB、working set最大65.4 MB、5 threadsで、この測定内に増え続けるメモリは観測しなかった。1920x1080ではrender 229.08 ms、約4 fps。従ってOffline画面の再描画はCPU描画の重さが直接確認できたが、この診断だけではユーザーPC全体のフリーズ原因や1080pのメモリ状態は確定しない。UI描画の修正とOnline画面の実サーバー確認は継続。
