
### 第15章：`ActiveLayer`の初期化順に注意

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


### 第16章：Terminal初回Activate時のz-orderずれ

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

### 第20章：  
20.2で rpn をCPL=3で起動すると、QEMU全体がフリーズして再起動した。
コードは概ね書籍通りで、20.2時点ではTSS/RSP0が未設定なため、Ring 3実行中のタイマ割り込みで不正なKernel stackを使って例外・resetに至っている可能性が高い。
20.3でTSS/RSP0を正しく設定した後に再確認する。

### 第21章：  
テキストには「OS用領域（PML4[0]〜PML4[255]）はすべてのコンテキストで共通だが、アプリ用領域（PML4[256]以降）は各コンテキストに固有」と書いてあったが、そもそも20章までは同じPML4を使ってたから、同じCR3だったはずだから、記述が矛盾している