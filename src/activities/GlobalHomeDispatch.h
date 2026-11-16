#pragma once
// Manager supplies its navigation callback; the activity can first cancel its
// native work without Lua ownership. This is also the host lifecycle seam.
template <typename Activity, typename Navigate>
bool dispatchHomeToActivity(Activity& activity, Navigate navigate) {
  if (activity.handleHomeGesture()) return true;
  navigate();
  return true;
}
