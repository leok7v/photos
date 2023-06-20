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
    const char* DateTimeOriginal;   // 0x9003
    const char* ImageDescription;   // 0x010e
    double geo_longitude;           // e.g. 54.9700627,82.7846017
    double geo_latitude;            // e.g. 82.7846017
} exif_extra_t;

static int total;
static int total_yy;
static int total_yy_mm;
static int total_yy_mm_dd;

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

// Convert double value to rational format
static void double_to_rational(double value, int32_t* numerator, uint32_t* denominator) {
    double abs_value = fabs(value);
    double fractional_part = fmod(abs_value, 1.0);
    double integral_part = abs_value - fractional_part;
    // Find the greatest common divisor (GCD) between the numerator and denominator
    uint32_t gcd = 1;
    const uint32_t max_denominator = UINT32_MAX / 2; // To avoid overflow
    for (uint32_t i = 1; i <= max_denominator; i++) {
        double error = fabs(fractional_part - (fractional_part * i) / (double)i);
        if (error < 1e-9) {
            *numerator = (uint32_t)(integral_part * i + fractional_part * i);
            *denominator = i;
            gcd = i;
            break;
        }
    }
    // Simplify the fraction by dividing the numerator and denominator by their GCD
    *numerator /= gcd;
    *denominator /= gcd;
    // If the value was negative, negate the numerator
    if (value < 0) {
        *numerator = -*numerator;
    }
}

#if 0
void insert_exif_description_info_jpeg(const uint8_t* data, int64_t bytes, const exif_extra_t* extra, uint8_t* output_jpeg_with_exif, int64_t max_output_bytes) {
    // Check if there is enough space to add the EXIF data
    int64_t required_bytes = bytes + 46 + strlen(extra->DateTimeOriginal) + strlen(extra->ImageDescription);
    fatal_if(required_bytes > max_output_bytes);
    // Create the APP1 marker segment
    uint8_t app1_marker[] = {
        0xFF, 0xE1, // APP1 marker
        0x00, 0x2A, // Length of APP1 segment (42 bytes)
        0x45, 0x78, 0x69, 0x66, // "Exif" ASCII characters
        0x00, 0x00, // Null terminator (required for some EXIF parsers)
        0x4D, 0x4D, // "MM" (Big-endian) identifier
        0x00, 0x2A, // TIFF header (42 bytes)
        0x00, 0x00, 0x00, 0x08, // IFD0 offset (8 bytes)
    };
    // Copy the original JPEG data before the APP1 segment
    memcpy(output_jpeg_with_exif, data, bytes);
    // Append the APP1 marker segment
    memcpy(output_jpeg_with_exif + bytes, app1_marker, sizeof(app1_marker));
    bytes += sizeof(app1_marker);
    // Append DateTimeOriginal tag (0x9003)
    uint8_t datetime_original_tag[] = {
        0x90, 0x03, // Tag ID (0x9003)
        0x02, 0x00, // Data type (ASCII string)
        0x00, 0x00, 0x00, 0x14, // Data length (20 bytes)
    };
    memcpy(output_jpeg_with_exif + bytes, datetime_original_tag, sizeof(datetime_original_tag));
    bytes += sizeof(datetime_original_tag);
    // Append DateTimeOriginal value
    size_t datetime_original_len = strlen(extra->DateTimeOriginal);
    memcpy(output_jpeg_with_exif + bytes, extra->DateTimeOriginal, datetime_original_len);
    bytes += datetime_original_len;
    // Append ImageDescription tag (0x010E)
    uint8_t image_description_tag[] = {
        0x01, 0x0E, // Tag ID (0x010E)
        0x02, 0x00, // Data type (ASCII string)
        0x00, 0x00, 0x00, 0x00, // Data length (initialized to placeholder for now, will be updated later
    };
    memcpy(output_jpeg_with_exif + bytes, image_description_tag, sizeof(image_description_tag));
    bytes += sizeof(image_description_tag);
    // Append ImageDescription value
    size_t image_description_len = strlen(extra->ImageDescription);
    memcpy(output_jpeg_with_exif + bytes, extra->ImageDescription, image_description_len);
    bytes += image_description_len;
    // TODO: Append geolocation information (to be implemented)
    // Add the final null terminator (required for some EXIF parsers)
    output_jpeg_with_exif[bytes] = 0;
}
#endif

static void insert_exif_description_info_jpeg(const uint8_t* data, int64_t bytes, 
        const exif_extra_t* extra, 
        uint8_t* output_jpeg_with_exif, int64_t max_output_bytes) {
    int64_t required_bytes = bytes + 256 + strlen(extra->DateTimeOriginal) + 
        strlen(extra->ImageDescription);
    fatal_if(required_bytes > max_output_bytes);
    // Create the APP1 marker segment
    uint8_t app1_marker[] = {
        0xFF, 0xE1, 0x00, 0x2A, 0x45, 0x78, 0x69, 0x66, 0x00, 0x00, 0x4D, 0x4D, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x08
    };
    // Copy the original JPEG data before the APP1 segment
    memcpy(output_jpeg_with_exif, data, bytes);
    // Append the APP1 marker segment
    memcpy(output_jpeg_with_exif + bytes, app1_marker, sizeof(app1_marker));
    bytes += sizeof(app1_marker);
    // Append DateTimeOriginal tag (0x9003)
    uint8_t datetime_original_tag[] = {
        0x90, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x14
    };
    memcpy(output_jpeg_with_exif + bytes, datetime_original_tag, sizeof(datetime_original_tag));
    bytes += sizeof(datetime_original_tag);
    // Append DateTimeOriginal value
    size_t datetime_original_len = strlen(extra->DateTimeOriginal);
    memcpy(output_jpeg_with_exif + bytes, extra->DateTimeOriginal, datetime_original_len);
    bytes += datetime_original_len;
    // Append ImageDescription tag (0x010E)
    uint8_t image_description_tag[] = {
        0x01, 0x0E, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    memcpy(output_jpeg_with_exif + bytes, image_description_tag, sizeof(image_description_tag));
    bytes += sizeof(image_description_tag);
    // Append ImageDescription value
    size_t image_description_len = strlen(extra->ImageDescription);
    memcpy(output_jpeg_with_exif + bytes, extra->ImageDescription, image_description_len);
    bytes += image_description_len;
    // Append geolocation information
    uint8_t geolocation_tag[] = {
        0x88, 0x25, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    memcpy(output_jpeg_with_exif + bytes, geolocation_tag, sizeof(geolocation_tag));
    bytes += sizeof(geolocation_tag);
    // Convert geolocation longitude to rational format
    int32_t geo_longitude_numerator = 0;
    uint32_t geo_longitude_denominator = 0;
    double_to_rational(extra->geo_longitude, &geo_longitude_numerator, &geo_longitude_denominator);
    // Append geolocation longitude
    memcpy(output_jpeg_with_exif + bytes, &geo_longitude_numerator, sizeof(geo_longitude_numerator));
    bytes += sizeof(geo_longitude_numerator);
    memcpy(output_jpeg_with_exif + bytes, &geo_longitude_denominator, sizeof(geo_longitude_denominator));
    bytes += sizeof(geo_longitude_denominator);
    // Convert geolocation latitude to rational format
    int32_t geo_latitude_numerator = 0;
    uint32_t geo_latitude_denominator = 0;
    double_to_rational(extra->geo_latitude, &geo_latitude_numerator, &geo_latitude_denominator);
    // Append geolocation latitude
    memcpy(output_jpeg_with_exif + bytes, &geo_latitude_numerator, sizeof(geo_latitude_numerator));
    bytes += sizeof(geo_latitude_numerator);
    memcpy(output_jpeg_with_exif + bytes, &geo_latitude_denominator, sizeof(geo_latitude_denominator));
    bytes += sizeof(geo_latitude_denominator);
    // Add the final null terminator (required for some EXIF parsers)
    output_jpeg_with_exif[bytes] = 0;
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
        int _d = -1;
        if (isdigit(fn[i])) {
            if (sscanf(fn + i, "%d-%d-%d", &m, &d, &y) == 3) {
                if (y < 100) { y += 1900; }
                traceln("%4d/%02d/%02d", y, m, d);
                if (verify < 0 || verify == y) { break; }
            } else if (sscanf(fn + i, "%d'%d'%d", &m, &d, &y) == 3) {
                if (y < 100) { y += 1900; }
                traceln("%4d/%02d/%02d", y, m, d);
                if (verify < 0 || verify == y) { break; }
            } else if (sscanf(fn + i, "%d`%d`%d", &m, &d, &y) == 3) {
                if (y < 100) { y += 1900; }
                traceln("%4d/%02d/%02d", y, m, d);
                if (verify < 0 || verify == y) { break; }
            } else if (sscanf(fn + i, "%d`%d", &m, &y) == 2) {
                d = -1;
                if (y < 100) { y += 1900; }
                traceln("%4d/%02d/??", y, m);
                d = -1;
                if (verify < 0 || verify == y) { break; }
            } else if (sscanf(fn + i, "%d'%d", &m, &y) == 2) {
                d = -1;
                if (y < 100) { y += 1900; }
                traceln("%4d/%02d/??", y, m);
                if (verify < 0 || verify == y) { break; }
            } else if (sscanf(fn + i, "%d,%d", &m, &y) == 2) {
                d = -1;
                if (y < 100) { y += 1900; }
                traceln("%4d/%02d/??", y, m);
                if (verify < 0 || verify == y) { break; }
            } else if (sscanf(fn + i, "%d-%d", &_y, &_m) == 2) {
                if (_y >= 1990 && 1 <= _m && _m <= 12) {
                    y = _y;
                    m = _m;
                    d = -1;
                    traceln("%4d/%02d/??", y, m);
                    if (verify < 0 || verify == y) { break; }
                }
            }
        } else if (fn[i] == '(') {
            if (sscanf(fn + i, "(%d)", &y) == 1) {
                d = -1;
                m = -1;
                if (y < 100) { y += 1900; }
                traceln("%4d/%02d/??", y, m);
                if (verify < 0 || verify == y) { break; }
            }
        } else if (fn[i] == '~') {
            if (sscanf(fn + i, "~%d", &y) == 1) {
                d = -1;
                m = -1;
                if (y < 100) { y += 1900; }
                traceln("%4d/%02d/??", y, m);
                if (verify < 0 || verify == y) { break; }
            } else if (sscanf(fn + i, "~ %d", &y) == 1) {
                d = -1;
                m = -1;
                if (y < 100) { y += 1900; }
                traceln("%4d/%02d/??", y, m);
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
        assert(year > 1990 && year < 2030);
        st.wYear  = (uint16_t)year;
        if (month > 0)   { st.wMonth  = (uint16_t)month; }
        if (day > 0)     { st.wDay    = (uint16_t)day; }
        if (hour > 0)    { st.wHour   = (uint16_t)hour; }
        if (minute > 0)  { st.wMinute = (uint16_t)minute; }
        if (second > 0)  { st.wSecond = (uint16_t)second; }
        fatal_if_false(SystemTimeToFileTime(&st, &ft));
        fatal_if_false(SetFileTime(file, &ft, NULL, &ft));
        fatal_if_false(CloseHandle(file));
    }
}

// STBIRDEF int stbir_resize_uint8(const unsigned char *input_pixels , int input_w , int input_h , int input_stride_in_bytes,
//                                 unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
//                                 int num_channels);

typedef struct write_context_s write_context_t;

typedef struct write_context_s {
    byte memory[16 * 1024 * 1024];
    int32_t written;
} write_context_t;

void jpeg_writer(void *context, void* data, int bytes) {
    write_context_t* write_context = (write_context_t*)context;
    fatal_if(write_context->written + bytes > sizeof(write_context->memory));
    memcpy(write_context->memory + write_context->written, data, bytes);
    write_context->written += bytes;
}

static bool jpeg_write(uint8_t* data, int w, int h, int c) {
    static write_context_t write_context;
    write_context.written = 0;
    int r = stbi_write_jpg_to_func(jpeg_writer, &write_context, w, h, c, data, 85);
    traceln("r: %d written: %d", r, write_context.written);
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

static void process(const char* pathname) {
    total++;
    void* data = null;
    int64_t bytes = 0;
    fatal_if_not_zero(crt.memmap_read(pathname, &data, &bytes));
    int w = 0, h = 0, c = 0;
    uint8_t* pixels = stbi_load(pathname, &w, &h, &c, 0);
    fatal_if_null(pixels);
    int32_t bytes_in_exif = 0;
    void* app1 = exif_of_jpeg(data, bytes, &bytes_in_exif);
    traceln("%s %d %dx%d:%d exif:%d", pathname, bytes, w, h, c, bytes_in_exif);
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
        traceln("%06d %s %04d %s", total, relative, folder_year, app1 != null ? "EXIF" : "");
    }
    exif_info_t exif = {0};
    exif_from_memory(&exif, data, (uint32_t)bytes);
    int exif_year = -1;
    int exif_month = -1;
    int exif_day = -1;
    int exif_hour = -1;
    int exif_minute = -1;
    int exif_second = -1;
    if (strlen(exif.DateTimeOriginal) > 0) {
        if (sscanf(exif.DateTimeOriginal, "%d:%d:%d %d:%d:%d",
            &exif_year, &exif_month,  &exif_day,
            &exif_hour, &exif_minute, &exif_second) != 6) {
            traceln("exif.DateTimeOriginal: %s", exif.DateTimeOriginal);
            exif_year = -1; exif_month = -1; exif_day = -1;
            exif_hour = -1; exif_minute = -1; exif_second = -1;
        }
    }
    if (exif_year < 0 && strlen(exif.DateTime) > 0) {
        if (sscanf(exif.DateTime, "%d:%d:%d %d:%d:%d",
            &exif_year, &exif_month,  &exif_day,
            &exif_hour, &exif_minute, &exif_second) != 6) {
            traceln("exif.DateTime: %s", exif.DateTime);
            exif_year = -1; exif_month = -1; exif_day = -1;
            exif_hour = -1; exif_minute = -1; exif_second = -1;
        }
    }
    if (exif_year < 0 && strlen(exif.DateTimeDigitized) > 0) {
        if (sscanf(exif.DateTimeDigitized, "%d:%d:%d %d:%d:%d",
            &exif_year, &exif_month,  &exif_day,
            &exif_hour, &exif_minute, &exif_second) != 6) {
            traceln("exif.DateTimeDigitized: %s", exif.DateTimeDigitized);
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
    if (strlen(exif.ImageDescription) > 0) {
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
    if (app1 != null) {
        // TODO: merge exifs?
    }
//  exif_extra_t extra;
//  https://www.awaresystems.be/imaging/tiff/tifftags/privateifd/exif/datetimeoriginal.html#:~:text=The%20format%20is%20%22YYYY%3AMM,blank%20character%20(hex%2020).
//  extra.DateTimeOriginal = "2023:06:19 15:30:00";
//  extra.ImageDescription = "Example description";
//  https://www.awaresystems.be/imaging/tiff/tifftags/gpsifd.html
//  https://www.awaresystems.be/imaging/tiff/tifftags/privateifd/gps/gpslatitude.html
//  If latitude is expressed as degrees, minutes and seconds, a typical format would be dd/1,mm/1,ss/1.
//  When degrees and minutes are used and, for example, fractions of minutes are given up to two decimal places, the format would be dd/1,mmmm/100,0/1.
    stbi_image_free(pixels);
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
