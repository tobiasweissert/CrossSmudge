#pragma once
#include <algorithm>
namespace stickychat {
struct ChatLayout { int gap, button, bubble, status; };
inline ChatLayout chatLayout(int height) {
  height=std::max(0,height);
  const int gap=std::min(6,height/20);
  const int status=std::min(32,height/8);
  const int button=std::min(48,(height-status-5*gap)/3);
  return {gap,button,(height-status-5*gap-3*button)/2,status};
}
}
