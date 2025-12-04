#include <cmath>
#include <cstdlib>
#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <pugixml.hpp>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <zip.h>

int main(int argc, char *argv[]) {
    // PSEUDOCODE: Validate inputs
    if (argc < 3) {
        printf("Too few arguments. Usage: pdf_to_epub input.pdf output.epub\n");
        return -1;
    }

    const std::string pdf_file = argv[1];
    poppler::document *document = poppler::document::load_from_file(pdf_file);
    // PSEUDOCODE: Check if document loaded
    if (!document) {
        printf("Failed to load PDF\n");
        return -1;
    }
    int pages = document->pages();
    // Prepare storage for content: mkdir -p temp/OEBPS temp/OEBPS/images
    std::filesystem::create_directories("temp/OEBPS");
    std::filesystem::create_directories("temp/OEBPS/images");
    std::filesystem::create_directories("temp/META-INF");

    // execute pdfimages in shell to get all images
    auto extract_images = "cd temp/OEBPS/images && pdfimages -png " + pdf_file;
    system((const char *)&extract_images);

    std::vector<std::string> chapters;
    std::vector<std::string> image_paths;
    std::vector<std::string> html_paths;

    for (int i = 0; i < pages; i++) {
        poppler::page *page = document->create_page(i);
        std::string text = page->text().to_latin1();
        // PSEUDOCODE: Store text for this chapter
        chapters.push_back(text);

        delete page;
    }

    // List extracted images in temp/OEBPS/images/ and populate image_paths
    try {
        for (const auto &entry :
             std::filesystem::directory_iterator("temp/OEBPS/images")) {
            image_paths.push_back(entry.path().string());
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    // Format content into HTML: For each chapter, create HTML doc with
    // <html><head><title>Chapter N</title></head><body><p>text</p> + img
    // tags for all images</body></html>, save to temp/OEBPS/chapter_N.html
    // and add to html_paths

    for (size_t i = 0; i < chapters.size(); i++) {
        pugi::xml_document html_doc;
        auto html = html_doc.append_child("html");
        auto head = html.append_child("head");
        auto title = head.append_child("title");
        title.text() = ("Chapter" + std::to_string(i + 1)).c_str();
        auto body = html.append_child("body");
        auto p = body.append_child("p");
        p.text() = chapters[i].c_str();

        for (const auto &img_path : image_paths) {
            auto filename = std::filesystem::path(img_path).filename().string();
            auto img = body.append_child("img");
            img.append_attribute("src") = ("images/" + filename).c_str();
            img.append_attribute("alt") = "Image";
        }

        std::string html_path =
            "temp/OEBPS/chapter_" + std::to_string(i) + ".html";

        if (!html_doc.save_file(html_path.c_str())) {
            printf("Failed to save file");
        }
        html_paths.push_back(html_path);
    }

    // Generate EPUB metadata:
    // - META-INF/container.xml: XML pointing to OEBPS/content.opf
    // - OEBPS/content.opf: Package with metadata (title, creator), manifest
    // (HTMLs and images), spine (chapter order)
    // - OEBPS/toc.ncx: NCX with navMap for chapters

    // Package into ZIP: Create ZIP with mimetype (uncompressed), then add
    // META-INF/container.xml, OEBPS/content.opf, OEBPS/toc.ncx, HTML files,
    // images, save as argv[2]

    // Cleanup: rm -rf temp

    delete document;
    return 0;
}
