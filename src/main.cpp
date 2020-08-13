
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
  auto pages = analyze_pages(settings);
  if (pages.empty()) {
    std::fprintf(stderr, "reading input file failed\n");
    return 1;
  }

  optimize_boxes(settings, pages);

  output_with_boxes(settings, pages);
  return 0;
}
catch (const std::exception& ex) {
  std::fprintf(stderr, "unhandled exception: %s\n", ex.what());
  return 1;
}
