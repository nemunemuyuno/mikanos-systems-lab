#pragma once


// #@@range_begin(layer_op)
enum class LayerOperation {
  Move, MoveRelative, Draw, DrawArea
};
// #@@range_end(layer_op)

struct Message {
  enum Type {
    kInterruptXHCI,
    kTimerTimeout,
    kKeyPush,
    kLayer,
    kLayerFinish,
    kMouseMove,
    kMouseButton,
  } type;

  uint64_t src_task;     //送信元タスクのIDを表す
  union {
    struct {
      unsigned long timeout;
      int value;
    } timer;

    struct {
      uint8_t modifier;
      uint8_t keycode;
      char ascii;
      int press;
    } keyboard;

    struct {
      LayerOperation op;
      unsigned int layer_id;
      int x, y;
      int w, h;  //幅と高さ
    } layer;
    struct {
      int x, y;
      int dx, dy;
      uint8_t buttons;
    } mouse_move;
    struct {
      int x, y;
      int press; // 1: press, 0: release
      int button;
    } mouse_button;
  } arg;
};
