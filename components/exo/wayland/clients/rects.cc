// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a client that produces output in the form of RGBA
// buffers when receiving pointer/touch events. RGB contains the lower
// 24 bits of the event timestamp and A is 0xff.

#include <linux-dmabuf-unstable-v1-client-protocol.h>
#include <presentation-time-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/scoped_generic.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/exo/wayland/clients/client_base.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gl/gl_bindings.h"

namespace exo {
namespace wayland {
namespace clients {
namespace {

// Rotation speed (degrees/second).
const double kRotationSpeed = 360.0;

// Benchmark warmup frames before starting measurement.
const int kBenchmarkWarmupFrames = 10;

using EventTimeStack = std::vector<uint32_t>;

void PointerEnter(void* data,
                  wl_pointer* pointer,
                  uint32_t serial,
                  wl_surface* surface,
                  wl_fixed_t x,
                  wl_fixed_t y) {}

void PointerLeave(void* data,
                  wl_pointer* pointer,
                  uint32_t serial,
                  wl_surface* surface) {}

void PointerMotion(void* data,
                   wl_pointer* pointer,
                   uint32_t time,
                   wl_fixed_t x,
                   wl_fixed_t y) {
  EventTimeStack* stack = static_cast<EventTimeStack*>(data);

  stack->push_back(time);
}

void PointerButton(void* data,
                   wl_pointer* pointer,
                   uint32_t serial,
                   uint32_t time,
                   uint32_t button,
                   uint32_t state) {}

void PointerAxis(void* data,
                 wl_pointer* pointer,
                 uint32_t time,
                 uint32_t axis,
                 wl_fixed_t value) {}

void PointerAxisSource(void* data, wl_pointer* pointer, uint32_t axis_source) {}

void PointerAxisStop(void* data,
                     wl_pointer* pointer,
                     uint32_t time,
                     uint32_t axis) {}

void PointerDiscrete(void* data,
                     wl_pointer* pointer,
                     uint32_t axis,
                     int32_t discrete) {}

void PointerFrame(void* data, wl_pointer* pointer) {}

void TouchDown(void* data,
               wl_touch* touch,
               uint32_t serial,
               uint32_t time,
               wl_surface* surface,
               int32_t id,
               wl_fixed_t x,
               wl_fixed_t y) {}

void TouchUp(void* data,
             wl_touch* touch,
             uint32_t serial,
             uint32_t time,
             int32_t id) {}

void TouchMotion(void* data,
                 wl_touch* touch,
                 uint32_t time,
                 int32_t id,
                 wl_fixed_t x,
                 wl_fixed_t y) {
  EventTimeStack* stack = static_cast<EventTimeStack*>(data);

  stack->push_back(time);
}

void TouchFrame(void* data, wl_touch* touch) {}

void TouchCancel(void* data, wl_touch* touch) {}

struct Schedule {
  uint32_t time = 0;
  bool callback_pending = false;
};

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  Schedule* schedule = static_cast<Schedule*>(data);

  static uint32_t initial_time = time;
  schedule->time = time - initial_time;
  schedule->callback_pending = false;
}

struct Frame {
  ClientBase::Buffer* buffer = nullptr;
  base::TimeDelta wall_time;
  base::TimeDelta cpu_time;
  std::vector<base::TimeTicks> event_times;
  std::unique_ptr<struct wp_presentation_feedback> feedback;
};

struct Presentation {
  base::circular_deque<std::unique_ptr<Frame>> scheduled_frames;
  base::TimeDelta wall_time;
  base::TimeDelta cpu_time;
  base::TimeDelta latency_time;
  uint32_t num_frames_presented = 0;
  uint32_t num_events_presented = 0;
};

void FeedbackSyncOutput(void* data,
                        struct wp_presentation_feedback* presentation_feedback,
                        wl_output* output) {}

void FeedbackPresented(void* data,
                       struct wp_presentation_feedback* presentation_feedback,
                       uint32_t tv_sec_hi,
                       uint32_t tv_sec_lo,
                       uint32_t tv_nsec,
                       uint32_t refresh,
                       uint32_t seq_hi,
                       uint32_t seq_lo,
                       uint32_t flags) {
  Presentation* presentation = static_cast<Presentation*>(data);
  DCHECK_GT(presentation->scheduled_frames.size(), 0u);
  std::unique_ptr<Frame> frame =
      std::move(presentation->scheduled_frames.front());
  presentation->scheduled_frames.pop_front();

  presentation->wall_time += frame->wall_time;
  presentation->cpu_time += frame->cpu_time;
  ++presentation->num_frames_presented;

  int64_t seconds = (static_cast<int64_t>(tv_sec_hi) << 32) + tv_sec_lo;
  int64_t microseconds = seconds * base::Time::kMicrosecondsPerSecond +
                         tv_nsec / base::Time::kNanosecondsPerMicrosecond;
  base::TimeTicks presentation_time =
      base::TimeTicks::FromInternalValue(microseconds);
  for (const auto& event_time : frame->event_times) {
    presentation->latency_time += presentation_time - event_time;
    ++presentation->num_events_presented;
  }
}

void FeedbackDiscarded(void* data,
                       struct wp_presentation_feedback* presentation_feedback) {
  Presentation* presentation = static_cast<Presentation*>(data);
  DCHECK_GT(presentation->scheduled_frames.size(), 0u);
  std::unique_ptr<Frame> frame =
      std::move(presentation->scheduled_frames.front());
  presentation->scheduled_frames.pop_front();
  LOG(WARNING) << "Frame discarded";
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// RectsClient:

class RectsClient : public ClientBase {
 public:
  RectsClient() {}

  // Initialize and run client main loop.
  int Run(const ClientBase::InitParams& params,
          size_t max_frames_pending,
          size_t num_rects,
          size_t num_benchmark_runs,
          base::TimeDelta benchmark_interval,
          bool show_fps_counter);

 private:
  DISALLOW_COPY_AND_ASSIGN(RectsClient);
};

int RectsClient::Run(const ClientBase::InitParams& params,
                     size_t max_frames_pending,
                     size_t num_rects,
                     size_t num_benchmark_runs,
                     base::TimeDelta benchmark_interval,
                     bool show_fps_counter) {
  if (!ClientBase::Init(params))
    return 1;

  EventTimeStack event_times;

  std::unique_ptr<wl_pointer> pointer(
      static_cast<wl_pointer*>(wl_seat_get_pointer(globals_.seat.get())));
  if (!pointer) {
    LOG(ERROR) << "Can't get pointer";
    return 1;
  }

  wl_pointer_listener pointer_listener = {
      PointerEnter,      PointerLeave,    PointerMotion,
      PointerButton,     PointerAxis,     PointerFrame,
      PointerAxisSource, PointerAxisStop, PointerDiscrete};
  wl_pointer_add_listener(pointer.get(), &pointer_listener, &event_times);

  std::unique_ptr<wl_touch> touch(
      static_cast<wl_touch*>(wl_seat_get_touch(globals_.seat.get())));
  if (!touch) {
    LOG(ERROR) << "Can't get touch";
    return 1;
  }
  wl_touch_listener touch_listener = {TouchDown, TouchUp, TouchMotion,
                                      TouchFrame, TouchCancel};
  wl_touch_add_listener(touch.get(), &touch_listener, &event_times);

  Schedule schedule;
  std::unique_ptr<wl_callback> frame_callback;
  wl_callback_listener frame_listener = {FrameCallback};

  Presentation presentation;
  base::circular_deque<std::unique_ptr<Frame>> pending_frames;

  size_t num_benchmark_runs_left = num_benchmark_runs;
  base::TimeTicks benchmark_start_time;
  std::string fps_counter_text("??");

  wp_presentation_feedback_listener feedback_listener = {
      FeedbackSyncOutput, FeedbackPresented, FeedbackDiscarded};

  SkPaint text_paint;
  text_paint.setTextSize(32.0f);
  text_paint.setColor(SK_ColorWHITE);
  text_paint.setStyle(SkPaint::kFill_Style);

  int dispatch_status = 0;
  do {
    bool enqueue_frame = schedule.callback_pending
                             ? pending_frames.size() < max_frames_pending
                             : pending_frames.empty();
    if (enqueue_frame) {
      auto buffer_it =
          std::find_if(buffers_.begin(), buffers_.end(),
                       [](const std::unique_ptr<ClientBase::Buffer>& buffer) {
                         return !buffer->busy;
                       });
      if (buffer_it == buffers_.end()) {
        LOG(ERROR) << "Can't find free buffer";
        return 1;
      }
      auto* buffer = buffer_it->get();

      auto frame = std::make_unique<Frame>();
      frame->buffer = buffer;

      base::TimeTicks wall_time_start;
      base::ThreadTicks cpu_time_start;
      if (num_benchmark_runs || show_fps_counter) {
        wall_time_start = base::TimeTicks::Now();
        if (presentation.num_frames_presented <= kBenchmarkWarmupFrames)
          benchmark_start_time = wall_time_start;

        base::TimeDelta benchmark_time = wall_time_start - benchmark_start_time;
        if (benchmark_time > benchmark_interval) {
          uint32_t benchmark_frames =
              presentation.num_frames_presented - kBenchmarkWarmupFrames;
          if (num_benchmark_runs_left) {
            // Print benchmark statistics for the frames presented and exit.
            std::cout << benchmark_frames << '\t'
                      << benchmark_time.InMilliseconds() << '\t'
                      << presentation.wall_time.InMilliseconds() << '\t'
                      << presentation.cpu_time.InMilliseconds() << '\t'
                      << presentation.num_events_presented << '\t'
                      << presentation.latency_time.InMilliseconds() << '\t'
                      << std::endl;
            if (!--num_benchmark_runs_left)
              return 0;
          }

          // Set FPS counter text in case it's being shown.
          fps_counter_text = base::UintToString(
              std::round(benchmark_frames / benchmark_interval.InSecondsF()));

          benchmark_start_time = wall_time_start;
          presentation.wall_time = base::TimeDelta();
          presentation.cpu_time = base::TimeDelta();
          presentation.latency_time = base::TimeDelta();
          presentation.num_frames_presented = kBenchmarkWarmupFrames;
          presentation.num_events_presented = 0;
        }

        cpu_time_start = base::ThreadTicks::Now();
      }

      SkCanvas* canvas = buffer->sk_surface->getCanvas();
      if (event_times.empty()) {
        canvas->clear(transparent_background_ ? SK_ColorTRANSPARENT
                                              : SK_ColorBLACK);
      } else {
        // Split buffer into one horizontal rectangle for each event received
        // since last frame. Latest event at the top.
        int y = 0;
        // Note: Rounding up to ensure we cover the whole canvas.
        int h =
            (size_.height() + (event_times.size() / 2)) / event_times.size();
        while (!event_times.empty()) {
          SkIRect rect = SkIRect::MakeXYWH(0, y, size_.width(), h);
          SkPaint paint;
          paint.setColor(SkColorSetRGB((event_times.back() & 0x0000ff) >> 0,
                                       (event_times.back() & 0x00ff00) >> 8,
                                       (event_times.back() & 0xff0000) >> 16));
          canvas->drawIRect(rect, paint);
          std::string text = base::UintToString(event_times.back());
          canvas->drawText(text.c_str(), text.length(), 8, y + 32, text_paint);
          frame->event_times.push_back(base::TimeTicks::FromInternalValue(
              event_times.back() * base::Time::kMicrosecondsPerMillisecond));
          event_times.pop_back();
          y += h;
        }
      }

      // Draw rotating rects.
      SkScalar half_width = SkScalarHalf(size_.width());
      SkScalar half_height = SkScalarHalf(size_.height());
      SkIRect rect = SkIRect::MakeXYWH(-SkScalarHalf(half_width),
                                       -SkScalarHalf(half_height), half_width,
                                       half_height);
      SkScalar rotation = schedule.time * kRotationSpeed / 1000;
      canvas->save();
      canvas->translate(half_width, half_height);
      for (size_t i = 0; i < num_rects; ++i) {
        const SkColor kColors[] = {SK_ColorBLUE, SK_ColorGREEN,
                                   SK_ColorRED,  SK_ColorYELLOW,
                                   SK_ColorCYAN, SK_ColorMAGENTA};
        SkPaint paint;
        paint.setColor(SkColorSetA(kColors[i % arraysize(kColors)], 0xA0));
        canvas->rotate(rotation / num_rects);
        canvas->drawIRect(rect, paint);
      }
      canvas->restore();

      // Draw FPS counter.
      if (show_fps_counter) {
        canvas->drawText(fps_counter_text.c_str(), fps_counter_text.length(),
                         size_.width() - 48, 32, text_paint);
      }
      GrContext* gr_context = gr_context_.get();
      if (gr_context) {
        gr_context->flush();

#if defined(USE_GBM)
        if (egl_sync_type_) {
          buffer->egl_sync.reset(new ScopedEglSync(eglCreateSyncKHR(
              eglGetCurrentDisplay(), egl_sync_type_, nullptr)));
          DCHECK(buffer->egl_sync->is_valid());
        }
#endif

        glFlush();
      }

      buffer->busy = true;

      if (num_benchmark_runs) {
        frame->wall_time = base::TimeTicks::Now() - wall_time_start;
        frame->cpu_time = base::ThreadTicks::Now() - cpu_time_start;
      }
      pending_frames.push_back(std::move(frame));
      continue;
    }

    if (!schedule.callback_pending) {
      DCHECK_GT(pending_frames.size(), 0u);
      std::unique_ptr<Frame> frame = std::move(pending_frames.front());
      pending_frames.pop_front();

      wl_surface* surface = surface_.get();
      wl_surface_set_buffer_scale(surface, scale_);
      wl_surface_set_buffer_transform(surface_.get(), transform_);
      wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                        surface_size_.height());
      wl_surface_attach(surface, frame->buffer->buffer.get(), 0, 0);

#if defined(USE_GBM)
      if (frame->buffer->egl_sync) {
        eglClientWaitSyncKHR(eglGetCurrentDisplay(),
                             frame->buffer->egl_sync->get(),
                             EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER_KHR);
      }
#endif

      frame_callback.reset(wl_surface_frame(surface));
      wl_callback_add_listener(frame_callback.get(), &frame_listener,
                               &schedule);
      schedule.callback_pending = true;

      frame->feedback.reset(
          wp_presentation_feedback(globals_.presentation.get(), surface));
      wp_presentation_feedback_add_listener(frame->feedback.get(),
                                            &feedback_listener, &presentation);
      presentation.scheduled_frames.push_back(std::move(frame));

      wl_surface_commit(surface);
      wl_display_flush(display_.get());
      continue;
    }

    dispatch_status = wl_display_dispatch(display_.get());
  } while (dispatch_status != -1);
  return 0;
}
}  // namespace clients
}  // namespace wayland
}  // namespace exo

namespace switches {

// Specifies the maximum number of pending frames.
const char kMaxFramesPending[] = "max-frames-pending";

// Specifies the number of rotating rects to draw.
const char kNumRects[] = "num-rects";

// Enables benchmark mode and specifies the number of benchmark runs to
// perform before client will exit. Client will print the results to
// standard output as a tab seperated list.
//
//  The output format is:
//   "frames wall-time-ms cpu-time-ms"
const char kBenchmark[] = "benchmark";

// Specifies the number of milliseconds to use as benchmark interval.
const char kBenchmarkInterval[] = "benchmark-interval";

// Specifies if FPS counter should be shown.
const char kShowFpsCounter[] = "show-fps-counter";

}  // namespace switches

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  exo::wayland::clients::ClientBase::InitParams params;
  if (!params.FromCommandLine(*command_line))
    return 1;

  size_t max_frames_pending = 0;
  if (command_line->HasSwitch(switches::kMaxFramesPending) &&
      (!base::StringToSizeT(
          command_line->GetSwitchValueASCII(switches::kMaxFramesPending),
          &max_frames_pending))) {
    LOG(ERROR) << "Invalid value for " << switches::kMaxFramesPending;
    return 1;
  }

  size_t num_rects = 1;
  if (command_line->HasSwitch(switches::kNumRects) &&
      !base::StringToSizeT(
          command_line->GetSwitchValueASCII(switches::kNumRects), &num_rects)) {
    LOG(ERROR) << "Invalid value for " << switches::kNumRects;
    return 1;
  }

  size_t num_benchmark_runs = 0;
  if (command_line->HasSwitch(switches::kBenchmark) &&
      (!base::StringToSizeT(
          command_line->GetSwitchValueASCII(switches::kBenchmark),
          &num_benchmark_runs))) {
    LOG(ERROR) << "Invalid value for " << switches::kBenchmark;
    return 1;
  }

  size_t benchmark_interval_ms = 5000;  // 5 seconds.
  if (command_line->HasSwitch(switches::kBenchmarkInterval) &&
      (!base::StringToSizeT(
          command_line->GetSwitchValueASCII(switches::kBenchmarkInterval),
          &benchmark_interval_ms))) {
    LOG(ERROR) << "Invalid value for " << switches::kBenchmarkInterval;
    return 1;
  }

  exo::wayland::clients::RectsClient client;
  return client.Run(params, max_frames_pending, num_rects, num_benchmark_runs,
                    base::TimeDelta::FromMilliseconds(benchmark_interval_ms),
                    command_line->HasSwitch(switches::kShowFpsCounter));
}
