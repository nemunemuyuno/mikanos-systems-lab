
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

教訓：**初期化関数にはコード上明示されていない前提条件があることがある。`Activate()`は「Mouse Layerが初期化済み」という暗黙の前提を持っていた。**



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

教訓：**「既存Layerの高さ変更」と「stack外のLayerの初回挿入」で、同じheight指定でも結果が異なることがある。**
