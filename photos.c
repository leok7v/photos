/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details         */
#include "quick.h"
#include "files.h"
#include "stb_image.h"

begin_c

const char* title = "Photos";

static void layout(uic_t* ui) {
    layouts.center(ui);
}

static void paint(uic_t* ui) {
    // all UIC are transparent and expect parent to paint background
    // UI control paint is always called with a hollow brush
    gdi.set_brush(gdi.brush_color);
    gdi.set_brush_color(colors.black);
    gdi.fill(0, 0, ui->w, ui->h);
}

static int total;


void* exif_of_jpeg(uint8_t *data, int64_t bytes, int32_t *bytes_in_exif) {
    // Check for valid JPEG marker
    if (bytes < 2 || data[0] != 0xFF || data[1] != 0xD8) {
        return null;
    }
    // Search for APP1 marker (EXIF marker)
    int64_t pos = 2;
    while (pos < bytes - 1) {
        if (data[pos] == 0xFF && data[pos + 1] == 0xE1) {
            // Found APP1 marker
            int32_t exif_length = (data[pos + 2] << 8) + data[pos + 3];
            *bytes_in_exif = exif_length;
            return &data[pos + 4];
        }
        pos++;
    }
    // No APP1 marker found
    return null;
}

static void process(const char* pathname) {
    void* data = null;
    int64_t bytes = 0;
    fatal_if_not_zero(crt.memmap_read(pathname, &data, &bytes));
    int w = 0, h = 0, c = 0;
    uint8_t* pixels = stbi_load(pathname, &w, &h, &c, 0);
    fatal_if_null(pixels);
    stbi_image_free(pixels);
    int32_t bytes_in_exif = 0;
    void* app1 = exif_of_jpeg(data, bytes, &bytes_in_exif);
    traceln("%06d %s %s", total++, pathname, app1 != null ? "EXIF" : "");
    crt.memunmap(data, bytes);
}

static void iterate(const char* folder) {
    const int n = (int)strlen(folder);
    folders_t dir = folders.open();
    fatal_if_not_zero(folders.enumerate(dir, folder));
    int count = folders.count(dir);
    for (int i = 0; i < count; i++) {
        const char* name = folders.name(dir, i);
        int k = (int)strlen(name);
        char* pathname = malloc(n + k + 2);
        snprintf(pathname, n + k + 2, "%s\\%s", folder, name);
        if (folders.is_folder(dir, i)) {
            iterate(pathname);
        } else if (k > 4) {
            const char* ext = name + k - 4;
            if (stricmp(ext, ".jpg") == 0 || stricmp(ext, ".png") == 0) {
                process(pathname);
            }
        }
        free(pathname);
    }
    folders.close(dir);
}

static void init(void) {
    app.title = title;
    app.ui->layout = layout;
    app.ui->paint = paint;
    static uic_text(text, "Custom Photo Processor");
    static uic_t* children[] = { &text.ui, null };
    app.ui->children = children;
    bool option = args.option_bool(&app.argc, app.argv, "option");
    traceln("option: %d", option);
    if (app.argc > 1 && files.is_folder(app.argv[1])) {
        iterate(app.argv[1]);
    }
}

app_t app = {
    .class_name = "photos",
    .init = init,
    .min_width = 400,
    .min_height = 200
};

end_c
