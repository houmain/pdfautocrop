
#include "optimize.h"
#include <cmath>
#include <algorithm>

namespace {
  template <typename Pages, typename F>
  void for_each_page(Pages& pages, bool even, F&& function) {
    for (auto i = (even ? 1u : 0u); i < pages.size(); i += 2)
      function(pages[i]);
  }

  template <typename F>
  std::vector<double> extract_component(const std::vector<Page>& pages, bool even, F&& get) {
    auto values = std::vector<double>();
    values.reserve((pages.size() + 1) / 2);
    for_each_page(pages, even, [&](const Page& page) {
      values.push_back(get(page));
    });
    return values;
  }

  std::vector<double> get_bounds_left(const std::vector<Page>& pages, bool even) {
    return extract_component(pages, even,
      [](const Page& page) { return page.bounding_box.llx; });
  }

  std::vector<double> get_bounds_right(const std::vector<Page>& pages, bool even) {
    return extract_component(pages, even,
      [](const Page& page) { return page.bounding_box.urx; });
  }

  std::vector<double> get_headers(const std::vector<Page>& pages, bool even) {
    return extract_component(pages, even,
      [](const Page& page) { return page.header; });
  }

  std::vector<double> get_footers(const std::vector<Page>& pages, bool even) {
    return extract_component(pages, even,
      [](const Page& page) { return page.footer; });
  }

  std::vector<double> remove_zero(std::vector<double> values) {
    values.erase(std::remove(begin(values), end(values), 0), end(values));
    return values;
  }

  double calculate_mean(const std::vector<double>& values) {
    if (values.size() < 1)
      return 0.0;
    auto sum = 0.0;
    for (const auto& value : values)
      sum += value;
    return sum / values.size();
  }

  double calculate_standard_deviation(
      const std::vector<double>& values, const double& mean_value) {
    if (values.size() < 2)
      return 0.0;
    auto square_sum = 0.0;
    const auto square = [](auto v) { return v * v; };
    for (const auto& value : values)
      square_sum += square(mean_value - value);
    return std::sqrt(square_sum / (values.size() - 1));
  }

  struct Distribution {
    double mean;
    double deviation;
  };

  bool is_outlier(double value, const Distribution& distribution) {
    return std::abs(value - distribution.mean) > distribution.deviation;
  }

  Distribution calculate_distribution(const std::vector<double>& values) {
    const auto mean = calculate_mean(values);
    const auto standard_deviation = calculate_standard_deviation(values, mean);
    return { mean, standard_deviation };
  }

  double calculate_common(std::vector<double>&& values) {
    // calculate mean/standard deviation of all values
    const auto distribution = calculate_distribution(values);

    // remove outliers
    values.erase(std::remove_if(begin(values), end(values),
      [&](const auto value) { return is_outlier(value, distribution); }),
      values.end());

    // calculate mean of common values
    return calculate_mean(values);
  }

  void crop_header_footer(std::vector<Page>& pages, bool even) {
    const auto header_mean =
      calculate_common(remove_zero(get_headers(pages, even)));
    const auto footer_mean =
      calculate_common(remove_zero(get_footers(pages, even)));
    const auto max_header_deviation = header_mean / 2;
    const auto max_footer_deviation = footer_mean / 2;

    for_each_page(pages, even, [&](Page& page) {
      const auto has_header = page.header &&
        !is_outlier(page.header, { header_mean, max_header_deviation });
      const auto has_footer = page.footer &&
        !is_outlier(page.footer, { footer_mean, max_footer_deviation });
      if (has_header || has_footer)
        page.bounding_box =
          (has_header && has_footer ? page.bounding_box_no_header_footer :
           has_header ? page.bounding_box_no_header :
           page.bounding_box_no_footer);
    });
  }

  void crop_outlier(std::vector<Page>& pages, bool even) {
    const auto left = calculate_distribution(get_bounds_left(pages, even));
    const auto right = calculate_distribution(get_bounds_right(pages, even));

    // find maximum bounds of common pages
    auto left_min = left.mean;
    auto right_max = right.mean;
    for_each_page(pages, even, [&](const Page& page) {
      if (!is_outlier(page.bounding_box.llx, left))
        left_min = std::min(left_min, page.bounding_box.llx);
      if (!is_outlier(page.bounding_box.urx, right))
        right_max = std::max(right_max, page.bounding_box.urx);
    });

    // clamp outliers to common page
    for_each_page(pages, even, [&](Page& page) {
      if (is_outlier(page.bounding_box.llx, left))
        page.bounding_box.llx = std::max(page.bounding_box.llx, left_min);
      if (is_outlier(page.bounding_box.urx, right))
        page.bounding_box.urx = std::min(page.bounding_box.urx, right_max);
    });
  }

  void apply_margins(const Settings& settings, std::vector<Page>& pages, bool even) {
    for_each_page(pages, even, [&](Page& page) {
      auto& box = page.bounding_box;
      box.llx -= settings.margin_left;
      box.lly -= settings.margin_bottom;
      box.urx += settings.margin_right;
      box.ury += settings.margin_top;
      if (even) {
        box.llx -= settings.margin_inner;
        box.urx += settings.margin_outer;
      }
      else {
        box.llx -= settings.margin_outer;
        box.urx += settings.margin_inner;
      }
    });
  }

  void optimize_boxes(const Settings& settings, std::vector<Page>& pages, bool even) {
    if (settings.crop_footer_size || settings.crop_header_size)
      crop_header_footer(pages, even);

    if (settings.crop_outlier)
      crop_outlier(pages, even);

    apply_margins(settings, pages, even);
  }
} // namespace

void optimize_boxes(const Settings& settings, std::vector<Page>& pages) {
  optimize_boxes(settings, pages, true);
  optimize_boxes(settings, pages, false);
}
