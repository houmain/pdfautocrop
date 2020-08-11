#pragma once

#include "settings.h"
#include <vector>

struct Box {
  double llx;
  double lly;
  double urx;
  double ury;
};

std::vector<Box> calculate_page_boxes(const Settings& settings);
