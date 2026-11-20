# TapeLofi — 汎用テープ・モジュレーション／ローファイ・エフェクト

解析仕様書（`P002_tape_lofi_engine_spec.md` v0.7）をもとに新規実装した、テープ／ローファイ系エフェクトプラグインの最小ビルド構成です。

- フォーマット: **VST3**（Windows / macOS / Linux）、**AU**（macOS）、Standalone（確認用）
- フレームワーク: JUCE 8.0.8（CMake FetchContent で取得）
- チャンネル: **モノラル／ステレオ両対応**（in == out のモノ→モノ、ステレオ→ステレオ）
- DSP コアはフレームワーク非依存の C++（`src/dsp/`）で、単体テスト付き

## 信号の流れ（仕様書 第3章の復元構造を踏襲）

```
入力 → オーバーサンプリング↑ → 変調ディレイ（Wow 3.63 Hz / Flutter 13.57+21.17 Hz、4 ms ライン）
     → テープEQカスケード6段（Slow/Fast テーブル切替）
     → ノイズ加算（ヒス＋クラックル） → サチュレーション2段（Age³ 駆動）
     → Age ダークニング → Low Cut → High Cut
     → 出力段（Power ゲート → Mix → Level） → オーバーサンプリング↓ → 出力
```

- 内部レートは約 192 kHz に統一（ホスト <50 kHz で ×4、<100 kHz で ×2、それ以上は ×1）。リサンプラは窓付き sinc（Blackman、32 タップ／位相）
- 分数ディレイの読み出しは 3 次エルミート補間（仕様書 4.4 の既定）
- レイテンシは固定（32 サンプル＋中心ディレイ 2 ms 分）。バイパス時も同じ経路を通るため Power のオン／オフで遅延は変わらない
- パラメータ平滑化は内部レートでのサンプル単位処理（ブロックサイズで音が変わらない）

## パラメータ（元製品の実値を採用）

| ID | 名称 | 範囲 | 既定値 |
| --- | --- | --- | --- |
| Powr | Power | Off / On | On |
| WowP | Wow Depth | 0〜100 % | 0 |
| WwSp | Wow Speed | −100〜+100 % | 0 |
| Fltt | Flutter Depth | 0〜100 % | 0 |
| FlSp | Flutter Speed | −100〜+100 % | 0 |
| Dirt | Dirt | 0〜100 % | 20 |
| AgeP | Age | 0〜100 % | 10 |
| Nois | Noise | −120〜−60 dB | −120（実質オフ） |
| LoCt | Low Cut Freq. | 10〜1,000 Hz（対数テーパー） | 10 Hz |
| LoCQ | Low Cut Q | 0.707〜1.414 | 1.061 |
| HiCt | High Cut Freq. | 1〜35 kHz（対数テーパー） | 35 kHz |
| HiCQ | High Cut Q | 0.707〜1.414 | 1.061 |
| MixP | Mix | 0〜100 % | 100 |
| Levl | Level | −18〜+18 dB | 0 |
| TpSp | Tape Speed | Slow / Fast | Fast |

テープ EQ の係数テーブル（タイプ／ゲイン／Q／周波数、Slow・Fast の2組）は復元した実値をそのまま使っています（`src/dsp/TapeEngine.cpp` の `kEq*` 表）。

## ビルド

### ローカル

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

成果物は `build/TapeLofi_artefacts/Release/` 以下（VST3 / AU / Standalone）。
Linux では JUCE の依存パッケージが必要です（`.github/workflows/build.yml` の linux ジョブ参照）。

### GitHub Actions

`.github/workflows/build.yml` が Windows（VST3）、macOS（VST3＋AU）、Linux（VST3）をビルドし、成果物を Artifacts に上げます。リポジトリに push すれば自動で走ります。

### DSP 単体テスト（JUCE 不要）

```sh
g++ -std=c++17 -O2 -I src src/dsp/TapeEngine.cpp tests/EngineTest.cpp -o engine_test
./engine_test
```

## Cubase での注意点

**`docs/CUBASE_NOTES.md` を必ず読んでください。** 要点だけ再掲します。

- `getNumPrograms()` は **1** を返す。2以上だと JUCE の VST3 ラッパーが「Program」パラメーターを作り、Cubase のフォーカス移動で `setCurrentProgram` が呼ばれて値が上書きされる
- それでも `setCurrentProgram(0)` は有効な呼び出しなので、本プロジェクトでは**ホスト向け program API を無力化**してある（`setCurrentProgram` は何もしない）。プリセット機能を足すときは、UI 専用の `loadPreset()` を別に作ること
- 状態の復元でプリセットローダーを呼ばない。値は APVTS の保存内容が正
- ビルドを入れ替えたら Cubase の再起動／再スキャンが必要（パラメーター構成のキャッシュ対策）

## 元製品との違い（意図的なもの）

- 乱数はインスタンス単位（元製品はグローバル共有だった）
- サチュレーションは同族の有理関数型（`x(27+x²)/(27+9x²)`、tanh 近似）で再実装。元製品の定数（0.8212／0.8932／0.8146）の正確な式形は未確定のため
- LFO の Speed 連動は ±100 % → ×0.5〜×2（1オクターブ）として解釈。50 Hz 発振器はフラッターに微量混ぜるハム成分として使用
- 出力段は Power → Mix → Level のクリーンな再実装（元製品は入れ子の線形補間）
- GUI は簡易版（全パラメータのノブ＋Power／Tape Speed のセレクタ）。製品 GUI（カセットデッキ風デザイン等）は別途

## 構成

```
CMakeLists.txt
.github/workflows/build.yml
src/PluginProcessor.h/.cpp   プラグイン外装（パラメータ、バス、状態）
src/PluginEditor.h/.cpp      簡易エディタ（ノブ＋セレクタ）
src/Presets.h                工場プリセット表（JUCE 非依存）
src/ui/PresetTabs.h/.cpp     プリセット棚（タブ UI）
src/dsp/TapeEngine.h/.cpp    フレームワーク非依存 DSP コア
tests/EngineTest.cpp         DSP 単体テスト
docs/CUBASE_NOTES.md         Cubase の不具合対策メモ
```


## V3 での追加点：プリセットタブ

パネルの**下側に専用の段（プリセット棚）を追加**しました。既存のつまみ・ヘッダーとは別の領域（高さ 566→686 px）なので、何も重なりません。

- 6×2 のカセットラベル風タブに、雨にちなんだ名前の工場プリセット 12 種
- クリックで全つまみが切り替わる（Power は変えない）。ホストのオートメーション履歴にも 1 つずつ残る
- 現在の音がどのプリセットと一致するかは、つまみの値から**毎回計算**して点灯（ランプ＋琥珀色の帯）。つまみを動かすと消えて「Custom」になる。プリセット番号は状態に保存しない
- マウスを載せるとツールチップで読みと説明が出る（日本語）
- プリセット表は `src/Presets.h`（JUCE 非依存）。`tests/EngineTest.cpp` で範囲・名前の重複・全プリセットの出力レベルを検査
- Cubase 対策は維持：`getNumPrograms()=1`、`setCurrentProgram()` は何もしない。読み込みは UI 専用の `loadPreset()` のみ

| タブ | 読み | 英名 | 音の傾向 |
| --- | --- | --- | --- |
| 小雨 | こさめ | Drizzle | ごく軽い味付け |
| 霧雨 | きりさめ | Mist | 高域がまるい、しっとり（Slow） |
| 春雨 | はるさめ | Spring Rain | ゆったりしたゆれ、やわらかい |
| 五月雨 | さみだれ | Early Summer Rain | ゆれとノイズが続く |
| 夕立 | ゆうだち | Evening Shower | ひずみ・ノイズ強め |
| 時雨 | しぐれ | Passing Shower | 大きなゆれ、音程がふらつく（Slow） |
| 雨宿り | あまやどり | Taking Shelter | くぐもった落ち着き（Slow） |
| 梅雨 | つゆ | Rainy Season | 古びて重たい（Slow） |
| 氷雨 | ひさめ | Cold Rain | 低域うすく冷たい |
| 雨音 | あまおと | Raindrops | パチパチ音を前面に |
| 雨上がり | あめあがり | After the Rain | 元音を残した軽いかけ方（Mix 70） |
| 豪雨 | ごうう | Downpour | 全部強め（Slow） |

プリセットを足すときは `src/Presets.h` の表に 1 行足すだけです（タブは 6 列で折り返し、2 行＝12 個までが収まる設計。13 個目以降は `kShelfHeight` と `PresetTabs` の高さを増やしてください）。

## V2 での修正点

V1 の実測で見つかった問題の修正です。詳細と較正の根拠は `docs/TUNING.md` を参照してください。

- サチュレーション：有理関数をクランプなしで評価していたため、大入力では「飽和」ではなく「減衰」になっていた（Age 100 で −10.6 dB、入力を上げても出力が下がる）。±3 でクランプし、ヘッドルームを設けて小信号ゲイン 1 に統一
- Dirt 0／Age 0 でも −0.6 dB・THD 約 3 % かかっていたのを、ほぼ透明（±0.1 dB）に
- Age のダークニングが Age 0 でも 18 kHz で効いていた（18 kHz で −3 dB）のを、Age 0 で全開に変更。係数は Hz 基準にし、ホストレートで音が変わらないように
- Age 100 ＋ Wow／Flutter 100 で変調量がディレイ線の範囲を超えてクリップしていたのを、範囲内に収まるよう修正
- Noise ノブの表示 dB と実出力が 6.6 dB ずれていたのを、全ホストレートで一致するよう較正
- Tape Speed 切替時のクリックを防ぐため、25 ms のクロスフェードを追加
- 申告レイテンシが 44.1 kHz で 0.2 サンプルずれる問題を修正（96k／192k を含め実測と一致をテスト）
- ホストが渡すチャンネル数がエンジンの準備数より少ない／null の場合のクラッシュ要因を除去（`process()` にチャンネル数引数を追加）
- リサンプラ／ディレイ線の内側ループから剰余演算を除去し、CPU 使用率を約 30 % 削減
