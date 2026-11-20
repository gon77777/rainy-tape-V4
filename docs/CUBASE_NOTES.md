# Cubase での注意点（JUCE 製 VST3 共通）

過去の JUCE 製プラグインで実際に踏んだ不具合と、その対策をまとめる。
本プロジェクト（TapeLofi）は最初から対策済みの構成にしてある。

## 1. 画面外クリックで値がデフォルト／先頭プリセットに戻る

### 症状

- プリセットを選んで音を作る。
- Cubase でプラグイン画面の外（別トラック、メインウィンドウなど）をクリックする。
- つまみの値が勝手に別の値（多くは先頭プリセット、またはデフォルト）に書き換わる。

### 原因

1. JUCE の VST3 ラッパーは、`getNumPrograms()` が **2以上** を返すと、ホストへ「Program」という隠しパラメーターを公開する。
2. Cubase は、プラグインウィンドウのフォーカスが外れたときなどに、このパラメーターの値をプラグインへ送り直す。
3. ラッパーは受け取った値から `setCurrentProgram(index)` を呼ぶ。Cubase 側の値は古いままで、多くは 0。
4. `setCurrentProgram` がプリセットの全値を APVTS に書く実装だと、いまの設定がプリセット 0（またはデフォルト）で上書きされる。

### 対策（必須）

- `getNumPrograms()` は `1` を返す（`0` は一部ホストで問題が出る。JUCE のドキュメントも `1` を推奨）。
- プリセット数は `getNumPresets()` など別名のメソッドで公開し、UI はそちらを使う。

## 2. 同じプラグインを2つ入れるとプリセットが変わる

### 経緯

上記1の対策後も、同じプラグインをプロジェクトに2つ入れるとプリセットが変わる現象が出た。
コード上、インスタンス間で状態を共有する箇所は存在しなかったため、原因はホスト側の呼び出しと推定される。

### 推定原因（未確認）

- `getNumPrograms()` を `1` にしても、`setCurrentProgram(0)` は**有効な呼び出し**のまま（プログラム1個なら番号0は範囲内）。
- ホストはインスタンス生成時や状態読込時などに `setCurrentProgram(0)` を呼ぶことがある。
- その実装が「プリセット0の全値を APVTS に書く」だと、保存値の復元後などに上書きされ、プリセットが変わったように見える。

### 対策（本プロジェクトで採用済み）

ホスト向けとプラグイン内部向けの関数を分ける。

```cpp
// ---- ホスト向け（何もしない） ----
int getNumPrograms() override { return 1; }
int getCurrentProgram() override { return 0; }
void setCurrentProgram (int) override {}              // 呼ばれても値は変えない
const juce::String getProgramName (int) override;     // 名前だけ返すのは可
void changeProgramName (int, const juce::String&) override {}

// ---- プラグイン内部（UI）向け ----
int getNumPresets() const noexcept;                   // プリセット数
int getCurrentPreset() const noexcept;                // 現在のプリセット番号
void loadPreset (int index);                          // 旧 setCurrentProgram の中身
```

- UI とテストは `loadPreset` / `getCurrentPreset` を使う。`setCurrentProgram` / `getCurrentProgram` が UI 側に残っていないか検索で確認する。
- ホストが `setCurrentProgram` をいつ何番で呼んでも、値は変わらない。

## 3. 状態の保存・復元

- プリセット番号が必要なら、状態 XML に属性として自前で保存する（例：`xml->setAttribute ("preset", currentPreset)`）。
- **復元時に `setCurrentProgram` / `loadPreset` を呼ばない。** 値は APVTS の保存内容が正で、番号は表示のためだけに戻す。
- `setStateInformation` の冒頭で「全パラメーターを既定値へ戻す」ループ（`setValueNotifyingHost`）を置くのも疑わしい。復元中にホストがその通知を自分の値として取り込む可能性がある。状態に無かった ID だけを既定値にする方が安全。

## 4. 切り替え時の注意（Cubase のキャッシュ）

- VST3 を入れ替えたら Cubase を再起動する。直らなければプラグインマネージャーで再スキャンする。
- Cubase はパラメーター構成をキャッシュする。保存済みプロジェクトは古い構成（Program パラメーターあり）を覚えている場合があるので、プラグインを一度外して挿し直して確認する。
- 確認方法：オートメーション一覧に「Program」パラメーターが出ていないこと。出ていれば古いビルドかキャッシュ残り。

## 4.5 V3 のプリセットタブとの関係

- タブは `loadPreset()`（UI 専用）だけを呼ぶ。ホストから呼ばれる経路には繋いでいない
- 点灯するタブは、つまみの値との一致を毎回計算して決める。番号を状態に保存しないので、復元時に呼ぶものが無い
- 確認項目：タブをクリック → 画面外クリックしても値が変わらない／同じプラグインを2つ入れて片方を切り替えても他方が変わらない／保存→再読込で値とタブ点灯が一致

## 5. この対策の代償

- Cubase のプリセット欄（ホスト側のプログラムリスト）にはプラグイン内プリセットが出なくなる。切り替えはプラグイン内のプリセットタブで行う。
- ホストにプリセット一覧を出したい場合は、別途 `.vstpreset` の用意など別の対策が必要。

## 6. それでも直らない場合の切り分け

`loadPreset`、`setStateInformation`、`getStateInformation` の先頭にインスタンスアドレス付きのログを入れ、どのインスタンスのどの関数がいつ呼ばれたかを記録で確かめる（推測で直そうとしない）。

```cpp
juce::Logger::writeToLog ("[TapeLofi " + juce::String::toHexString ((juce::int64) (uintptr_t) this)
                          + "] loadPreset " + juce::String (index));
```

## 要点

| 項目 | 内容 |
| --- | --- |
| 原因1 | `getNumPrograms()` が2以上で Program パラメーターが作られ、Cubase のフォーカス移動で `setCurrentProgram` が呼ばれる |
| 原因2（推定） | `getNumPrograms()=1` でも `setCurrentProgram(0)` は有効で、ホストから呼ばれると全値が上書きされる |
| 修正 | ホスト向け program API を無力化し、プリセット読込は UI 専用の `loadPreset` に分離する |
| 状態 | APVTS の値が正。プリセット番号は属性で保存し、復元時にローダーを呼ばない |
| 確認 | 画面外クリック、複数インスタンス、保存→再読込で値が変わらないこと |
