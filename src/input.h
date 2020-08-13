#pragma once

#include "settings.h"
#include <vector>

struct Box {
  double llx;
  double lly;
  double urx;
  double ury;
};

struct Page {
  Box bounding_box{ };
  double header{ };
  double footer{ };
  // bounding box, when header and/or footer were removed
  Box bounding_box_no_header{ };
  Box bounding_box_no_footer{ };
  Box bounding_box_no_header_footer{ };
};

std::vector<Page> analyze_pages(const Settings& settings);
