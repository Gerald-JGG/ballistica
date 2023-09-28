// Released under the MIT License. See LICENSE for details.

#include "ballistica/base/ui/dev_console.h"

#include "ballistica/base/app_mode/app_mode.h"
#include "ballistica/base/audio/audio.h"
#include "ballistica/base/graphics/component/simple_component.h"
#include "ballistica/base/graphics/text/text_graphics.h"
#include "ballistica/base/input/input.h"
#include "ballistica/base/logic/logic.h"
#include "ballistica/base/platform/base_platform.h"
#include "ballistica/base/python/base_python.h"
#include "ballistica/base/support/context.h"
#include "ballistica/base/ui/ui.h"
#include "ballistica/core/core.h"
#include "ballistica/core/platform/support/min_sdl.h"
#include "ballistica/shared/foundation/event_loop.h"
#include "ballistica/shared/generic/utils.h"
#include "ballistica/shared/python/python_command.h"
#include "ballistica/shared/python/python_sys.h"

namespace ballistica::base {

// How much of the screen the console covers when it is at full size.
const float kDevConsoleSize = 0.9f;
const int kDevConsoleLineLimit = 80;
const int kDevConsoleStringBreakUpSize = 1950;
const int kDevConsoleActivateKey1 = SDLK_BACKQUOTE;
const int kDevConsoleActivateKey2 = SDLK_F2;

const double kTransitionSeconds = 0.1;

enum class DevButtonAttach_ { kLeft, kCenter, kRight };

static auto XOffs(DevButtonAttach_ attach) -> float {
  switch (attach) {
    case DevButtonAttach_::kLeft:
      return 0.0f;
    case DevButtonAttach_::kRight:
      return g_base->graphics->screen_virtual_width();
    case DevButtonAttach_::kCenter:
      return g_base->graphics->screen_virtual_width() * 0.5f;
  }
  assert(false);
  return 0.0f;
}

static void DrawBasicButton(RenderPass* pass, TextGroup* tgrp, float tscale,
                            float bottom, float x, float y, float width,
                            float height, const Vector3f& fgcolor,
                            const Vector3f& bgcolor) {
  SimpleComponent c(pass);
  c.SetTransparent(true);
  c.SetColor(bgcolor.x, bgcolor.y, bgcolor.z, 1.0f);
  {
    auto xf = c.ScopedTransform();
    c.Translate(x + width * 0.5f, y + bottom + height * 0.5f,
                kDevConsoleZDepth);
    // Draw our backing.
    {
      auto xf = c.ScopedTransform();
      c.Scale(width, height);
      c.DrawMeshAsset(g_base->assets->SysMesh(SysMeshID::kImage1x1));
    }
    // Draw our text.
    c.SetColor(fgcolor.x, fgcolor.y, fgcolor.z, 1.0f);
    c.SetFlatness(1.0f);
    int elem_count = tgrp->GetElementCount();
    for (int e = 0; e < elem_count; e++) {
      c.SetTexture(tgrp->GetElementTexture(e));
      {
        auto xf = c.ScopedTransform();
        float sc{0.6f * tscale};
        c.Scale(sc, sc, 1.0f);
        c.DrawMesh(tgrp->GetElementMesh(e));
      }
    }
  }
  c.Submit();
}

/// Super-simple widget type for populating dev-console
/// (we don't want to depend on any of our full UI feature-sets).
class DevConsole::Widget_ {
 public:
  virtual ~Widget_() = default;
  virtual auto HandleMouseDown(float mx, float my) -> bool = 0;
  virtual void HandleMouseUp(float mx, float my) = 0;
  virtual void Draw(RenderPass* pass, float bottom) = 0;
};

class DevConsole::Button_ : public DevConsole::Widget_ {
 public:
  DevButtonAttach_ attach;
  float x;
  float y;
  float width;
  float height;
  bool pressed{};
  Object::Ref<Runnable> call;
  TextGroup text_group;
  float text_scale;

  template <typename F>
  Button_(const std::string& label, float text_scale, DevButtonAttach_ attach,
          float x, float y, float width, float height, const F& lambda)
      : attach{attach},
        x{x},
        y{y},
        width{width},
        height{height},
        call{NewLambdaRunnable(lambda)},
        text_scale{text_scale} {
    text_group.SetText(label, TextMesh::HAlign::kCenter,
                       TextMesh::VAlign::kCenter);
  }

  auto InUs(float mx, float my) -> bool {
    mx -= XOffs(attach);
    return (mx >= x && mx <= (x + width) && my >= y && my <= (y + height));
  }

  auto HandleMouseDown(float mx, float my) -> bool override {
    if (InUs(mx, my)) {
      pressed = true;
      return true;
    }
    return false;
  }

  void HandleMouseUp(float mx, float my) override {
    if (pressed) {
      pressed = false;
      if (InUs(mx, my)) {
        if (call.Exists()) {
          call.Get()->Run();
        }
      }
    }
  }

  void Draw(RenderPass* pass, float bottom) override {
    DrawBasicButton(
        pass, &text_group, text_scale, bottom, x + XOffs(attach), y, width,
        height,
        pressed ? Vector3f{0.0f, 0.0f, 0.0f} : Vector3f{0.8f, 0.7f, 0.8f},
        pressed ? Vector3f{0.8f, 0.7f, 0.8f} : Vector3f{0.25, 0.2f, 0.3f});
  }
};

class DevConsole::ToggleButton_ : public DevConsole::Widget_ {
 public:
  DevButtonAttach_ attach;
  float x;
  float y;
  float width;
  float height;
  bool pressed{};
  bool on{};
  Object::Ref<Runnable> on_call;
  Object::Ref<Runnable> off_call;
  TextGroup text_group;
  float text_scale;

  template <typename F, typename G>
  ToggleButton_(const std::string& label, float text_scale,
                DevButtonAttach_ attach, float x, float y, float width,
                float height, const F& on_call, const G& off_call)
      : attach{attach},
        x{x},
        y{y},
        width{width},
        height{height},
        on_call{NewLambdaRunnable(on_call)},
        off_call{NewLambdaRunnable(off_call)},
        text_scale{text_scale} {
    text_group.SetText(label, TextMesh::HAlign::kCenter,
                       TextMesh::VAlign::kCenter);
  }

  auto InUs(float mx, float my) -> bool {
    mx -= XOffs(attach);
    return (mx >= x && mx <= (x + width) && my >= y && my <= (y + height));
  }

  auto HandleMouseDown(float mx, float my) -> bool override {
    if (InUs(mx, my)) {
      pressed = true;
      return true;
    }
    return false;
  }

  void HandleMouseUp(float mx, float my) override {
    if (pressed) {
      pressed = false;
      if (InUs(mx, my)) {
        on = !on;
        auto&& call = on ? on_call : off_call;
        if (call.Exists()) {
          call.Get()->Run();
        }
      }
    }
  }

  void Draw(RenderPass* pass, float bottom) override {
    DrawBasicButton(pass, &text_group, text_scale, bottom, x + XOffs(attach), y,
                    width, height,
                    pressed ? Vector3f{1.0f, 1.0f, 1.0f}
                    : on    ? Vector3f{1.0f, 1.0f, 1.0f}
                            : Vector3f{0.8f, 0.7f, 0.8f},
                    pressed ? Vector3f{0.5f, 0.2f, 1.0f}
                    : on    ? Vector3f{0.5f, 0.4f, 0.6f}
                            : Vector3f{0.25, 0.2f, 0.3f});
  }
};

class DevConsole::TabButton_ : public DevConsole::Widget_ {
 public:
  DevButtonAttach_ attach;
  float x;
  float y;
  float width;
  float height;
  bool pressed{};
  bool selected{};
  Object::Ref<Runnable> call;
  TextGroup text_group;
  float text_scale;

  template <typename F>
  TabButton_(const std::string& label, bool selected, float text_scale,
             DevButtonAttach_ attach, float x, float y, float width,
             float height, const F& call)
      : attach{attach},
        x{x},
        y{y},
        selected{selected},
        width{width},
        height{height},
        call{NewLambdaRunnable(call)},
        text_scale{text_scale} {
    text_group.SetText(label, TextMesh::HAlign::kCenter,
                       TextMesh::VAlign::kCenter);
  }

  auto InUs(float mx, float my) -> bool {
    mx -= XOffs(attach);
    return (mx >= x && mx <= (x + width) && my >= y && my <= (y + height));
  }

  auto HandleMouseDown(float mx, float my) -> bool override {
    if (InUs(mx, my) && !selected) {
      pressed = true;
      return true;
    }
    return false;
  }

  void HandleMouseUp(float mx, float my) override {
    if (pressed) {
      pressed = false;
      if (InUs(mx, my)) {
        if (call.Exists()) {
          call.Get()->Run();
        }
      }
    }
  }

  void Draw(RenderPass* pass, float bottom) override {
    DrawBasicButton(pass, &text_group, text_scale, bottom, x + XOffs(attach), y,
                    width, height,
                    pressed    ? Vector3f{1.0f, 1.0f, 1.0f}
                    : selected ? Vector3f{1.0f, 1.0f, 1.0f}
                               : Vector3f{0.8f, 0.7f, 0.8f},
                    pressed    ? Vector3f{0.5f, 0.2f, 1.0f}
                    : selected ? Vector3f{0.5f, 0.4f, 0.6f}
                               : Vector3f{0.25, 0.2f, 0.3f});
  }
};

class DevConsole::Line_ {
 public:
  Line_(std::string s_in, double c) : creation_time(c), s(std::move(s_in)) {}
  double creation_time;
  std::string s;
  auto GetText() -> TextGroup& {
    if (!s_mesh_.Exists()) {
      s_mesh_ = Object::New<TextGroup>();
      s_mesh_->SetText(s);
    }
    return *s_mesh_;
  }

 private:
  Object::Ref<TextGroup> s_mesh_;
};

DevConsole::DevConsole() {
  assert(g_base->InLogicThread());
  std::string title = std::string("BallisticaKit ") + kEngineVersion + " ("
                      + std::to_string(kEngineBuildNumber) + ")";
  if (g_buildconfig.debug_build()) {
    title += " (debug)";
  }
  if (g_buildconfig.test_build()) {
    title += " (test)";
  }

  title_text_group_.SetText(title);
  built_text_group_.SetText("Built: " __DATE__ " " __TIME__);
  prompt_text_group_.SetText(">");

  Refresh();
}

void DevConsole::Refresh() {
  BA_PRECONDITION(g_base->InLogicThread());
  buttons_.clear();
  tab_buttons_.clear();
  RefreshTabsButtons_();

  if (active_tab_ == "Python") {
    float bs = PythonConsoleBaseScale_();
    buttons_.emplace_back(std::make_unique<Button_>(
        "Exec", 0.75f * bs, DevButtonAttach_::kRight, -33.0f * bs, 15.95f * bs,
        32.0f * bs, 13.0f * bs, [this] { Exec(); }));
  }
}

void DevConsole::RefreshTabsButtons_() {
  float bs = PythonConsoleBaseScale_();
  float bwidth = 90.0f * bs;
  float bheight = 26.0f * bs;
  float bscale = 0.8f * bs;
  float total_width = tabs_.size() * bwidth;
  float x = total_width * -0.5f;

  for (auto&& tab : tabs_) {
    tab_buttons_.emplace_back(std::make_unique<TabButton_>(
        tab, active_tab_ == tab, bscale, DevButtonAttach_::kCenter, x, -bheight,
        bwidth, bheight, [this, tab] {
          active_tab_ = tab;
          Refresh();
        }));
    x += bwidth;
  }
}
DevConsole::~DevConsole() = default;

auto DevConsole::HandleMouseDown(int button, float x, float y) -> bool {
  assert(g_base->InLogicThread());

  if (state_ == State_::kInactive) {
    return false;
  }
  float bottom{Bottom_()};

  // Pass to any buttons (in bottom-local space).
  if (button == 1) {
    for (auto&& button : tab_buttons_) {
      if (button->HandleMouseDown(x, y - bottom)) {
        return true;
      }
    }
    for (auto&& button : buttons_) {
      if (button->HandleMouseDown(x, y - bottom)) {
        return true;
      }
    }
  }

  if (y < bottom) {
    return false;
  }

  if (button == 1) {
    python_console_pressed_ = true;
  }

  return true;
}

void DevConsole::HandleMouseUp(int button, float x, float y) {
  assert(g_base->InLogicThread());
  float bottom{Bottom_()};

  if (button == 1) {
    for (auto&& button : tab_buttons_) {
      button->HandleMouseUp(x, y - bottom);
    }
    for (auto&& button : buttons_) {
      button->HandleMouseUp(x, y - bottom);
    }
  }

  if (button == 1 && python_console_pressed_) {
    python_console_pressed_ = false;
    if (y > bottom) {
      // If we're not getting fed keyboard events and have a string editor
      // available, invoke it.
      if (!g_base->ui->UIHasDirectKeyboardInput()
          && g_base->platform->HaveStringEditor()) {
        InvokeStringEditor_();
      }
    }
  }
}

void DevConsole::InvokeStringEditor_() {
  // If there's already a valid edit-adapter attached to us, do nothing.
  if (string_edit_adapter_.Exists()
      && !g_base->python->CanPyStringEditAdapterBeReplaced(
          string_edit_adapter_.Get())) {
    return;
  }

  // Create a Python StringEditAdapter for this widget, passing ourself as
  // the sole arg.
  auto result = g_base->python->objs()
                    .Get(BasePython::ObjID::kDevConsoleStringEditAdapterClass)
                    .Call();
  if (!result.Exists()) {
    Log(LogLevel::kError, "Error invoking string edit dialog.");
    return;
  }

  // If this new one is already marked replacable, it means it wasn't able
  // to register as the active one, so we can ignore it.
  if (g_base->python->CanPyStringEditAdapterBeReplaced(result.Get())) {
    return;
  }

  // Ok looks like we're good; store the adapter as our active one.
  string_edit_adapter_ = result;

  g_base->platform->InvokeStringEditor(string_edit_adapter_.Get());
}

void DevConsole::set_input_string(const std::string& val) {
  assert(g_base->InLogicThread());
  input_string_ = val;
  input_text_dirty_ = true;
}

void DevConsole::InputAdapterFinish() {
  assert(g_base->InLogicThread());
  string_edit_adapter_.Release();
}

auto DevConsole::HandleKeyPress(const SDL_Keysym* keysym) -> bool {
  assert(g_base->InLogicThread());

  // Handle our toggle buttons no matter whether we're active.
  switch (keysym->sym) {
    case kDevConsoleActivateKey1:
    case kDevConsoleActivateKey2: {
      if (!g_buildconfig.demo_build() && !g_buildconfig.arcade_build()) {
        // (reset input so characters don't continue walking and stuff)
        g_base->input->ResetHoldStates();
        if (auto console = g_base->ui->dev_console()) {
          console->ToggleState();
        }
      }
      return true;
    }
    default:
      break;
  }

  if (state_ == State_::kInactive) {
    return false;
  }

  // The rest of these presses we only handle while active.
  switch (keysym->sym) {
    case SDLK_ESCAPE:
      Dismiss();
      break;
    case SDLK_BACKSPACE:
    case SDLK_DELETE: {
      std::vector<uint32_t> unichars =
          Utils::UnicodeFromUTF8(input_string_, "fjco38");
      if (!unichars.empty()) {
        unichars.resize(unichars.size() - 1);
        input_string_ = Utils::UTF8FromUnicode(unichars);
        input_text_dirty_ = true;
      }
      break;
    }
    case SDLK_UP:
    case SDLK_DOWN: {
      if (input_history_.empty()) {
        break;
      }
      if (keysym->sym == SDLK_UP) {
        input_history_position_++;
      } else {
        input_history_position_--;
      }
      int input_history_position_used =
          (input_history_position_ - 1)
          % static_cast<int>(input_history_.size());
      int j = 0;
      for (auto& i : input_history_) {
        if (j == input_history_position_used) {
          input_string_ = i;
          input_text_dirty_ = true;
          break;
        }
        j++;
      }
      break;
    }
    case SDLK_KP_ENTER:
    case SDLK_RETURN: {
      Exec();
      break;
    }
    default: {
      // #if BA_SDL2_BUILD || BA_MINSDL_BUILD
      //       // (in SDL2/Non-SDL we dont' get chars from keypress events;
      //       // they come through as text edit events)
      // #else   // BA_SDL2_BUILD
      //       if (keysym->unicode < 0x80 && keysym->unicode > 0) {
      //         std::vector<uint32_t> unichars =
      //             Utils::UnicodeFromUTF8(input_string_, "cjofrh0");
      //         unichars.push_back(keysym->unicode);
      //         input_string_ = Utils::GetValidUTF8(
      //             Utils::UTF8FromUnicode(unichars).c_str(), "sdkr");
      //         input_text_dirty_ = true;
      //       }
      // #endif  // BA_SDL2_BUILD
      break;
    }
  }
  return true;
}

void DevConsole::Exec() {
  BA_PRECONDITION(g_base->InLogicThread());
  if (!input_enabled_) {
    Log(LogLevel::kWarning, "Console input is not allowed yet.");
    return;
  }
  input_history_position_ = 0;
  if (input_string_ == "clear") {
    last_line_.clear();
    lines_.clear();
  } else {
    SubmitCommand_(input_string_);
  }
  input_history_.push_front(input_string_);
  if (input_history_.size() > 100) {
    input_history_.pop_back();
  }
  input_string_.resize(0);
  input_text_dirty_ = true;
}

void DevConsole::SubmitCommand_(const std::string& command) {
  assert(g_base);
  g_base->logic->event_loop()->PushCall([command, this] {
    // These are always run in whichever context is 'visible'.
    ScopedSetContext ssc(g_base->app_mode()->GetForegroundContext());
    PythonCommand cmd(command, "<console>");
    if (!g_core->user_ran_commands) {
      g_core->user_ran_commands = true;
    }
    if (cmd.CanEval()) {
      auto obj = cmd.Eval(true, nullptr, nullptr);
      if (obj.Exists() && obj.Get() != Py_None) {
        Print(obj.Repr() + "\n");
      }
    } else {
      // Not eval-able; just exec it.
      cmd.Exec(true, nullptr, nullptr);
    }
  });
}

void DevConsole::EnableInput() {
  assert(g_base->InLogicThread());
  input_enabled_ = true;
}

void DevConsole::Dismiss() {
  assert(g_base->InLogicThread());
  if (state_ == State_::kInactive) {
    return;
  }

  state_prev_ = state_;
  state_ = State_::kInactive;
  transition_start_ = g_base->logic->display_time();
}

void DevConsole::ToggleState() {
  assert(g_base->InLogicThread());
  state_prev_ = state_;
  switch (state_) {
    case State_::kInactive:
      state_ = State_::kMini;
      break;
    case State_::kMini:
      state_ = State_::kFull;
      break;
    case State_::kFull:
      state_ = State_::kInactive;
      break;
  }
  g_base->audio->PlaySound(g_base->assets->SysSound(SysSoundID::kBlip));
  transition_start_ = g_base->logic->display_time();
}

auto DevConsole::HandleTextEditing(const std::string& text) -> bool {
  assert(g_base->InLogicThread());
  if (state_ == State_::kInactive) {
    return false;
  }

  // Ignore back-tick because we use that key to toggle the console.
  if (text == "`") {
    return false;
  }
  input_string_ += text;
  input_text_dirty_ = true;
  return true;
}

auto DevConsole::HandleKeyRelease(const SDL_Keysym* keysym) -> bool {
  // Always absorb our activate keys.
  if (keysym->sym == kDevConsoleActivateKey1
      || keysym->sym == kDevConsoleActivateKey2) {
    return true;
  }

  // Otherwise absorb *all* key-ups when we're active.
  return state_ != State_::kInactive;
}

void DevConsole::Print(const std::string& s_in) {
  assert(g_base->InLogicThread());
  std::string s = Utils::GetValidUTF8(s_in.c_str(), "cspr");
  last_line_ += s;
  std::vector<std::string> broken_up;
  g_base->text_graphics->BreakUpString(
      last_line_.c_str(), kDevConsoleStringBreakUpSize, &broken_up);

  // Spit out all completed lines and keep the last one as lastline.
  for (size_t i = 0; i < broken_up.size() - 1; i++) {
    lines_.emplace_back(broken_up[i], g_base->logic->display_time());
    if (lines_.size() > kDevConsoleLineLimit) {
      lines_.pop_front();
    }
  }
  last_line_ = broken_up[broken_up.size() - 1];
  last_line_mesh_dirty_ = true;
}

auto DevConsole::Bottom_() const -> float {
  float bs = PythonConsoleBaseScale_();
  float vh = g_base->graphics->screen_virtual_height();

  float ratio =
      (g_base->logic->display_time() - transition_start_) / kTransitionSeconds;
  float bottom;
  float mini_size = 90.0f * bs;
  if (state_ == State_::kMini) {
    bottom = vh - mini_size;
  } else {
    bottom = vh - vh * kDevConsoleSize;
  }
  if (g_base->logic->display_time() - transition_start_ < kTransitionSeconds) {
    float from_height;
    if (state_prev_ == State_::kMini) {
      from_height = vh - mini_size;
    } else if (state_prev_ == State_::kFull) {
      from_height = vh - vh * kDevConsoleSize;
    } else {
      from_height = vh;
    }
    float to_height;
    if (state_ == State_::kMini) {
      to_height = vh - mini_size;
    } else if (state_ == State_::kFull) {
      to_height = vh - vh * kDevConsoleSize;
    } else {
      to_height = vh;
    }
    bottom = to_height * ratio + from_height * (1.0 - ratio);
  }
  return bottom;
}

void DevConsole::Draw(FrameDef* frame_def) {
  float bs = PythonConsoleBaseScale_();
  RenderPass* pass = frame_def->overlay_front_pass();

  // If we're not yet transitioning in for the first time OR have completed
  // transitioning out, do nothing.
  if (transition_start_ <= 0.0
      || (state_ == State_::kInactive
          && ((g_base->logic->display_time() - transition_start_)
              >= kTransitionSeconds))) {
    return;
  }

  float bottom = Bottom_();

  float border_height{3.0f};
  {
    bg_mesh_.SetPositionAndSize(0, bottom, kDevConsoleZDepth,
                                pass->virtual_width(),
                                (pass->virtual_height() - bottom));
    stripe_mesh_.SetPositionAndSize(0, bottom + 15.0f * bs, kDevConsoleZDepth,
                                    pass->virtual_width(), 15.0f * bs);
    border_mesh_.SetPositionAndSize(0, bottom - border_height * bs,
                                    kDevConsoleZDepth, pass->virtual_width(),
                                    border_height * bs);
    SimpleComponent c(pass);
    c.SetTransparent(true);
    c.SetColor(0, 0, 0.1f, 0.9f);
    c.DrawMesh(&bg_mesh_);
    c.Submit();
    if (active_tab_ == "Python") {
      c.SetColor(1.0f, 1.0f, 1.0f, 0.1f);
      c.DrawMesh(&stripe_mesh_);
      c.Submit();
    }
    c.SetColor(0.25f, 0.2f, 0.3f, 1.0f);
    c.DrawMesh(&border_mesh_);
    c.Submit();
  }

  // Drop shadow.
  {
    SimpleComponent c(pass);
    c.SetTransparent(true);
    c.SetColor(0.03, 0, 0.09, 0.9f);
    c.SetTexture(g_base->assets->SysTexture(SysTextureID::kSoftRectVertical));
    {
      auto scissor = c.ScopedScissor({0.0f, 0.0f, pass->virtual_width(),
                                      bottom - (border_height * 0.95f) * bs});
      auto xf = c.ScopedTransform();
      c.Translate(pass->virtual_width() * 0.5f, bottom + 160.0f);
      c.Scale(pass->virtual_width() * 1.2f, 600.0f);
      c.DrawMeshAsset(g_base->assets->SysMesh(SysMeshID::kImage1x1));
    }
  }

  if (active_tab_ == "Python") {
    if (input_text_dirty_) {
      input_text_group_.SetText(input_string_);
      input_text_dirty_ = false;
      last_input_text_change_time_ = pass->frame_def()->real_time();
    }
    {
      SimpleComponent c(pass);
      c.SetFlatness(1.0f);
      c.SetTransparent(true);
      c.SetColor(0.5f, 0.5f, 0.7f, 0.8f);
      int elem_count = built_text_group_.GetElementCount();
      for (int e = 0; e < elem_count; e++) {
        c.SetTexture(built_text_group_.GetElementTexture(e));
        {
          auto xf = c.ScopedTransform();
          c.Translate(pass->virtual_width() - 115.0f * bs, bottom + 4.0f,
                      kDevConsoleZDepth);
          c.Scale(0.35f * bs, 0.35f * bs, 1.0f);
          c.DrawMesh(built_text_group_.GetElementMesh(e));
        }
      }
      elem_count = title_text_group_.GetElementCount();
      for (int e = 0; e < elem_count; e++) {
        c.SetTexture(title_text_group_.GetElementTexture(e));
        {
          auto xf = c.ScopedTransform();
          c.Translate(10.0f * bs, bottom + 4.0f, kDevConsoleZDepth);
          c.Scale(0.35f * bs, 0.35f * bs, 1.0f);
          c.DrawMesh(title_text_group_.GetElementMesh(e));
        }
      }
      elem_count = prompt_text_group_.GetElementCount();
      for (int e = 0; e < elem_count; e++) {
        c.SetTexture(prompt_text_group_.GetElementTexture(e));
        c.SetColor(1, 1, 1, 1);
        {
          auto xf = c.ScopedTransform();
          c.Translate(5.0f * bs, bottom + 14.5f * bs, kDevConsoleZDepth);
          c.Scale(0.5f * bs, 0.5f * bs, 1.0f);
          c.DrawMesh(prompt_text_group_.GetElementMesh(e));
        }
      }
      elem_count = input_text_group_.GetElementCount();
      for (int e = 0; e < elem_count; e++) {
        c.SetTexture(input_text_group_.GetElementTexture(e));
        {
          auto xf = c.ScopedTransform();
          c.Translate(15.0f * bs, bottom + 14.5f * bs, kDevConsoleZDepth);
          c.Scale(0.5f * bs, 0.5f * bs, 1.0f);
          c.DrawMesh(input_text_group_.GetElementMesh(e));
        }
      }
      c.Submit();
    }

    // Carat.
    millisecs_t real_time = pass->frame_def()->real_time();
    if (real_time % 200 < 100
        || (real_time - last_input_text_change_time_ < 100)) {
      SimpleComponent c(pass);
      c.SetTransparent(true);
      c.SetColor(1, 1, 1, 0.7f);
      {
        auto xf = c.ScopedTransform();
        c.Translate(
            (19.0f
             + g_base->text_graphics->GetStringWidth(input_string_) * 0.5f)
                * bs,
            bottom + 22.5f * bs, kDevConsoleZDepth);
        c.Scale(6.0f * bs, 12.0f * bs, 1.0f);
        c.DrawMeshAsset(g_base->assets->SysMesh(SysMeshID::kImage1x1));
      }
      c.Submit();
    }

    // Draw output lines.
    {
      float draw_scale = 0.6f;
      float v_inc = 18.0f;
      SimpleComponent c(pass);
      c.SetTransparent(true);
      c.SetColor(1, 1, 1, 1);
      c.SetFlatness(1.0f);
      float h = 0.5f
                * (g_base->graphics->screen_virtual_width()
                   - (kDevConsoleStringBreakUpSize * draw_scale));
      float v = bottom + 32.0f * bs;
      if (!last_line_.empty()) {
        if (last_line_mesh_dirty_) {
          if (!last_line_mesh_group_.Exists()) {
            last_line_mesh_group_ = Object::New<TextGroup>();
          }
          last_line_mesh_group_->SetText(last_line_);
          last_line_mesh_dirty_ = false;
        }
        int elem_count = last_line_mesh_group_->GetElementCount();
        for (int e = 0; e < elem_count; e++) {
          c.SetTexture(last_line_mesh_group_->GetElementTexture(e));
          {
            auto xf = c.ScopedTransform();
            c.Translate(h, v + 2, kDevConsoleZDepth);
            c.Scale(draw_scale, draw_scale);
            c.DrawMesh(last_line_mesh_group_->GetElementMesh(e));
          }
        }
        v += v_inc;
      }
      for (auto i = lines_.rbegin(); i != lines_.rend(); i++) {
        int elem_count = i->GetText().GetElementCount();
        for (int e = 0; e < elem_count; e++) {
          c.SetTexture(i->GetText().GetElementTexture(e));
          {
            auto xf = c.ScopedTransform();
            c.Translate(h, v + 2, kDevConsoleZDepth);
            c.Scale(draw_scale, draw_scale);
            c.DrawMesh(i->GetText().GetElementMesh(e));
          }
        }
        v += v_inc;
        if (v > pass->virtual_height() + v_inc) {
          break;
        }
      }
      c.Submit();
    }
  }

  // Tab Buttons.
  {
    for (auto&& button : tab_buttons_) {
      button->Draw(pass, bottom);
    }
  }

  // Buttons.
  {
    for (auto&& button : buttons_) {
      button->Draw(pass, bottom);
    }
  }
}

auto DevConsole::PythonConsoleBaseScale_() const -> float {
  switch (g_base->ui->scale()) {
    case UIScale::kLarge:
      return 1.5f;
    case UIScale::kMedium:
      return 1.75f;
    case UIScale::kSmall:
    case UIScale::kLast:
      return 2.0f;
  }
  FatalError("Unhandled scale.");
  return 1.0f;
}

}  // namespace ballistica::base