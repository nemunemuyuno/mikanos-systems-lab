
## 第15章：`ActiveLayer`の初期化順に注意

公式コードでは `active_layer->Activate(task_b_window_layer_id)` が `InitializeMouse()` より前に呼ばれていた。

しかし `ActiveLayer::Activate()` は内部で

```cpp
manager_.GetHeight(mouse_layer_) - 1
```

を使って、アクティブウィンドウをマウスレイヤの1段下へ移動する。

`mouse_layer_` は初期値が `0` で、実際のマウスレイヤIDが設定されるのは `InitializeMouse()` 内の

```cpp
active_layer->SetMouseLayer(mouse_layer_id);
```

の時点。

したがってMouse初期化前に `Activate()` すると、

```text
mouse_layer_ = 0
→ GetHeight(0) = -1
→ new_height = -2
→ UpDown(..., -2)
→ Hide()
```

となり、TaskBのレイヤが非表示になる可能性がある。

また `TaskTerminal()` も起動時に `active_layer->Activate()` を呼ぶため、Terminal TaskもMouse初期化前にWakeupさせない方がよい。

修正後の基本順序：

```text
Layer / Window生成
↓
TaskManager初期化
↓
xHCI初期化
↓
Keyboard / Mouse初期化
↓
mouse_layer_確定
↓
TaskBをActivate
↓
TaskB / Terminal Taskを生成・Wakeup
```


## 第16章：Terminal初回Activate時のz-orderずれ

Terminalは生成時には `layers_` に登録されるだけで、`layer_stack_` にはまだ入っていない。
最初の

```cpp
active_layer->Activate(terminal->LayerID());
```

で初めて `UpDown()` により `layer_stack_` へ挿入される。

このとき `GetHeight(mouse_layer_) - 1` を挿入位置として使うが、Terminalはまだstack内にいないため `erase()` が発生しない。その結果、挿入によってMouseや既存Windowのindexが1つずれ、Terminalが意図した「Mouse直下」ではなくTextBoxの下に入ることがある。

その後、一度別WindowをActivateしてからTerminalをActivateし直すと、今度はTerminalが既に `layer_stack_` に存在するため、

```text
erase → 正しい位置へinsert
```

となり、Mouse直下へ正常に移動する。

## 第20章：  
20.2で rpn をCPL=3で起動すると、QEMU全体がフリーズして再起動した。
コードは概ね書籍通りで、20.2時点ではTSS/RSP0が未設定なため、Ring 3実行中のタイマ割り込みで不正なKernel stackを使って例外・resetに至っている可能性が高い。
20.3でTSS/RSP0を正しく設定した後に再確認する。

## 第21章：  
テキストには「OS用領域（PML4[0]〜PML4[255]）はすべてのコンテキストで共通だが、アプリ用領域（PML4[256]以降）は各コンテキストに固有」と書いてあったが、そもそも20章までは同じPML4を使ってたから、同じCR3だったはずだから、記述が矛盾している

## 第23章：  
### paintがTerminal上のマウス操作でクラッシュ

`paint` 実行中、Terminal上でマウスを操作するとpaintにも描画され、特定座標ではOSがクラッシュした。

原因は、TerminalとpaintのLayerが同じTerminal Taskに紐付いているため、`SendMouseMessage()` がTerminal向けに送ったマウスイベントもpaintの `ReadEvent()` が受け取ってしまうこと。

さらに `kMouseButton` 処理では座標範囲を確認せず、

```cpp
SyscallWinFillRectangle(layer_id, arg.x, arg.y, 1, 1, 0x000000);
```

していたため、Terminal基準の座標をpaint Windowへそのまま描画し、範囲外アクセスが発生していた。

特に、

* `x` のみ範囲外：`data_[y]` 自体は存在するため、隣接メモリを壊しても即座には落ちない場合がある
* `y` が範囲外：`data_[y]` の行そのものが存在せず、より直接的に不正アクセスしてクラッシュしやすい

という違いが見られた。だからX超過はたまたま耐えるのに、Y超過すると落ちる。ただし、どちらも未定義動作で安全ではない。

暫定修正：

```cpp
if (IsInside(arg.x, arg.y)) {
  SyscallWinFillRectangle(
      layer_id, arg.x, arg.y, 1, 1, 0x000000);
}
```

これでクラッシュは解消した。

根本的には、イベント配送が `Layer → Task` までしか識別しておらず、同一Task内のどのWindow向けイベントか区別できないことが原因。

### Terminalスクロール時に表示が崩れる

`timer 10000` 実行中にマウスをTerminal上で動かすと、文字が部分的に消えたり復元されたりする現象が発生した。特に `ls` や `echo` などでTerminalの表示行数が増え、スクロールが発生した後に再現しやすかった。

原因は `Terminal::Print()` の部分再描画範囲の計算。

通常は、

```text
cursor_before ～ cursor_after
```

を再描画すれば十分だが、`Scroll1()` が発生すると **TerminalのWindow buffer全体が1行上へ移動する**。

しかしスクロール前後でカーソルは最下段に留まるため、`Print()` は変更範囲を最下段付近だけだと誤認し、実際に変更されたTerminal全体をscreenへ反映しない。

その結果、

```text
Terminal Window buffer = 最新
screen / back_buffer   = 一部古い
```

という不整合が発生する。

この状態でマウスを動かすと、カーソルが通った部分だけ `LayerManager::Draw()` により最新Windowから再描画されるため、文字が消えたり戻ったりするように見えた。

一時修正として `Terminal::Print()` の再描画範囲を常にTerminal全体に変更した。

```cpp
Rectangle<int> draw_area{
    ToplevelWindow::kTopLeftMargin,
    window_->InnerSize()
};
```

これにより症状は解消した。

本質的には、**スクロール発生時だけdirty rectangleをTerminal全体にする**のがより適切。
