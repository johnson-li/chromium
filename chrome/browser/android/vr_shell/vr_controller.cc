// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_controller.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/math_constants.h"
#include "base/numerics/ranges.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_controller.h"
#include "ui/gfx/transform.h"

namespace vr_shell {

namespace {

constexpr float kDisplacementScaleFactor = 300.0f;

// A slop represents a small rectangular region around the first touch point of
// a gesture.
// If the user does not move outside of the slop, no gesture is detected.
// Gestures start to be detected when the user moves outside of the slop.
// Vertical distance from the border to the center of slop.
constexpr float kSlopVertical = 0.165f;

// Horizontal distance from the border to the center of slop.
constexpr float kSlopHorizontal = 0.15f;

// Minimum distance needed in at least one direction to call two vectors
// not equal. Also, minimum time distance needed to call two timestamps
// not equal.
constexpr float kDelta = 1.0e-7f;

constexpr float kCutoffHz = 10.0f;
constexpr float kRC = 1.0f / (2.0f * base::kPiFloat * kCutoffHz);
constexpr float kNanoSecondsPerSecond = 1.0e9f;

constexpr int kMaxNumOfExtrapolations = 2;

// Distance from the center of the controller to start rendering the laser.
constexpr float kLaserStartDisplacement = 0.045;

constexpr float kFadeDistanceFromFace = 0.34f;
constexpr float kDeltaAlpha = 3.0f;

void ClampTouchpadPosition(gfx::Vector2dF* position) {
  position->set_x(base::ClampToRange(position->x(), 0.0f, 1.0f));
  position->set_y(base::ClampToRange(position->y(), 0.0f, 1.0f));
}

float DeltaTimeSeconds(int64_t last_timestamp_nanos) {
  return (gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos -
          last_timestamp_nanos) /
         kNanoSecondsPerSecond;
}

}  // namespace

VrController::VrController(gvr_context* gvr_context) {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  CHECK(gvr_context != nullptr) << "invalid gvr_context";
  controller_api_ = base::MakeUnique<gvr::ControllerApi>();
  controller_state_ = base::MakeUnique<gvr::ControllerState>();
  gvr_api_ = gvr::GvrApi::WrapNonOwned(gvr_context);

  int32_t options = gvr::ControllerApi::DefaultOptions();

  options |= GVR_CONTROLLER_ENABLE_ARM_MODEL;

  // Enable non-default options - WebVR needs gyro and linear acceleration, and
  // since VrShell implements GvrGamepadDataProvider we need this always.
  options |= GVR_CONTROLLER_ENABLE_GYRO;
  options |= GVR_CONTROLLER_ENABLE_ACCEL;

  CHECK(controller_api_->Init(options, gvr_context));
  controller_api_->Resume();

  handedness_ = gvr_api_->GetUserPrefs().GetControllerHandedness();

  Reset();
  last_timestamp_nanos_ =
      gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos;
}

VrController::~VrController() {
  DVLOG(1) << __FUNCTION__ << "=" << this;
}

void VrController::OnResume() {
  if (controller_api_) {
    controller_api_->Resume();
    handedness_ = gvr_api_->GetUserPrefs().GetControllerHandedness();
  }
}

void VrController::OnPause() {
  if (controller_api_)
    controller_api_->Pause();
}

device::GvrGamepadData VrController::GetGamepadData() {
  device::GvrGamepadData pad = {};
  pad.connected = IsConnected();
  pad.timestamp = controller_state_->GetLastOrientationTimestamp();

  if (pad.connected) {
    pad.touch_pos.set_x(TouchPosX());
    pad.touch_pos.set_y(TouchPosY());
    pad.orientation = Orientation();

    // Use orientation to rotate acceleration/gyro into seated space.
    gfx::Transform pose_mat(Orientation());
    const gvr::Vec3f& accel = controller_state_->GetAccel();
    const gvr::Vec3f& gyro = controller_state_->GetGyro();
    pad.accel = gfx::Vector3dF(accel.x, accel.y, accel.z);
    pose_mat.TransformVector(&pad.accel);
    pad.gyro = gfx::Vector3dF(gyro.x, gyro.y, gyro.z);
    pose_mat.TransformVector(&pad.gyro);

    pad.is_touching = controller_state_->IsTouching();
    pad.controller_button_pressed =
        controller_state_->GetButtonState(GVR_CONTROLLER_BUTTON_CLICK);
    pad.right_handed = handedness_ == GVR_CONTROLLER_RIGHT_HANDED;
  }

  return pad;
}

bool VrController::IsTouching() {
  return controller_state_->IsTouching();
}

float VrController::TouchPosX() {
  return controller_state_->GetTouchPos().x;
}

float VrController::TouchPosY() {
  return controller_state_->GetTouchPos().y;
}

base::TimeTicks VrController::GetLastOrientationTimestamp() const {
  // controller_state_->GetLast*Timestamp() returns timestamps in a
  // different timebase from base::TimeTicks::Now(), so we can't use the
  // timestamps in any meaningful way in the rest of Chrome.
  // TODO(mthiesse): Use controller_state_->GetLastOrientationTimestamp() when
  // b/62818778 is resolved.
  return base::TimeTicks::Now();
}

base::TimeTicks VrController::GetLastTouchTimestamp() const {
  // TODO(mthiesse): Use controller_state_->GetLastTouchTimestamp() when
  // b/62818778 is resolved.
  return base::TimeTicks::Now();
}

base::TimeTicks VrController::GetLastButtonTimestamp() const {
  // TODO(mthiesse): Use controller_state_->GetLastButtonTimestamp() when
  // b/62818778 is resolved.
  return base::TimeTicks::Now();
}

gfx::Quaternion VrController::Orientation() const {
  const gvr::Quatf& orientation = controller_state_->GetOrientation();
  return gfx::Quaternion(orientation.qx, orientation.qy, orientation.qz,
                         orientation.qw);
}

gfx::Point3F VrController::Position() const {
  gvr::Vec3f position = controller_state_->GetPosition();
  return gfx::Point3F(position.x, position.y, position.z);
}

void VrController::GetTransform(gfx::Transform* out) const {
  *out = gfx::Transform(Orientation());
  gvr::Vec3f position = controller_state_->GetPosition();
  out->matrix().postTranslate(position.x, position.y, position.z);
}

float VrController::GetOpacity() const {
  return alpha_value_;
}

gfx::Point3F VrController::GetPointerStart() const {
  gfx::Vector3dF pointer_direction{0.0f, -sin(kErgoAngleOffset),
                                   -cos(kErgoAngleOffset)};
  gfx::Transform rotation_mat(Orientation());
  rotation_mat.TransformVector(&pointer_direction);
  return Position() +
         gfx::ScaleVector3d(pointer_direction, kLaserStartDisplacement);
}

vr::VrControllerModel::State VrController::GetModelState() const {
  if (ButtonState(gvr::ControllerButton::GVR_CONTROLLER_BUTTON_CLICK))
    return vr::VrControllerModel::TOUCHPAD;
  if (ButtonState(gvr::ControllerButton::GVR_CONTROLLER_BUTTON_APP))
    return vr::VrControllerModel::APP;
  if (ButtonState(gvr::ControllerButton::GVR_CONTROLLER_BUTTON_HOME))
    return vr::VrControllerModel::SYSTEM;
  return vr::VrControllerModel::IDLE;
}

bool VrController::TouchDownHappened() {
  return controller_state_->GetTouchDown();
}

bool VrController::TouchUpHappened() {
  return controller_state_->GetTouchUp();
}

bool VrController::ButtonDownHappened(gvr::ControllerButton button) {
  return controller_state_->GetButtonDown(button);
}

bool VrController::ButtonUpHappened(gvr::ControllerButton button) {
  return controller_state_->GetButtonUp(button);
}

bool VrController::ButtonState(gvr::ControllerButton button) const {
  return controller_state_->GetButtonState(button);
}

bool VrController::IsConnected() {
  return controller_state_->GetConnectionState() == gvr::kControllerConnected;
}

void VrController::UpdateState(const gvr::Mat4f& head_direction) {
  controller_api_->ApplyArmModel(handedness_, gvr::kArmModelBehaviorFollowGaze,
                                 head_direction);
  const int32_t old_status = controller_state_->GetApiStatus();
  const int32_t old_connection_state = controller_state_->GetConnectionState();
  // Read current controller state.
  controller_state_->Update(*controller_api_);
  // Print new API status and connection state, if they changed.
  if (controller_state_->GetApiStatus() != old_status ||
      controller_state_->GetConnectionState() != old_connection_state) {
    VLOG(1) << "Controller Connection status: "
            << gvr_controller_connection_state_to_string(
                   controller_state_->GetConnectionState());
  }
  UpdateAlpha();
}

void VrController::UpdateTouchInfo() {
  CHECK(touch_info_ != nullptr) << "touch_info_ not initialized properly.";
  if (IsTouching() && state_ == SCROLLING &&
      (controller_state_->GetLastTouchTimestamp() == last_touch_timestamp_ ||
       (cur_touch_point_->position == prev_touch_point_->position &&
        extrapolated_touch_ < kMaxNumOfExtrapolations))) {
    extrapolated_touch_++;
    touch_position_changed_ = true;
    // vr::Fill the touch_info
    float duration = DeltaTimeSeconds(last_timestamp_nanos_);
    touch_info_->touch_point.position.set_x(cur_touch_point_->position.x() +
                                            overall_velocity_.x() * duration);
    touch_info_->touch_point.position.set_y(cur_touch_point_->position.y() +
                                            overall_velocity_.y() * duration);
  } else {
    if (extrapolated_touch_ == kMaxNumOfExtrapolations) {
      overall_velocity_ = {0, 0};
    }
    extrapolated_touch_ = 0;
  }
  last_touch_timestamp_ = controller_state_->GetLastTouchTimestamp();
  last_timestamp_nanos_ =
      gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos;
}

std::unique_ptr<GestureList> VrController::DetectGestures() {
  std::unique_ptr<GestureList> gesture_list = base::MakeUnique<GestureList>();
  std::unique_ptr<blink::WebGestureEvent> gesture(new blink::WebGestureEvent());

  if (controller_state_->GetConnectionState() != gvr::kControllerConnected) {
    gesture_list->push_back(std::move(gesture));
    return gesture_list;
  }

  touch_position_changed_ = UpdateCurrentTouchpoint();
  UpdateTouchInfo();
  if (touch_position_changed_)
    UpdateOverallVelocity();

  UpdateGestureFromTouchInfo(gesture.get());
  gesture->source_device = blink::kWebGestureDeviceTouchpad;
  gesture_list->push_back(std::move(gesture));

  if (gesture_list->back()->GetType() ==
      blink::WebInputEvent::kGestureScrollEnd) {
    if (!ButtonDownHappened(gvr::kControllerButtonClick) &&
        (last_velocity_.x() != 0.0 || last_velocity_.y() != 0.0)) {
      std::unique_ptr<blink::WebGestureEvent> fling(
          new blink::WebGestureEvent(blink::WebInputEvent::kGestureFlingStart,
                                     blink::WebInputEvent::kNoModifiers,
                                     gesture_list->back()->TimeStampSeconds()));
      fling->source_device = blink::kWebGestureDeviceTouchpad;
      if (IsHorizontalGesture()) {
        fling->data.fling_start.velocity_x =
            last_velocity_.x() * kDisplacementScaleFactor;
      } else {
        fling->data.fling_start.velocity_y =
            last_velocity_.y() * kDisplacementScaleFactor;
      }
      // FlingStart replaces ScrollEnd
      gesture_list->pop_back();
      gesture_list->push_back(std::move(fling));
    }
    Reset();
  }

  return gesture_list;
}

void VrController::UpdateGestureFromTouchInfo(blink::WebGestureEvent* gesture) {
  gesture->SetTimeStampSeconds(
      (GetLastTouchTimestamp() - base::TimeTicks()).InSecondsF());
  switch (state_) {
    // User has not put finger on touch pad.
    case WAITING:
      HandleWaitingState(gesture);
      break;
    // User has not started a gesture (by moving out of slop).
    case TOUCHING:
      HandleDetectingState(gesture);
      break;
    // User is scrolling on touchpad
    case SCROLLING:
      HandleScrollingState(gesture);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void VrController::HandleWaitingState(blink::WebGestureEvent* gesture) {
  // User puts finger on touch pad (or when the touch down for current gesture
  // is missed, initiate gesture from current touch point).
  if (touch_info_->touch_down || touch_info_->is_touching) {
    // update initial touchpoint
    *init_touch_point_ = touch_info_->touch_point;
    // update current touchpoint
    *cur_touch_point_ = touch_info_->touch_point;
    state_ = TOUCHING;

    gesture->SetType(blink::WebInputEvent::kGestureFlingCancel);
    gesture->data.fling_cancel.prevent_boosting = false;
  }
}

void VrController::HandleDetectingState(blink::WebGestureEvent* gesture) {
  // User lifts up finger from touch pad.
  if (touch_info_->touch_up || !(touch_info_->is_touching)) {
    Reset();
    return;
  }

  // Touch position is changed, the touch point moves outside of slop,
  // and the Controller's button is not down.
  if (touch_position_changed_ && touch_info_->is_touching &&
      !InSlop(touch_info_->touch_point.position) &&
      !ButtonState(gvr::kControllerButtonClick)) {
    state_ = SCROLLING;
    gesture->SetType(blink::WebInputEvent::kGestureScrollBegin);
    UpdateGestureParameters();
    gesture->data.scroll_begin.delta_x_hint =
        displacement_.x() * kDisplacementScaleFactor;
    gesture->data.scroll_begin.delta_y_hint =
        displacement_.y() * kDisplacementScaleFactor;
    gesture->data.scroll_begin.delta_hint_units =
        blink::WebGestureEvent::ScrollUnits::kPrecisePixels;
  }
}

void VrController::HandleScrollingState(blink::WebGestureEvent* gesture) {
  if (touch_info_->touch_up || !(touch_info_->is_touching) ||
      ButtonDownHappened(gvr::kControllerButtonClick)) {
    // Gesture ends.
    gesture->SetType(blink::WebInputEvent::kGestureScrollEnd);
    UpdateGestureParameters();
  } else if (touch_position_changed_) {
    // User continues scrolling and there is a change in touch position.
    gesture->SetType(blink::WebInputEvent::kGestureScrollUpdate);
    UpdateGestureParameters();
    if (IsHorizontalGesture()) {
      gesture->data.scroll_update.delta_x =
          displacement_.x() * kDisplacementScaleFactor;
    } else {
      gesture->data.scroll_update.delta_y =
          displacement_.y() * kDisplacementScaleFactor;
    }
    last_velocity_ = overall_velocity_;
  }
}

bool VrController::IsHorizontalGesture() {
  return std::abs(last_velocity_.x()) > std::abs(last_velocity_.y());
}

bool VrController::InSlop(const gfx::Vector2dF touch_position) {
  return (std::abs(touch_position.x() - init_touch_point_->position.x()) <
          kSlopHorizontal) &&
         (std::abs(touch_position.y() - init_touch_point_->position.y()) <
          kSlopVertical);
}

void VrController::Reset() {
  // Reset state.
  state_ = WAITING;

  // Reset the pointers.
  prev_touch_point_.reset(new TouchPoint);
  cur_touch_point_.reset(new TouchPoint);
  init_touch_point_.reset(new TouchPoint);
  touch_info_.reset(new TouchInfo);
  overall_velocity_ = {0, 0};
  last_velocity_ = {0, 0};
}

void VrController::UpdateGestureParameters() {
  displacement_ =
      touch_info_->touch_point.position - prev_touch_point_->position;
}

bool VrController::UpdateCurrentTouchpoint() {
  touch_info_->touch_up = TouchUpHappened();
  touch_info_->touch_down = TouchDownHappened();
  touch_info_->is_touching = IsTouching();
  touch_info_->touch_point.position.set_x(TouchPosX());
  touch_info_->touch_point.position.set_y(TouchPosY());
  ClampTouchpadPosition(&touch_info_->touch_point.position);
  touch_info_->touch_point.timestamp =
      gvr::GvrApi::GetTimePointNow().monotonic_system_time_nanos;

  if (IsTouching() || TouchUpHappened()) {
    // Update the touch point when the touch position has changed.
    if (cur_touch_point_->position != touch_info_->touch_point.position) {
      prev_touch_point_.swap(cur_touch_point_);
      cur_touch_point_.reset(new TouchPoint);
      cur_touch_point_->position = touch_info_->touch_point.position;
      cur_touch_point_->timestamp = touch_info_->touch_point.timestamp;
      return true;
    }
  }
  return false;
}

void VrController::UpdateOverallVelocity() {
  float duration =
      (touch_info_->touch_point.timestamp - prev_touch_point_->timestamp) /
      kNanoSecondsPerSecond;

  // If the timestamp does not change, do not update velocity.
  if (duration < kDelta)
    return;

  const gfx::Vector2dF& displacement =
      touch_info_->touch_point.position - prev_touch_point_->position;

  const gfx::Vector2dF& velocity = ScaleVector2d(displacement, (1 / duration));

  float weight = duration / (kRC + duration);

  overall_velocity_ = ScaleVector2d(overall_velocity_, (1 - weight)) +
                      ScaleVector2d(velocity, weight);
}

void VrController::UpdateAlpha() {
  float distance_to_face = (Position() - gfx::Point3F()).Length();
  float alpha_change = kDeltaAlpha * DeltaTimeSeconds(last_timestamp_nanos_);
  alpha_value_ = base::ClampToRange(distance_to_face < kFadeDistanceFromFace
                                        ? alpha_value_ - alpha_change
                                        : alpha_value_ + alpha_change,
                                    0.0f, 1.0f);
}

}  // namespace vr_shell
