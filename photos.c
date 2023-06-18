/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details */
#include "quick.h"

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

static void process() {
    
}

static void init(void) {
    app.title = title;
    app.ui->layout = layout;
    app.ui->paint = paint;
    static uic_text(text, "Custom Photo Processor");
    static uic_t* children[] = { &text.ui, null };
    app.ui->children = children;
}

app_t app = {
    .class_name = "photos",
    .init = init,
    .min_width = 400,
    .min_height = 200
};

end_c
