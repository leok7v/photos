/* Copyright (c) Dmitry "Leo" Kuznetsov 2021 see LICENSE for details         */
#include "quick.h"
#include "files.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_image_resize.h"
#include "tiny_exif.h"
#include "crt.h"
#include <math.h>
#include <Windows.h>

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

typedef struct exif_extra_s {
    char DateTimeOriginal[1024];   // 0x9003
    char ImageDescription[1024];   // 0x010e
} exif_extra_t;

static int total;
static int total_yy;
static int total_yy_mm;
static int total_yy_mm_dd;


static void big_endian_32(uint8_t* p, uint32_t v) {
    for (int i = 0; i < 4; i++) {
        p[3 - i] = (uint8_t)(v & 0xFF);
        v >>= 8;
    }
}

static int32_t append_exif_description(const uint8_t* data, int64_t bytes,
        const exif_extra_t* extra, uint8_t* output, int64_t max_output_bytes) {
    // Check if there is enough space to add the EXIF data
memset(output, 0xFF, 256);
    int64_t required_bytes = bytes + 43 + strlen(extra->DateTimeOriginal) + strlen(extra->ImageDescription);
    fatal_if(required_bytes > max_output_bytes);
    fatal_if(data[0] != 0xFF || data[1] != 0xD8); // SOI
    uint8_t* out = output;
    memcpy(out, data, 2);
    out += 2;
    uint8_t app1_marker[] = {
        0xFF, 0xE1, // APP1 marker
        0x00, 0x00, // Length of APP1 segment (44 bytes)
        0x45, 0x78, 0x69, 0x66, // "Exif" ASCII characters
        0x00, 0x00, // Null terminator (required for some EXIF parsers)
        0x4D, 0x4D, // "MM" (Big-endian) identifier
        0x00, 0x2A, // TIFF header (42 bytes)
        0x00, 0x00, 0x00, 0x08, // IFD0 offset (8 bytes)
    };
    uint8_t* app1 = out;
    memcpy(out, app1_marker, sizeof(app1_marker));
    out += sizeof(app1_marker);
    // num_entries = 2
    memcpy(out, "\x00\x02", 2);
    out += 2;
    // Append DateTimeOriginal tag (0x9003)
    uint8_t datetime_original_tag[] = {
        0x90, 0x03, // Tag ID (0x9003)
        0x00, 0x02, // Data type (ASCII string)
        0x00, 0x00, 0x00, 0x14, // Data length (20 bytes)
        0x00, 0x00, 0x00, 0x00, // Offset (because length > 4) or value
    };
    static_assertion(sizeof(datetime_original_tag) == 12);
    uint8_t* datetime_original = out;
    memcpy(out, datetime_original_tag, sizeof(datetime_original_tag));
    out += sizeof(datetime_original_tag);
    // Append ImageDescription tag (0x010E)
    uint8_t image_description_tag[] = {
        0x01, 0x0E, // Tag ID (0x010E)
        0x00, 0x02, // Data type (ASCII string)
        0x00, 0x00, 0x00, 0x00, // Data length (initialized to placeholder for now, will be updated later)
        0x00, 0x00, 0x00, 0x00, // Offset (if length > 4) or value
    };
    static_assertion(sizeof(image_description_tag) == 12);
    uint8_t* image_description = out;
    memcpy(out, image_description_tag, sizeof(image_description_tag));
    out += sizeof(image_description_tag);
    // DateTimeOriginal
    size_t datetime_original_len = strlen(extra->DateTimeOriginal) + 1;
    assert(datetime_original_len == 0x14);
    // offset to the data:
    big_endian_32(datetime_original + 8, (uint32_t)(out - (app1 + 10)));
    memcpy(out, extra->DateTimeOriginal, datetime_original_len);
    out += datetime_original_len;

    // ImageDescription
    size_t image_description_len = strlen(extra->ImageDescription) + 1;
    big_endian_32(image_description + 4, (uint32_t)image_description_len);
    if (image_description_len < 4) {
        memcpy(image_description + 8, extra->ImageDescription, image_description_len);
    } else {
        memcpy(out, extra->ImageDescription, image_description_len);
        big_endian_32(image_description + 8, (uint32_t)(out - (app1 + 10)));
        out += image_description_len;
    }
    big_endian_32(out, 0); // last IFD record
    out += 4;
    size_t app_len = out - app1;
    app1[3] = (uint8_t)((app_len >> 0) & 0xFF);
    app1[2] = (uint8_t)((app_len >> 8) & 0xFF);
    memcpy(out, data + 2, bytes - 2);
    out += bytes - 2;
    return (int32_t)(out - output);
}

static void yymmdd(const char* fn, int verify, int* year, int* month, int* day) {
    int n = (int)strlen(fn);
    int y = -1;
    int m = -1;
    int d = -1;
    for (int i = 0; i < n; i++) {
        y = -1;
        m = -1;
        d = -1;
        int _y = -1;
        int _m = -1;
        if (isdigit(fn[i])) {
            if (sscanf(fn + i, "%d-%d-%d", &m, &d, &y) == 3) {
                if (y < 100) { y += 1900; }
                if (m > 12 && d <= 12) { int swap = m; m = d; d = swap; }
//              traceln("%4d/%02d/%02d", y, m, d);
                if (verify < 0 || verify == y) { break; }
            } else if (sscanf(fn + i, "%d'%d'%d", &m, &d, &y) == 3) {
                if (y < 100) { y += 1900; }
                if (m > 12 && d <= 12) { int swap = m; m = d; d = swap; }
//              traceln("%4d/%02d/%02d", y, m, d);
                if (verify < 0 || verify == y) { break; }
            } else if (sscanf(fn + i, "%d`%d`%d", &m, &d, &y) == 3) {
                if (y < 100) { y += 1900; }
                if (m > 12 && d <= 12) { int swap = m; m = d; d = swap; }
//              traceln("%4d/%02d/%02d", y, m, d);
                if (verify < 0 || verify == y) { break; }
            } else if (sscanf(fn + i, "%d`%d", &m, &y) == 2) {
                d = -1;
                if (y < 100) { y += 1900; }
//              traceln("%4d/%02d/??", y, m);
                d = -1;
                if (verify < 0 || verify == y) { break; }
            } else if (sscanf(fn + i, "%d'%d", &m, &y) == 2) {
                d = -1;
                if (y < 100) { y += 1900; }
//              traceln("%4d/%02d/??", y, m);
                if (verify < 0 || verify == y) { break; }
            } else if (sscanf(fn + i, "%d,%d", &m, &y) == 2) {
                d = -1;
                if (y < 100) { y += 1900; }
//              traceln("%4d/%02d/??", y, m);
                if (verify < 0 || verify == y) { break; }
            } else if (sscanf(fn + i, "%d-%d", &_y, &_m) == 2) {
                if (_y >= 1990 && 1 <= _m && _m <= 12) {
                    y = _y;
                    m = _m;
                    d = -1;
//                  traceln("%4d/%02d/??", y, m);
                    if (verify < 0 || verify == y) { break; }
                }
            }
        } else if (fn[i] == '(') {
            if (sscanf(fn + i, "(%d)", &y) == 1) {
                d = -1;
                m = -1;
                if (y < 100) { y += 1900; }
//              traceln("%4d/%02d/??", y, m);
                if (verify < 0 || verify == y) { break; }
            }
        } else if (fn[i] == '~') {
            if (sscanf(fn + i, "~%d", &y) == 1) {
                d = -1;
                m = -1;
                if (y < 100) { y += 1900; }
//              traceln("%4d/%02d/??", y, m);
                if (verify < 0 || verify == y) { break; }
            } else if (sscanf(fn + i, "~ %d", &y) == 1) {
                d = -1;
                m = -1;
                if (y < 100) { y += 1900; }
//              traceln("%4d/%02d/??", y, m);
                if (verify < 0 || verify == y) { break; }
            }
        }
    }
    if (y > 0 && m > 0 && d > 0) {
        *year = y;
        *month = m;
        *day = d;
        total_yy_mm_dd++;
    } else if (y > 0 && m > 0) {
        *year = y;
        *month = m;
        total_yy_mm++;
    } else if (y > 1900) {
        total_yy++;
        *year = y;
    }
}

static void change_file_creation_and_write_time(const char* fn, int year, int month, int day,
        int hour, int minute, int second) {
    void* file = CreateFileA(fn, GENERIC_READ | FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    fatal_if_null(file);
    if (file != INVALID_HANDLE_VALUE) {
        FILETIME ft = {0};
        SYSTEMTIME st = {0};
        GetFileTime(file, &ft, null, null); // creation, access, write
        FileTimeToSystemTime(&ft, &st);
        assert(year > 1900 && year < 2030);
        st.wYear  = (uint16_t)year;
        if (month > 0)   { st.wMonth  = (uint16_t)month; }
        if (day > 0)     { st.wDay    = (uint16_t)day; }
        if (hour > 0)    { st.wHour   = (uint16_t)hour; }
        if (minute > 0)  { st.wMinute = (uint16_t)minute; }
        if (second > 0)  { st.wSecond = (uint16_t)second; }
        if (SystemTimeToFileTime(&st, &ft)) {
            fatal_if_false(SetFileTime(file, &ft, NULL, &ft));
        } else {
            traceln("bad time: %s", fn);
        }
        fatal_if_false(CloseHandle(file));
    }
}

// STBIRDEF int stbir_resize_uint8(const unsigned char *input_pixels , int input_w , int input_h , int input_stride_in_bytes,
//                                 unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
//                                 int num_channels);

typedef struct writer_context_s writer_context_t;

typedef struct writer_context_s {
    byte memory[16 * 1024 * 1024];
    int32_t written;
} writer_context_t;

static writer_context_t writer_context;
static byte jpeg_memory[16 * 1024 * 1024];

void jpeg_writer(void *context, void* data, int bytes) {
    writer_context_t* wc = (writer_context_t*)context;
    fatal_if(wc->written + bytes > sizeof(wc->memory));
    memcpy(wc->memory + wc->written, data, bytes);
    wc->written += bytes;
}

static bool jpeg_write(uint8_t* data, int w, int h, int c) {
    writer_context.written = 0;
    int r = stbi_write_jpg_to_func(jpeg_writer, &writer_context, w, h, c, data, 85);
//  traceln("r: %d written: %d", r, writer_context.written);
    return r;
}

static const char* months[13] = {
    "",
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};


static const char* output_folder = "c:/tmp/photos";
static char output_path[260];

static void append_pathname(const char* relative) {
    int n = (int)strlen(relative);
    int k = (int)strlen(output_path);
    for (int i = 0; i < n && k < countof(output_path) - 32; i++) {
        if (isalpha(relative[i]) || isdigit(relative[i]) || relative[i] == '.') {
            output_path[k] = relative[i];
            k++;
        } else if (k > 0 && output_path[k - 1] != '_' && output_path[k - 1] != '-') {
            output_path[k] = '_';
            k++;
        }
        output_path[k] = 0;
    }
}


static const char* words(const char* fn) {
    static char desc[1024];
    int n = (int)strlen(fn);
    int k = 0;
    for (int i = 0; i < n && k < countof(desc) - 2; i++) {
        if (fn[i] == '_' || fn[i] == '.' || fn[i] == '-') {
            desc[k] = 0x20;
        } else {
            desc[k] = fn[i];
        }
        k++;
    }
    desc[k] = 0;
    return desc;
}

static void process(const char* pathname) {
    total++;
    void* data = null;
    int64_t bytes = 0;
    crt.memmap_read(pathname, &data, &bytes);
    int w = 0, h = 0, c = 0;
    uint8_t* pixels = data == null ? null : stbi_load(pathname, &w, &h, &c, 0);
    if (pixels != null) {
        exif_info_t exif = {0};
        bool has_exif = exif_from_memory(&exif, data, (uint32_t)bytes) == 0;
        has_exif = has_exif && exif.ImageHeight > 0 && exif.ImageHeight > 0;
    //  traceln("%s %d %dx%d:%d exif: %d", pathname, bytes, w, h, c, has_exif);
        const char* relative = pathname + strlen(app.argv[1]) + 1;
        int folder_year = -1;
        int year = -1;
        int month = -1;
        int day = -1;
        int hour = -1;
        int minute = -1;
        int second = -1;
        if (sscanf(relative, "%d/", &folder_year) != 1) {
            folder_year = -1;
            traceln("NO folder_year");
        } else {
            if (folder_year < 100) { folder_year += 1900; };
            yymmdd(relative, folder_year, &year, &month, &day);
    //      traceln("%06d %s %04d %s", total, relative, folder_year, has_exif ? "EXIF" : "");
        }
        int exif_year = -1;
        int exif_month = -1;
        int exif_day = -1;
        int exif_hour = -1;
        int exif_minute = -1;
        int exif_second = -1;
        if (exif.DateTimeOriginal != null && strlen(exif.DateTimeOriginal) > 0) {
            if (sscanf(exif.DateTimeOriginal, "%d:%d:%d %d:%d:%d",
                &exif_year, &exif_month,  &exif_day,
                &exif_hour, &exif_minute, &exif_second) != 6) {
    //          traceln("exif.DateTimeOriginal: %s", exif.DateTimeOriginal);
                exif_year = -1; exif_month = -1; exif_day = -1;
                exif_hour = -1; exif_minute = -1; exif_second = -1;
            }
        }
        if (exif_year < 0 && exif.DateTime != null  && strlen(exif.DateTime) > 0) {
            if (sscanf(exif.DateTime, "%d:%d:%d %d:%d:%d",
                &exif_year, &exif_month,  &exif_day,
                &exif_hour, &exif_minute, &exif_second) != 6) {
    //          traceln("exif.DateTime: %s", exif.DateTime);
                exif_year = -1; exif_month = -1; exif_day = -1;
                exif_hour = -1; exif_minute = -1; exif_second = -1;
            }
        }
        if (exif_year < 0 && exif.DateTimeDigitized != null && strlen(exif.DateTimeDigitized) > 0) {
            if (sscanf(exif.DateTimeDigitized, "%d:%d:%d %d:%d:%d",
                &exif_year, &exif_month,  &exif_day,
                &exif_hour, &exif_minute, &exif_second) != 6) {
    //          traceln("exif.DateTimeDigitized: %s", exif.DateTimeDigitized);
                exif_year = -1; exif_month = -1; exif_day = -1;
                exif_hour = -1; exif_minute = -1; exif_second = -1;
            }
        }
        if (exif_year > 1900 && 1 <= exif_month && exif_month <= 12 && exif_day > 0) {
            year   = exif_year;
            month  = exif_month;
            day    = exif_day;
            hour   = exif_hour;
            minute = exif_minute;
            second = exif_second;
        }
        if (year < 0) { year = folder_year; }
        if (month > 12) { month = -1; }
        if (day   > 31) { day   = -1; }
        if (exif.ImageDescription != null && strlen(exif.ImageDescription) > 0) {
            traceln("exif.ImageDescription: %s", exif.ImageDescription);
        }
        files.mkdirs(output_folder);
        if (folder_year > 1900 && abs(year - folder_year) > 2) { year = folder_year; }
        if (year > 1990 && month > 0 && day > 0) {
            snprintf(output_path, countof(output_path), "%s/img%06d_%04d-%s-%02d_",
                output_folder, total, year, months[month], day);
        } else if (year > 1990 && month > 0) {
            snprintf(output_path, countof(output_path), "%s/img%06d_%04d-%s_", output_folder, total, year, months[month]);
        } else if (year > 1990) {
            snprintf(output_path, countof(output_path), "%s/img%06d_%04d_", output_folder, total, year);
        } else {
            snprintf(output_path, countof(output_path), "%s/img%06d_", output_folder, total);
        }
        append_pathname(relative);
        traceln("%s", output_path);
        jpeg_write(pixels, w, h, c);
        assert(year > 1900);
        void*   write_data = writer_context.memory;
        int32_t write_bytes = writer_context.written;
        if (has_exif) {
    //      traceln("TODO: merge exifs?");
        } else {
            exif_extra_t extra = {0};
            int m  =  month  < 1 ?  6 : month;
            int d  =  day    < 1 ? 15 : day;
            int hr =  hour   < 1 ? 11 : hour;
            int mn =  minute < 1 ? 58 : minute;
            int sc =  second < 1 ? 29  : second;
            snprintf(extra.DateTimeOriginal, countof(extra.DateTimeOriginal),
                "%04d:%02d:%02d %02d:%02d:%02d",
                year, m, d, hr, mn, sc);
            snprintf(extra.ImageDescription, countof(extra.ImageDescription),
                "%s",
                words(output_path + strlen(output_folder) + 1));
            write_bytes = append_exif_description(writer_context.memory, writer_context.written,
                &extra, jpeg_memory, sizeof(jpeg_memory));
            write_data = jpeg_memory;
            assert(write_bytes > writer_context.written);
        }
        FILE* file = fopen(output_path, "wb");
        size_t k = fwrite(write_data, 1, write_bytes, file);
        fatal_if(k != write_bytes);
        fclose(file);
        if (!has_exif) {
            memset(&exif, 0, sizeof(exif));
            int r = exif_from_memory(&exif, write_data, write_bytes);
            fatal_if(r != EXIF_PARSE_SUCCESS);
            assert(exif.ImageDescription[0] != 0);
            assert(exif.DateTimeOriginal[0] != 0);
        }
        change_file_creation_and_write_time(output_path, year, month, day, hour, minute, second);
    //  https://www.awaresystems.be/imaging/tiff/tifftags/privateifd/exif/datetimeoriginal.html#:~:text=The%20format%20is%20%22YYYY%3AMM,blank%20character%20(hex%2020).
    //  extra.DateTimeOriginal = "2023:06:19 15:30:00";
    //  extra.ImageDescription = "Example description";
    //  https://www.awaresystems.be/imaging/tiff/tifftags/gpsifd.html
    //  https://www.awaresystems.be/imaging/tiff/tifftags/privateifd/gps/gpslatitude.html
    //  If latitude is expressed as degrees, minutes and seconds, a typical format would be dd/1,mm/1,ss/1.
    //  When degrees and minutes are used and, for example, fractions of minutes are given up to two decimal places, the format would be dd/1,mmmm/100,0/1.
        stbi_image_free(pixels);
    }
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
        for (int j = 0; j < n + k + 1; j++) {
            if (pathname[j] == '\\') { pathname[j] = '/'; }
        }
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

static void exif_test(const char* pathname) {
    void* data = null;
    int64_t bytes = 0;
    crt.memmap_read(pathname, &data, &bytes);
    exif_info_t exif = {0};
    exif.dump = true;
    bool has_exif = exif_from_memory(&exif, data, (uint32_t)bytes) == 0;
    has_exif = has_exif && exif.ImageHeight > 0 && exif.ImageHeight > 0;
    crt.memunmap(data, bytes);
}

static void init(void) {
    app.title = title;
    app.ui->layout = layout;
    app.ui->paint = paint;
    static uic_text(text, "Custom Photo Processor");
    static uic_t* children[] = { &text.ui, null };
    app.ui->children = children;
    bool test_exif = args.option_bool(&app.argc, app.argv, "--test-exif");
    if (test_exif && app.argc > 1 && files.exists(app.argv[1]) && !files.is_folder(app.argv[1])) {
        exif_test(app.argv[1]);
        exit(0);
    } else if (test_exif && app.argc == 1) {
        exif_test("metadata_test_file_IIM_XMP_EXIF.jpg");
        exif_test("IPTC-PhotometadataRef-Std2022.1.jpg");
        exit(0);
    } else if (app.argc > 1 && files.is_folder(app.argv[1])) {
        iterate(app.argv[1]);
        traceln("totals: %d yymmdd: %d yymm: %d yy: %d",
            total, total_yy_mm_dd, total_yy_mm, total_yy);
    }
}

app_t app = {
    .class_name = "photos",
    .init = init,
    .min_width = 400,
    .min_height = 200
};

end_c
