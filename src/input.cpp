
#include "input.h"
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <poppler/cpp/poppler-page-renderer.h>
#include <thread>
#include <cstring>
#include <algorithm>

#if !defined(NDEBUG)
#  include <fstream>

void dump_pgm(const std::string& filename, const poppler::image& image,
    const std::vector<poppler::rect>& rectangles) {

  auto os = std::ofstream(filename, std::ios::out);
  os << "P2\n" << image.width() << " " << image.height() << "\n" << "255\n";
  const auto bytes_per_row = image.bytes_per_row();
  const auto data = image.const_data();
  for (auto y = 0; y < image.height(); ++y) {
    for (auto x = 0; x < image.width(); ++x) {
      const auto on_rectangle = [&](){
        for (const auto& r : rectangles) {
          if ((x == r.left() || x == r.right()) && y >= r.top() && y <= r.bottom())
            return true;
          if ((y == r.bottom() || y == r.top()) && x >= r.left() && x <= r.right())
            return true;
        }
        return false;
      };
      os << (on_rectangle() ? 127u :
        static_cast<unsigned char>(data[y * bytes_per_row + x])) << " ";
    }
    os << "\n";
  }
}
#endif

namespace {
  using Rect = poppler::rect;
  using Image = poppler::image;

  char guess_background_color(const Image& image) {
    const auto data = image.const_data();
    const auto right = image.width() - 1;
    const auto bottom = (image.height() - 1) * image.bytes_per_row();
    const auto colors = std::array<char, 4>{
      data[0], data[right], data[bottom], data[bottom + right]
    };
    return *std::max_element(begin(colors), end(colors),
      [](char a, char b) {
        return static_cast<unsigned char>(a) < static_cast<unsigned char>(b);
      });
  }

  Rect get_bounds(const Image& image) {
    return { 0, 0, image.width(), image.height() };
  }

  Rect get_used_bounds(const Image& image, const Rect& rect) {
    const auto bytes_per_row = image.bytes_per_row();
    const auto data = image.const_data();
    const auto background_color = guess_background_color(image);
    const auto rect_has_background_color = [&](const Rect& rect) {
      for (auto y = rect.top(); y < rect.bottom(); ++y)
        for (auto x = rect.left(); x < rect.right(); ++x)
          if (data[y * bytes_per_row + x] != background_color)
            return false;
      return true;
    };

    const auto x1 = rect.x() + rect.width() - 1;
    const auto y1 = rect.y() + rect.height() - 1;

    auto min_y = rect.y();
    for (; min_y < y1; ++min_y)
      if (!rect_has_background_color({ rect.x(), min_y, rect.width(), 1 }))
        break;

    auto max_y = y1;
    for (; max_y > min_y; --max_y)
      if (!rect_has_background_color({ rect.x(), max_y, rect.width(), 1 }))
        break;

    auto min_x = rect.x();
    for (; min_x < x1; ++min_x)
      if (!rect_has_background_color({ min_x, min_y, 1, max_y - min_y }))
        break;

    auto max_x = x1;
    for (; max_x > min_x; --max_x)
      if (!rect_has_background_color({ max_x, min_y, 1, max_y - min_y }))
        break;

    return { min_x, min_y, max_x - min_x + 1, max_y - min_y + 1 };
  }

  Rect indent_bounds(const Rect& bounds, int header_size, int footer_size) {
    return Rect{
      bounds.x(),
      bounds.y() + header_size,
      bounds.width(),
      bounds.height() - (header_size + footer_size)
    };
  }

  int guess_header_size(const Image& image, const Rect& page_bounds, int max_size) {
    const auto max_space_within = 5;
    max_size = std::min(max_size, image.height());
    auto header_size = 0;
    for (auto i = 1; i < max_size; ++i) {
      const auto indented_bounds = indent_bounds(page_bounds, i, 0);
      const auto reduced_bounds = get_used_bounds(image, indented_bounds);
      if (indented_bounds.top() != reduced_bounds.top()) {
        if (header_size && i > header_size + max_space_within)
          break;
        header_size = i;
        i = reduced_bounds.top() - page_bounds.top();
      }
    }
    return header_size;
  }

  int guess_footer_size(const Image& image, const Rect& page_bounds, int max_size) {
    const auto max_space_within = 5;
    max_size = std::min(max_size, image.height());
    auto footer_size = 0;
    for (auto i = 1; i < max_size; ++i) {
      const auto indented_bounds = indent_bounds(page_bounds, 0, i);
      const auto reduced_bounds = get_used_bounds(image, indented_bounds);
      if (indented_bounds.bottom() != reduced_bounds.bottom()) {
        if (footer_size && i > footer_size + max_space_within)
          break;
        footer_size = i;
        i = page_bounds.bottom() - reduced_bounds.bottom();
      }
    }
    return footer_size;
  }

  poppler::image transform(poppler::image&& image,
      poppler::page::orientation_enum orientation) {

    const auto w = image.width();
    const auto h = image.height();
    const auto bytes_per_row = image.bytes_per_row();

    if (orientation == poppler::page::landscape) {
      auto rotated = poppler::image(h, w, poppler::image::format_gray8);
      const auto rotated_bytes_per_row = rotated.bytes_per_row();
      for (auto y = 0; y < h; ++y)
        for (auto x = 0; x < w; ++x)
          rotated.data()[x * rotated_bytes_per_row + y] =
            image.const_data()[y * bytes_per_row + (w - 1 - x)];
      return rotated;
    }

    if (orientation == poppler::page::seascape) {
      auto rotated = poppler::image(h, w, poppler::image::format_gray8);
      const auto rotated_bytes_per_row = rotated.bytes_per_row();
      for (auto y = 0; y < h; ++y)
        for (auto x = 0; x < w; ++x)
          rotated.data()[x * rotated_bytes_per_row + (h - 1 - y)] =
            image.const_data()[y * bytes_per_row + x];
      return rotated;
    }

    if (orientation == poppler::page::upside_down) {
      auto upside_down = poppler::image(w, h, poppler::image::format_gray8);
      for (auto y = 0; y < h; ++y)
        for (auto x = 0; x < w; ++x)
          upside_down.data()[(h - 1 - y) * bytes_per_row + (w - 1 - x)] =
            image.const_data()[y * bytes_per_row + x];
      return upside_down;
    }

    return image;
  }
} // namespace

std::vector<Page> analyze_pages(const Settings& settings) {
  // silence errors
  poppler::set_debug_error_function([](const std::string&, void*) { }, nullptr);

  auto document = std::unique_ptr<poppler::document>(
      poppler::document::load_from_file(settings.input_file.u8string()));
  if (!document)
    return { };

  const auto page_count = document->pages();
  auto pages = std::vector<Page>(page_count);

  const auto work = [&](int begin, int end) {
    auto renderer = poppler::page_renderer();
    renderer.set_image_format(Image::format_gray8);
    if (settings.high_quality) {
      renderer.set_render_hint(poppler::page_renderer::antialiasing);
      renderer.set_render_hint(poppler::page_renderer::text_antialiasing);
      renderer.set_render_hint(poppler::page_renderer::text_hinting);
    }

    for (auto i = begin; i < end; ++i) {
      const auto page = std::unique_ptr<poppler::page>(document->create_page(i));
      const auto image = transform(renderer.render_page(page.get(),
        settings.resolution, settings.resolution), page->orientation());

      const auto page_bounds = get_used_bounds(image, get_bounds(image));

      const auto page_width = page->page_rect().width();
      const auto page_height = page->page_rect().height();
      const auto scale_x = page_width / image.width();
      const auto scale_y = page_height / image.height();
      const auto bounds_to_box = [&](const Rect& bounds) {
        return Box{
          bounds.left() * scale_x,
          page_height - bounds.bottom() * scale_y,
          bounds.right() * scale_x,
          page_height - bounds.top() * scale_y
        };
      };

      pages[i].bounding_box = bounds_to_box(page_bounds);

      if (settings.crop_header_size || settings.crop_footer_size) {
        const auto header_size = guess_header_size(image, page_bounds,
          static_cast<int>(settings.crop_header_size / scale_y));
        const auto footer_size = guess_footer_size(image, page_bounds,
          static_cast<int>(settings.crop_footer_size / scale_y));
        pages[i].header = header_size * scale_y;
        pages[i].footer = footer_size * scale_y;
        pages[i].bounding_box_no_header = bounds_to_box(
            get_used_bounds(image, indent_bounds(page_bounds, header_size, 0)));
        pages[i].bounding_box_no_footer = bounds_to_box(
            get_used_bounds(image, indent_bounds(page_bounds, 0, footer_size)));
        pages[i].bounding_box_no_header_footer = bounds_to_box(
            get_used_bounds(image, indent_bounds(page_bounds, header_size, footer_size)));

#if 0 && !defined (NDEBUG)
        if (i == 27)
          dump_pgm("page.pgm", image, { page_bounds,
            indent_bounds(page_bounds, header_size, footer_size) });
#endif
      }
    }
  };

  auto threads = std::vector<std::thread>();
  const auto pages_per_thread = page_count / std::thread::hardware_concurrency();
  auto pos = 0;
  for (auto i = 0u; i < std::thread::hardware_concurrency() - 1; ++i) {
    threads.emplace_back(work, pos, pos + pages_per_thread);
    pos += pages_per_thread;
  }
  work(pos, page_count);
  for (auto& thread : threads)
    thread.join();

  return pages;
}
