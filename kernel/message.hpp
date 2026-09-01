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
    } keyboard;

    struct {
      LayerOperation op;
      unsigned int layer_id;
      int x, y;
      int w, h;  //幅と高さ
    } layer;

  } arg;
};
