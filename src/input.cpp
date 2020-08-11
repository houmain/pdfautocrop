
#include "input.h"
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <poppler/cpp/poppler-page-renderer.h>
#include <thread>
#include <cstring>

namespace {
  using Rect = poppler::rect;
  using Image = poppler::image;

  char guess_background_color(const Image& image) {
    return *image.const_data();
  }

  Rect get_used_bounds(const Image& image) {
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

    const auto x1 = image.width() - 1;
    const auto y1 = image.height() - 1;

    auto min_y = 0;
    for (; min_y < y1; ++min_y)
      if (!rect_has_background_color({ 0, min_y, x1, 1 }))
        break;

    auto max_y = y1;
    for (; max_y > min_y; --max_y)
      if (!rect_has_background_color({ 0, max_y, x1, 1 }))
        break;

    auto min_x = 0;
    for (; min_x < x1; ++min_x)
      if (!rect_has_background_color({ min_x, min_y, 1, max_y - min_y }))
        break;

    auto max_x = x1;
    for (; max_x > min_x; --max_x)
      if (!rect_has_background_color({ max_x, min_y, 1, max_y - min_y }))
        break;

    return { min_x, min_y, max_x - min_x + 1, max_y - min_y + 1 };
  }

  Box calculate_box(poppler::page_renderer& renderer,
      const poppler::document& document, int index) {
    const auto page = std::unique_ptr<poppler::page>(document.create_page(index));
    const auto image = renderer.render_page(page.get());
    const auto bounds = get_used_bounds(image);
    const auto scale_x = page->page_rect().width() / image.width();
    const auto scale_y = page->page_rect().height() / image.height();
    return {
      bounds.left() * scale_x,
      page->page_rect().height() - (bounds.bottom() * scale_y),
      bounds.right() * scale_x,
      page->page_rect().height() - (bounds.top() * scale_y)
    };
  }
} // namespace

std::vector<Box> calculate_page_boxes(const Settings& settings) {
  auto document = std::unique_ptr<poppler::document>(
      poppler::document::load_from_file(settings.input_file.u8string()));
  if (!document)
    return { };

  auto boxes = std::vector<Box>();
  const auto page_count = document->pages();
  boxes.resize(page_count);

  const auto work = [&](int begin, int end) {
    auto renderer = poppler::page_renderer();
    renderer.set_image_format(Image::format_gray8);

    for (auto i = begin; i < end; ++i)
      boxes[i] = calculate_box(renderer, *document, i);
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

  return boxes;
}
