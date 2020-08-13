#pragma once

#include <filesystem>
#include <array>

struct Settings {
  std::filesystem::path input_file;
  std::filesystem::path output_file;
  bool crop_outlier{ };
  double resolution{ 72 };
  bool high_quality{ };
  double margin_top{ 5 };
  double margin_bottom{ 5 };
  double margin_right{ 5 };
  double margin_left{ 5 };
  double margin_inner{ };
  double margin_outer{ };
  double max_header_size{ 0 };
  double max_footer_size{ 0 };
};

bool interpret_commandline(Settings& settings, int argc, const char* argv[]);
void print_help_message(const char* argv0);
