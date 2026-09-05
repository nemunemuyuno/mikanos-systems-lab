#pragma once

#include <deque>
#include <map>
#include "window.hpp"
#include "task.hpp"
#include "layer.hpp"
#include "fat.hpp"

// #@@range_begin(term)
class Terminal {
 public:
  static const int kRows = 15, kColumns = 60;
  // #@@range_begin(linemax)
  static const int kLineMax = 128;
  // #@@range_end(linemax)

  Terminal();
  unsigned int LayerID() const { return layer_id_; }
  Rectangle<int> BlinkCursor();
  //これ多分入力文字とカーソルの2文字分だけの描画範囲
  Rectangle<int> InputKey(uint8_t modifier, uint8_t keycode, char ascii);

 private:
  std::shared_ptr<ToplevelWindow> window_;
  unsigned int layer_id_;

  Vector2D<int> cursor_{0, 0};
  bool cursor_visible_{false};
  void DrawCursor(bool visible);
  Vector2D<int> CalcCursorPos() const;

  //キー入力を1行分溜めておくバッファ
  int linebuf_index_{0};
  std::array<char, kLineMax> linebuf_{};
  //入力がターミナルの最終行に来た時に1行画面を上にずらす
  void Scroll1();

  void ExecuteLine();
  void ExecuteFile(const fat::DirectoryEntry& file_entry, char* command, char* first_arg);

  void Print(const char* s);
  void Print(char c);

  // #@@range_begin(term_fields)
  std::deque<std::array<char, kLineMax>> cmd_history_{};
  int cmd_history_index_{-1};
  Rectangle<int> HistoryUpDown(int direction);
  // #@@range_end(term_fields)
};

void TaskTerminal(uint64_t task_id, int64_t data);
// #@@range_end(term)
