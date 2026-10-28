#pragma once
#include <array>
namespace relay {
// Focus transitions discard edges, so Alt-Tab cannot leave a stuck action.
class ButtonState {
 public:
  using Snapshot=std::array<bool,349>;
  void update(const Snapshot& sample,bool focused) {
    previous_=current_;
    current_=focused?sample:Snapshot{};
    if(!focused_ || !focused) previous_=current_;
    focused_=focused;
  }
  bool held(int key) const { return current_.at(static_cast<unsigned>(key)); }
  bool pressed(int key) const { return held(key)&&!previous_.at(static_cast<unsigned>(key)); }
  bool released(int key) const { return !held(key)&&previous_.at(static_cast<unsigned>(key)); }
 private:
  Snapshot current_{},previous_{};
  bool focused_=false;
};
}
