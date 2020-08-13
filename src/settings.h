#pragma once

#include <filesystem>
#include <array>

struct Settings {
  std::filesystem::path input_file;
  std::filesystem::path output_file;
  double crop_header_size{ };
  double crop_footer_size{ };
  bool crop_outlier{ };
  bool high_quality{ true };
  double resolution{ 96 };
  double margin_top{ 5 };
  double margin_bottom{ 5 };
  double margin_right{ 5 };
  double margin_left{ 5 };
  double margin_inner{ };
  double margin_outer{ };
};

bool interpret_commandline(Settings& settings, int argc, const char* argv[]);
void print_help_message(const char* argv0);
