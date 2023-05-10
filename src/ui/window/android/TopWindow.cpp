// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "../TopWindow.hpp"
#include "ui/canvas/custom/Cache.hpp"
#include "ui/canvas/custom/TopCanvas.hpp"
#include "ui/event/Queue.hpp"
#include "ui/event/Globals.hpp"
#include "Android/Main.hpp"
#include "Android/NativeView.hpp"
#include "LogFile.hpp"

#include <cassert>

namespace UI {

TopWindow::TopWindow(UI::Display &_display) noexcept
  :display(_display)
{
  native_view->SetPointer(Java::GetEnv(), this);
}

void
TopWindow::AnnounceResize(PixelSize _new_size) noexcept
{
  const std::lock_guard lock{paused_mutex};
  resized = true;
  new_size = _new_size;
}

bool
TopWindow::ResumeSurface() noexcept
{
  /* Try to reinitialize OpenGL.  This often fails on the first
     attempt (IllegalArgumentException "Make sure the SurfaceView or
     associated SurfaceHolder has a valid Surface"), therefore we're
     trying again until we're successful. */

  assert(!should_pause);

  try {
    if (!screen->AcquireSurface())
      return false;
  } catch (...) {
    LogError(std::current_exception(), "Failed to initialize GL surface");
    return false;
  }

  assert(screen->IsReady());

  should_resume = false;

  RefreshSize();

  return true;
}

bool
TopWindow::CheckResumeSurface() noexcept
{
  return !should_pause && (!should_resume || ResumeSurface()) && screen->IsReady();
}

void
TopWindow::RefreshSize() noexcept
{
  PixelSize new_size_copy;

  {
    const std::lock_guard lock{paused_mutex};
    if (!resized)
      return;

    resized = false;
    new_size_copy = new_size;
  }

  if (screen->CheckResize(new_size_copy))
    Resize(new_size_copy);
}

void
TopWindow::OnResize(PixelSize new_size) noexcept
{
  if (native_view != nullptr)
    native_view->SetSize(new_size.width, new_size.height);

  ContainerWindow::OnResize(new_size);
}

void
TopWindow::OnPause() noexcept
{
  TextCache::Flush();

  screen->ReleaseSurface();

  assert(!screen->IsReady());

  const std::lock_guard lock{paused_mutex};
  if (!should_pause)
    return;

  should_pause = false;
  should_resume = false;
  paused_cond.notify_one();
}

void
TopWindow::OnResume() noexcept
{
  /* tell TopWindow::Expose() to reinitialize OpenGL */
  should_resume = true;

  /* schedule a redraw */
  Invalidate();
}

static bool
match_pause_and_resume(const Event &event, [[maybe_unused]] void *ctx) noexcept
{
  return event.type == Event::PAUSE || event.type == Event::RESUME;
}

void
TopWindow::Pause() noexcept
{
  {
    const std::lock_guard lock{paused_mutex};
    should_pause = true;
  }

  event_queue->Purge(match_pause_and_resume, nullptr);
  event_queue->Inject(Event::PAUSE);

  std::unique_lock lock{paused_mutex};
  paused_cond.wait(lock, [this]{ return !running || !should_pause; });
}

void
TopWindow::Resume() noexcept
{
  event_queue->Purge(match_pause_and_resume, nullptr);
  event_queue->Inject(Event::RESUME);
}

bool
TopWindow::OnEvent(const Event &event)
{
  switch (event.type) {
    Window *w;

  case Event::NOP:
  case Event::TIMER:
  case Event::CALLBACK:
    break;

  case Event::KEY_DOWN:
    w = GetFocusedWindow();
    if (w == nullptr)
      w = this;

    return w->OnKeyDown(event.param);

  case Event::KEY_UP:
    w = GetFocusedWindow();
    if (w == nullptr)
      w = this;

    return w->OnKeyUp(event.param);

  case Event::MOUSE_MOTION:
    // XXX keys
    return OnMouseMove(event.point, 0);

  case Event::MOUSE_DOWN:
    return double_click.Check(event.point)
      ? OnMouseDouble(event.point)
      : OnMouseDown(event.point);

  case Event::MOUSE_UP:
    double_click.Moved(event.point);

    return OnMouseUp(event.point);

  case Event::MOUSE_WHEEL:
    return OnMouseWheel(event.point, (int)event.param);

  case Event::POINTER_DOWN:
    return OnMultiTouchDown();

  case Event::POINTER_UP:
    return OnMultiTouchUp();

  case Event::RESIZE:
    if (!screen->IsReady())
      /* postpone the resize if we're paused; the real resize will be
         handled by TopWindow::refresh() as soon as XCSoar is
         resumed */
      return true;

    if (screen->CheckResize(PixelSize(event.point.x, event.point.y)))
      Resize(screen->GetSize());

    /* it seems the first page flip after a display orientation change
       is ignored on Android (tested on a Dell Streak / Android
       2.2.2); let's do one dummy call before we really draw
       something */
    screen->Flip();
    return true;

  case Event::PAUSE:
    OnPause();
    return true;

  case Event::RESUME:
    OnResume();
    return true;
  }

  return false;
}

void
TopWindow::BeginRunning() noexcept
{
  const std::lock_guard lock{paused_mutex};
  assert(!running);
  running = true;
}

void
TopWindow::EndRunning() noexcept
{
  const std::lock_guard lock{paused_mutex};
  assert(running);
  running = false;
  /* wake up the Android Activity thread, just in case it's waiting
     inside Pause() */
  paused_cond.notify_one();
}

} // namespace UI
