
#include "output.h"
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

namespace {
  void update_box(QPDFObjectHandle& page, char const* box_name, const Box& box) {
    auto handle = page.getKey(box_name);
    if (handle.isRectangle()) {
      const auto original_box = handle.getArrayAsRectangle();
      page.replaceKey(box_name, QPDFObjectHandle::newArray(
        QPDFObjectHandle::Rectangle{
          std::max(box.llx, 0.0) + original_box.llx,
          std::max(box.lly, 0.0) + original_box.lly,
          std::min(box.urx, original_box.urx - original_box.llx) + original_box.llx,
          std::min(box.ury, original_box.ury - original_box.lly) + original_box.lly
        }));
    }
  }

  void update_boxes(QPDFObjectHandle& page, const Box& box) {
    for (auto box_name : { "/MediaBox", "/CropBox", "/BleedBox", "/TrimBox", "/ArtBox" })
      update_box(page, box_name, box);
  }
} // namespace

void output_with_boxes(const Settings& settings, const std::vector<Page>& pages) {
  auto pdf = QPDF();
  pdf.setSuppressWarnings(true);
  pdf.processFile(settings.input_file.u8string().c_str());

  auto i = 0;
  for (QPDFPageObjectHelper& ph : QPDFPageDocumentHelper(pdf).getAllPages()) {
    auto page = ph.getObjectHandle();
    update_boxes(page, pages.at(i).bounding_box);
    ++i;
  }

  auto writer = QPDFWriter(pdf, settings.output_file.u8string().c_str());
  writer.write();
}
