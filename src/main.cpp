
#include "settings.h"
#include "input.h"
#include "optimize.h"
#include "output.h"
#include <cstdio>

int main(int argc, const char* argv[]) try {
  auto settings = Settings{ };
  if (!interpret_commandline(settings, argc, argv)) {
    print_help_message(argv[0]);
    return 1;
  }
  auto boxes = calculate_page_boxes(settings);
  if (boxes.empty())
    return 1;

  optimize_boxes(settings, boxes);

  output_with_boxes(settings, boxes);
  return 0;
}
catch (const std::exception& ex) {
  std::fprintf(stderr, "unhandled exception: %s\n", ex.what());
  return 1;
}
