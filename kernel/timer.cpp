#include "timer.hpp"
#include "interrupt.hpp"
#include "acpi.hpp"
#include "task.hpp"

namespace {
  const uint32_t kCountMax = 0xffffffffu;
  volatile uint32_t& lvt_timer = *reinterpret_cast<uint32_t*>(0xfee00320);
  volatile uint32_t& initial_count = *reinterpret_cast<uint32_t*>(0xfee00380);
  volatile uint32_t& current_count = *reinterpret_cast<uint32_t*>(0xfee00390);
  volatile uint32_t& divide_config = *reinterpret_cast<uint32_t*>(0xfee003e0);
}

void InitializeLAPICTimer() {
  timer_manager = new TimerManager;
  divide_config = 0b1011; // divide 1:1
  /*Periodic Modeを指定している,後半でベクタ番号を指定しているのだろうが、
  この機能がデフォでlvt_timerについているのだろうか*/
  //実際、Local APIC の LVT Timer Register の下位8ビット [7:0] は割り込みベクタ番号っぽい
  lvt_timer = (0b010 << 16) | InterruptVector::kLAPICTimer; 
  StartLAPICTimer();
  acpi::WaitMilliseconds(100);
  const auto elapsed = LAPICTimerElapsed();
  StopLAPICTimer();
  //1sあたりのapicの方のカウント数
  lapic_timer_freq = static_cast<unsigned long>(elapsed)*10;
  lvt_timer = (0b010 << 16) | InterruptVector::kLAPICTimer;
  //100で割ることで10msあたりにカウント何回するかに変換しててそれがカウントダウンの初期値に
  initial_count = lapic_timer_freq / kTimerFreq; //10msに設定
}

void StartLAPICTimer() {
  initial_count = kCountMax;
}

uint32_t LAPICTimerElapsed() {
  return kCountMax - current_count;
}

void StopLAPICTimer() {
  initial_count = 0;
}


Timer::Timer(unsigned long timeout, int value)
    : timeout_{timeout}, value_{value} {
}


// #@@range_begin(timermgr_addtimer)
TimerManager::TimerManager(){
  timers_.push(Timer{std::numeric_limits<unsigned long>::max(), -1}); //番兵法
}

void TimerManager::AddTimer(const Timer& timer) {
  timers_.push(timer);
}
// #@@range_end(timermgr_addtimer)

// #@@range_begin(tick)
bool TimerManager::Tick() {
  ++tick_;

  bool task_timer_timeout = false;
  while (true) {
    const auto& t = timers_.top();
    if (t.Timeout() > tick_) {
      break;
    }

    if (t.Value() == kTaskTimerValue) {
      task_timer_timeout = true;
      timers_.pop();
      timers_.push(Timer{tick_ + kTaskTimerPeriod, kTaskTimerValue});
      continue;
    }

    Message m{Message::kTimerTimeout};
    m.arg.timer.timeout = t.Timeout();
    m.arg.timer.value = t.Value();
    task_manager->SendMessage(1, m); //今まで

    timers_.pop();
  }

  return task_timer_timeout;
}
// #@@range_end(tick)

TimerManager* timer_manager;
unsigned long lapic_timer_freq;

// #@@range_begin(call_switchtask)
extern "C" void LAPICTimerOnInterrupt(const TaskContext& ctx_stack) {
  const bool task_timer_timeout = timer_manager->Tick();
  NotifyEndOfInterrupt();

  if (task_timer_timeout) {
    task_manager->SwitchTask(ctx_stack);
  }
}
// #@@range_end(call_switchtask)