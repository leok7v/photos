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

static void test_exif(const char* pathname) {
    void* data = null;
    int64_t bytes = 0;
    crt.memmap_read(pathname, &data, &bytes);
    exif_info_t exif = {0};
    bool has_exif = exif_from_memory(&exif, data, (uint32_t)bytes) == 0;
    has_exif = has_exif && exif.ImageHeight > 0 && exif.ImageHeight > 0;
    crt.memunmap(data, bytes);
}

#include "yxml.h"
#include <inttypes.h>

typedef struct xml_npv_s {
    const char* n; 
    const char**v;
    int32_t up; // for nested tags e.g.: 
    // <dc:creator><rdf:Seq><rdf:li>name</rdf:li></rdf:Seq></dc:creator>
    // <rdf:Alt><rdf:li>...</rdf:li></rdf:Alt>
    // <rdf:Bag><rdf:li>...</rdf:li></rdf:Bag>
    int32_t append; // for appending lists like:
    // <dc:subject><rdf:Bag>
    //    <rdf:li>keyword1</rdf:li><rdf:li>keyword2</rdf:li>...
    // </rdf:Bag></dc:subject>
    const char* p; // parent for disambiguation 
} xml_npv_t;

// https://stackoverflow.com/questions/29001433/how-rdfbag-rdfseq-and-rdfalt-is-different-while-using-them

// xml_rdf_t represents rdf:Bag rdf:Seq and rdf:Alt
// interpretation is left to the client.
// UTF-8 "\x00\x00" terminated list of "\x00" terminated strings

typedef const char* xml_rdf_t; 
typedef const char* xml_str_t; 

typedef struct xml_context_s {
    yxml_t yxml;
    struct {
        struct {
            xml_str_t CountryCode;
            struct {
                xml_str_t CiAdrCity;
                xml_str_t CiAdrCtry;
                xml_str_t CiAdrExtadr;
                xml_str_t CiAdrPcode;
                xml_str_t CiAdrRegion;
                xml_str_t CiEmailWork;
                xml_str_t CiTelWork;
                xml_str_t CiUrlWork;
            } CreatorContactInfo;
            xml_str_t IntellectualGenre;
            xml_str_t Location;
            xml_str_t Scene;
            xml_str_t SubjectCode;
        } Iptc4xmpCore;
        struct {
            // see: https://github.com/Exiv2/exiv2/issues/1959 for the problems
            // https://www.iptc.org/std/photometadata/examples/
            // with ArtworkOrObject disambiguations...
            xml_str_t AOCopyrightNotice;
            xml_rdf_t AOCreator;      
            xml_str_t AODateCreated;  // "2017-05-29T17:19:21-0400"
            xml_str_t AddlModelInfo;
            struct {
                xml_str_t AOCircaDateCreated;
                xml_rdf_t AOContentDescription;       
                xml_rdf_t AOContributionDescription;  
                xml_str_t AOCopyrightNotice; 
                xml_rdf_t AOCreator;                  
                xml_rdf_t AOCreatorId;                
                xml_str_t AOCurrentCopyrightOwnerId;
                xml_str_t AOCurrentCopyrightOwnerName;
                xml_str_t AOCurrentLicensorId;
                xml_str_t AOCurrentLicensorName;
                xml_rdf_t AOPhysicalDescription;      
                xml_str_t AOSource;
                xml_str_t AOSourceInvNo;
                xml_str_t AOSourceInvURL;
                xml_rdf_t AOStylePeriod;              
                xml_rdf_t AOTitle;                    
            } ArtworkOrObject;
            xml_str_t DigImageGUID;
            xml_rdf_t EmbdEncRightsExpr;
            xml_str_t DigitalSourceType;
            xml_rdf_t Event;                          
            xml_rdf_t EventId;                          
            struct { // TODO: this is actually bag of triplets
                xml_str_t LinkedRightsExpr; 
                xml_str_t RightsExprEncType;
                xml_str_t RightsExprLangId;
            } LinkedEncRightsExpr;
            struct {
                xml_str_t City;
                xml_str_t CountryCode;
                xml_str_t CountryName;
                xml_str_t ProvinceState;
                xml_str_t Sublocation;
                xml_str_t WorldRegion;
            } LocationCreated;
            struct {
                xml_str_t City;
                xml_str_t CountryCode;
                xml_str_t CountryName;
                xml_str_t ProvinceState;
                xml_str_t Sublocation;
                xml_str_t WorldRegion;
            } LocationShown;
            xml_str_t MaxAvailHeight;
            xml_str_t MaxAvailWidth;
            xml_rdf_t ModelAge;               
            xml_rdf_t OrganisationInImageCode;
            xml_rdf_t OrganisationInImageName;
            xml_rdf_t PersonInImage; 
            xml_rdf_t PersonCharacteristic;
            xml_rdf_t PersonDescription;
            struct { // TODO: should be PersonInImageDetails[count]
                xml_rdf_t PersonCharacteristic;
                xml_rdf_t PersonDescription;
                xml_rdf_t PersonId;
                xml_rdf_t PersonName;
            } PersonInImageWDetails;
            struct { // TODO: should be PersonInImageDetails[count]
                xml_rdf_t ProductDescription;
                xml_str_t ProductGTIN;
                xml_str_t ProductId;
                xml_rdf_t ProductName;
            } ProductInImage;
            struct {
                xml_str_t RegItemId;
                xml_str_t RegOrgId;
            } RegistryId;
            struct {
                xml_str_t CvId;
                xml_str_t CvTermId;
                xml_rdf_t CvTermName; 
                xml_str_t CvTermRefinedAbout;
            } AboutCvTerm;
        } Iptc4xmpExt;
        struct {
            xml_str_t creator;
            xml_str_t date;   // "2017-05-29T17:19:21-0400"
            xml_str_t format; // "image/jpeg"
            xml_str_t description;
            xml_str_t rights;
            xml_rdf_t title;
            xml_rdf_t subject;
        } dc;
        struct {
            xml_str_t GPSAltitude;    // "0/10"
            xml_str_t GPSAltitudeRef; // "0"
            xml_str_t GPSLatitude;    // "26,34.951N"
            xml_str_t GPSLongitude;   // "80,12.014W"
        } exif;
//      struct {                      // TODO: complicated
//          int32_n count;
//          struct {
//              xml_rdf_t Name;
//              xml_rdf_t Role;
//          } contributors;
//      } Contributor;
        xml_str_t CreateDate;
        xml_str_t CreatorTool;
        xml_str_t MetadataDate;
        xml_str_t ModifyDate;
        xml_str_t Rating;
        xml_str_t Lens; // <aux:Lens>Samsung Galaxy S7 Rear Camera</aux:Lens>
    } xmp;
    char* stack[64]; // element names stack
    int32_t top;
    char* strings;
    char* next;
    char* end;
    xml_npv_t* npv;
    int32_t npv_count;
    int32_t index;
} xml_context_t;

static const char* yxml_parent(xml_context_t* ctx, int up) {
    int sp = ctx->top - 1 - up;
    return sp >= 0 ? ctx->stack[sp] : "";
}

static inline bool yxml_strequ(const char* s0, const char* s1) {
    uint32_t l0 = (uint32_t)strlen(s0);
    uint32_t l1 = (uint32_t)strlen(s1);
    return l0 == l1 && memcmp(s0, s1, l0) == 0;
}

static void yxml_element_start(yxml_t* x) {
    xml_context_t* ctx = (xml_context_t*)x;
	assert(yxml_symlen(x, x->elem) == strlen(x->elem));
    fatal_if(ctx->top >= countof(ctx->stack));
    ctx->stack[ctx->top++] = x->elem;
    for (int i = 0; i < ctx->npv_count && ctx->index < 0; i++) {
        // if .up == 0 both up and top are the same and are top
        //             element of the stack
        // if .up > 0  up could be empty "" or parent/grandparent
        //             of element
        // In XMP spec the elements may have rdf:containers as content
        //             but in the field broken writters may write single
        //             item container as direct content.
        const char* up  = yxml_parent(ctx, ctx->npv[i].up);
        const char* top = yxml_parent(ctx, 0);
        const char* name = ctx->npv[i].n;
        if (yxml_strequ(up, name) || yxml_strequ(top, name)) {
            const char* parent = ctx->npv[i].p;
            bool has_parent = parent == null; // parent was not required
            for (int p = ctx->top - 2; p >= 0 && !has_parent; p--) {
                has_parent = yxml_strequ(ctx->stack[p], parent);
            }
            if (has_parent) {
                assert(ctx->index < 0, "content nesting not supported");
                if (!ctx->npv[i].append || *ctx->npv[i].v == null || *ctx->npv[i].v[0] == 0) {
                    ctx->next++; // extra zero byte after zero separated appended rdf:li
                    *ctx->npv[i].v = ctx->next;
                } else { // appended values "\x00" separated:
                    fatal_if(ctx->next + 3 >= ctx->end);
                    memcpy(ctx->next, "\x00\x00", 2); // including terminating double zero 
                    ctx->next++; // only advance first "\x00" byte 
                }
                ctx->index = i;
//              if (strstr(top, "EventId") != null) { crt.breakpoint(); }
            }
        }
    }
}

static void yxml_content(yxml_t* x) {
    xml_context_t* ctx = (xml_context_t*)x;
    if (ctx->index >= 0) {
        char* p = ctx->yxml.data;
        if (ctx->next[0] == 0) { // skip starting white space bytes
            while (isspace(*(uint8_t*)p)) { p++; }
        }
        const int64_t k = strlen((char*)p);
        fatal_if(ctx->next + k + 1 >= ctx->end);
        memcpy(ctx->next, p, k + 1); // including terminating zero byte
        ctx->next += k;
    }
}

static void yxml_element_end(yxml_t* x) {
    xml_context_t* ctx = (xml_context_t*)x;
    assert(ctx->top > 0);
    const int i = ctx->index;
    if (i >= 0) {
        const char* top = yxml_parent(ctx, 0);
//      if (strstr(top, "EventId") != null) { crt.breakpoint(); }
        const char* name = ctx->npv[i].n;
        if (yxml_strequ(top, name)) {
            ctx->index = -1;
        }
        ctx->next++; // double zero byte termination
    }
    ctx->top--;
}

static void yxml_xmp(yxml_t* x, yxml_ret_t r) {
	switch(r) {
	    case YXML_ELEMSTART: yxml_element_start(x); break;
	    case YXML_CONTENT  : yxml_content(x);       break;
	    case YXML_ELEMEND  : yxml_element_end(x);   break;
        // ignored:
	    case YXML_ATTRSTART:
	    case YXML_ATTREND:
	    case YXML_PICONTENT:
	    case YXML_ATTRVAL:
	    case YXML_PISTART:
	    case YXML_PIEND:
	    case YXML_OK:
		    break;
	    default:
		    assert(false, "r: %d", r);
	}
}

static const char* rdf2str(const char* rdf) {
    static char append[1024];
    char* p = append;
    *p = 0;
    while (*rdf != 0) {
        char* s = (char*)rdf;
        const int64_t k = strlen(s);
        if (p == append) {
            p += snprintf(p, append + sizeof(append) - p, "%s", s);
        } else {
            p += snprintf(p, append + sizeof(append) - p, "<|>%s", s);
        }
        rdf += k + 1;
    }
    return append;
}

static void test_xml() {
    static char stack[64 * 1024];
    static char strings[64 * 1024];
    xml_context_t context = {0};
	yxml_ret_t r = 0;
	yxml_init(&context.yxml, stack, sizeof(stack));
    context.strings = strings;
    context.next    = strings;
    context.end     = strings + sizeof(strings);
    // nvp O(n^2) not a performance champion. Can be optimized using hashmaps
    xml_npv_t npv[] = {
        { "Iptc4xmpCore:CiAdrCity"  , &context.xmp.Iptc4xmpCore.CreatorContactInfo.CiAdrCity  },
        { "Iptc4xmpCore:CiAdrCtry"  , &context.xmp.Iptc4xmpCore.CreatorContactInfo.CiAdrCtry  },
        { "Iptc4xmpCore:CiAdrExtadr", &context.xmp.Iptc4xmpCore.CreatorContactInfo.CiAdrExtadr},
        { "Iptc4xmpCore:CiAdrPcode" , &context.xmp.Iptc4xmpCore.CreatorContactInfo.CiAdrPcode },
        { "Iptc4xmpCore:CiAdrRegion", &context.xmp.Iptc4xmpCore.CreatorContactInfo.CiAdrRegion},
        { "Iptc4xmpCore:CiEmailWork", &context.xmp.Iptc4xmpCore.CreatorContactInfo.CiEmailWork},
        { "Iptc4xmpCore:CiTelWork"  , &context.xmp.Iptc4xmpCore.CreatorContactInfo.CiTelWork  },
        { "Iptc4xmpCore:CiUrlWork"  , &context.xmp.Iptc4xmpCore.CreatorContactInfo.CiUrlWork  },
  
        { "Iptc4xmpCore:IntellectualGenre" , &context.xmp.Iptc4xmpCore.IntellectualGenre },
        { "Iptc4xmpCore:Location"          , &context.xmp.Iptc4xmpCore.Location },
        { "Iptc4xmpCore:Scene"             , &context.xmp.Iptc4xmpCore.Scene },
        { "Iptc4xmpCore:SubjectCode"       , &context.xmp.Iptc4xmpCore.SubjectCode },

        { "Iptc4xmpExt:AOCopyrightNotice"           , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOCopyrightNotice, 0, false, "Iptc4xmpExt:ArtworkOrObject" },
        { "Iptc4xmpExt:AOCreator"                   , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOCreator,         0, false, "Iptc4xmpExt:ArtworkOrObject" },
        { "Iptc4xmpExt:AOCircaDateCreated"          , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOCircaDateCreated },
        { "Iptc4xmpExt:AOContentDescription"        , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOContentDescription, 2 },
        { "Iptc4xmpExt:AOContributionDescription"   , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOContributionDescription, 2 },
        { "Iptc4xmpExt:AOCreatorId"                 , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOCreatorId, 2 },
                                                    
        { "Iptc4xmpExt:AOCurrentCopyrightOwnerId"   , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOCurrentCopyrightOwnerId },
        { "Iptc4xmpExt:AOCurrentCopyrightOwnerName" , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOCurrentCopyrightOwnerName },
        { "Iptc4xmpExt:AOCurrentLicensorId"         , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOCurrentLicensorId},
        { "Iptc4xmpExt:AOCurrentLicensorName"       , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOCurrentLicensorName},

        { "Iptc4xmpExt:AOPhysicalDescription"       , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOPhysicalDescription, 2 },
        { "Iptc4xmpExt:AOSource"                    , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOSource},
        { "Iptc4xmpExt:AOSourceInvNo"               , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOSourceInvNo},
        { "Iptc4xmpExt:AOSourceInvURL"              , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOSourceInvURL},
        { "Iptc4xmpExt:AOStylePeriod"               , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOStylePeriod, 2},
        { "Iptc4xmpExt:AOTitle"                     , &context.xmp.Iptc4xmpExt.ArtworkOrObject.AOTitle, 2},

        { "Iptc4xmpExt:DigImageGUID"                , &context.xmp.Iptc4xmpExt.DigImageGUID},
        { "Iptc4xmpExt:EmbdEncRightsExpr"           , &context.xmp.Iptc4xmpExt.EmbdEncRightsExpr, 2},
        { "Iptc4xmpExt:DigitalSourceType"           , &context.xmp.Iptc4xmpExt.DigitalSourceType},
        { "Iptc4xmpExt:Event"                       , &context.xmp.Iptc4xmpExt.Event, 2},
        { "Iptc4xmpExt:EventId"                     , &context.xmp.Iptc4xmpExt.EventId, 2},

        { "Iptc4xmpExt:LinkedRightsExpr"        , &context.xmp.Iptc4xmpExt.LinkedEncRightsExpr.LinkedRightsExpr,  0, false, "Iptc4xmpExt:LinkedEncRightsExpr"},
        { "Iptc4xmpExt:RightsExprEncType"       , &context.xmp.Iptc4xmpExt.LinkedEncRightsExpr.RightsExprEncType, 0, false, "Iptc4xmpExt:LinkedEncRightsExpr"},
        { "Iptc4xmpExt:RightsExprLangId"        , &context.xmp.Iptc4xmpExt.LinkedEncRightsExpr.RightsExprLangId,  0, false, "Iptc4xmpExt:LinkedEncRightsExpr"},

        { "Iptc4xmpExt:City"                    , &context.xmp.Iptc4xmpExt.LocationCreated.City,          0, false, "Iptc4xmpExt:LocationCreated"},
        { "Iptc4xmpExt:CountryCode"             , &context.xmp.Iptc4xmpExt.LocationCreated.CountryCode,   0, false, "Iptc4xmpExt:LocationCreated"},
        { "Iptc4xmpExt:CountryName"             , &context.xmp.Iptc4xmpExt.LocationCreated.CountryName,   0, false, "Iptc4xmpExt:LocationCreated"},
        { "Iptc4xmpExt:ProvinceState"           , &context.xmp.Iptc4xmpExt.LocationCreated.ProvinceState, 0, false, "Iptc4xmpExt:LocationCreated"},
        { "Iptc4xmpExt:Sublocation"             , &context.xmp.Iptc4xmpExt.LocationCreated.Sublocation,   0, false, "Iptc4xmpExt:LocationCreated"},
        { "Iptc4xmpExt:WorldRegion"             , &context.xmp.Iptc4xmpExt.LocationCreated.WorldRegion,   0, false, "Iptc4xmpExt:LocationCreated"},

        { "Iptc4xmpExt:City"                    , &context.xmp.Iptc4xmpExt.LocationShown.City,          0, false, "Iptc4xmpExt:LocationShown"},
        { "Iptc4xmpExt:CountryCode"             , &context.xmp.Iptc4xmpExt.LocationShown.CountryCode,   0, false, "Iptc4xmpExt:LocationShown"},
        { "Iptc4xmpExt:CountryName"             , &context.xmp.Iptc4xmpExt.LocationShown.CountryName,   0, false, "Iptc4xmpExt:LocationShown"},
        { "Iptc4xmpExt:ProvinceState"           , &context.xmp.Iptc4xmpExt.LocationShown.ProvinceState, 0, false, "Iptc4xmpExt:LocationShown"},
        { "Iptc4xmpExt:Sublocation"             , &context.xmp.Iptc4xmpExt.LocationShown.Sublocation,   0, false, "Iptc4xmpExt:LocationShown"},
        { "Iptc4xmpExt:WorldRegion"             , &context.xmp.Iptc4xmpExt.LocationShown.WorldRegion,   0, false, "Iptc4xmpExt:LocationShown"},

        { "Iptc4xmpExt:MaxAvailHeight"          , &context.xmp.Iptc4xmpExt.MaxAvailHeight},
        { "Iptc4xmpExt:MaxAvailWidth"           , &context.xmp.Iptc4xmpExt.MaxAvailWidth},
        { "Iptc4xmpExt:ModelAge"                , &context.xmp.Iptc4xmpExt.ModelAge},

        { "Iptc4xmpExt:PersonCharacteristic" , &context.xmp.Iptc4xmpExt.PersonInImageWDetails.PersonCharacteristic, 7, true, "Iptc4xmpExt:PersonInImageWDetails"},
        { "Iptc4xmpExt:PersonDescription"    , &context.xmp.Iptc4xmpExt.PersonInImageWDetails.PersonDescription,    2, true, "Iptc4xmpExt:PersonInImageWDetails"},
        { "Iptc4xmpExt:PersonId"             , &context.xmp.Iptc4xmpExt.PersonInImageWDetails.PersonId,             2, true, "Iptc4xmpExt:PersonInImageWDetails"},
        { "Iptc4xmpExt:PersonName"           , &context.xmp.Iptc4xmpExt.PersonInImageWDetails.PersonName,           2, true, "Iptc4xmpExt:PersonInImageWDetails"},

        { "Iptc4xmpExt:ProductDescription"   , &context.xmp.Iptc4xmpExt.ProductInImage.ProductDescription, 2, true,  "Iptc4xmpExt:ProductInImage"},
        { "Iptc4xmpExt:ProductGTIN"          , &context.xmp.Iptc4xmpExt.ProductInImage.ProductGTIN,        0, false, "Iptc4xmpExt:ProductInImage"},
        { "Iptc4xmpExt:ProductId"            , &context.xmp.Iptc4xmpExt.ProductInImage.ProductId,          0, false, "Iptc4xmpExt:ProductInImage"},
        { "Iptc4xmpExt:ProductName"          , &context.xmp.Iptc4xmpExt.ProductInImage.ProductName,        2, true,  "Iptc4xmpExt:ProductInImage"},

        { "Iptc4xmpExt:OrganisationInImageCode" , &context.xmp.Iptc4xmpExt.OrganisationInImageCode, 2},
        { "Iptc4xmpExt:OrganisationInImageName" , &context.xmp.Iptc4xmpExt.OrganisationInImageName, 2},
        { "Iptc4xmpExt:PersonInImage"           , &context.xmp.Iptc4xmpExt.PersonInImage,           2},
        { "Iptc4xmpExt:PersonCharacteristic"    , &context.xmp.Iptc4xmpExt.PersonCharacteristic,    2},
        { "Iptc4xmpExt:PersonDescription"       , &context.xmp.Iptc4xmpExt.PersonDescription,       2},

        { "Iptc4xmpExt:RegItemId"            , &context.xmp.Iptc4xmpExt.RegistryId.RegItemId},
        { "Iptc4xmpExt:RegOrgId"             , &context.xmp.Iptc4xmpExt.RegistryId.RegOrgId},

        { "Iptc4xmpExt:CvId"                 , &context.xmp.Iptc4xmpExt.AboutCvTerm.CvId,               0, false, "Iptc4xmpExt:AboutCvTerm"},
        { "Iptc4xmpExt:CvTermId"             , &context.xmp.Iptc4xmpExt.AboutCvTerm.CvTermId,           0, false, "Iptc4xmpExt:AboutCvTerm"},
        { "Iptc4xmpExt:CvTermName"           , &context.xmp.Iptc4xmpExt.AboutCvTerm.CvTermName,         2, true, "Iptc4xmpExt:AboutCvTerm"},
        { "Iptc4xmpExt:CvTermRefinedAbout"   , &context.xmp.Iptc4xmpExt.AboutCvTerm.CvTermRefinedAbout, 0, false, "Iptc4xmpExt:AboutCvTerm"},

        { "dc:creator"              , &context.xmp.dc.creator,      2, true },
        { "dc:date"                 , &context.xmp.dc.date,         2, true },
        { "dc:format"               , &context.xmp.dc.format},
        { "dc:description"          , &context.xmp.dc.description,  2, true },
        { "dc:rights"               , &context.xmp.dc.rights,       2, true },
        { "dc:subject"              , &context.xmp.dc.subject,      2, true },
        { "dc:title"                , &context.xmp.dc.title,        2, true },

        { "aux:Lens"                , &context.xmp.Lens },

        { "exif:GPSAltitude"        , &context.xmp.exif.GPSAltitude },
        { "exif:GPSAltitudeRef"     , &context.xmp.exif.GPSAltitudeRef },
        { "exif:GPSLatitude"        , &context.xmp.exif.GPSLatitude },
        { "exif:GPSLongitude"       , &context.xmp.exif.GPSLongitude },

        { "xmp:CreateDate"          , &context.xmp.CreateDate },
        { "xmp:CreatorTool"         , &context.xmp.CreatorTool },
        { "xmp:MetadataDate"        , &context.xmp.MetadataDate },
        { "xmp:ModifyDate"          , &context.xmp.ModifyDate },
        { "xmp:Rating"              , &context.xmp.Rating },

        // for disambiguation non-parented items must apprear last in the list
        { "Iptc4xmpExt:AOCopyrightNotice"           , &context.xmp.Iptc4xmpExt.AOCopyrightNotice },
        { "Iptc4xmpExt:AOCreator"                   , &context.xmp.Iptc4xmpExt.AOCreator         },
        { "Iptc4xmpExt:AODateCreated"               , &context.xmp.Iptc4xmpExt.AODateCreated },
        { "Iptc4xmpExt:AddlModelInfo"               , &context.xmp.Iptc4xmpExt.AddlModelInfo },

        { "Iptc4xmpCore:CountryCode", &context.xmp.Iptc4xmpCore.CountryCode }
    };
    context.index = -1;
    context.npv = npv;
    context.npv_count = countof(npv);
//  FILE* file = fopen("../metadata_test_file_IIM_XMP_EXIF.xml", "r");
    FILE* file = fopen("../IPTC-PhotometadataRef-Std2022.1.xml", "r");
	int c = getc(file);
    while (c != EOF && r >= 0) {
		r = yxml_parse(&context.yxml, c);
        if (r >= 0) {
		    yxml_xmp(&context.yxml, r);
            c = getc(file);
        }
	}
    fclose(file);
    assert(r == YXML_OK, "r: %d", r);
	bool ok = yxml_eof(&context.yxml) >= 0;
    for (int i = 0; i < context.npv_count; i++) {
        if (*context.npv[i].v == null || *context.npv[i].v[0] == 0) {
            *context.npv[i].v = ""; // much easier for the client
        } else { // trim white space. UTF-8 single characters can be negative
            // isspace() isspace returns true for space, form feed, line feed, 
            // carriage return, horizontal tab, and vertical tab. 
            // isblank() returns true only for space and horizontal tab.
            while (isspace((uint8_t)*context.npv[i].v[0])) {
                (*context.npv[i].v)++;
            }
            char* p = (char*)*context.npv[i].v + strlen(*context.npv[i].v) - 1;
            while (p >= *context.npv[i].v && isspace((uint8_t)*p)) {
                *p-- = 0;
            }
        }
    }
    context.npv = npv;
    context.npv_count = countof(npv);
    traceln("ok: %d", ok);
    #define dump(field) do { \
        /* assert(context.xmp.field != null && context.xmp.field[0] != 0); */ \
        traceln("%-55s: %s", #field, rdf2str(context.xmp.field)); \
    } while (0)
    dump(Iptc4xmpCore.CountryCode);
    dump(Iptc4xmpCore.CreatorContactInfo.CiAdrCity );
    dump(Iptc4xmpCore.CreatorContactInfo.CiAdrCtry );
    dump(Iptc4xmpCore.CreatorContactInfo.CiAdrExtadr);
    dump(Iptc4xmpCore.CreatorContactInfo.CiAdrPcode);
    dump(Iptc4xmpCore.CreatorContactInfo.CiAdrRegion);
    dump(Iptc4xmpCore.CreatorContactInfo.CiEmailWork);
    dump(Iptc4xmpCore.CreatorContactInfo.CiTelWork );
    dump(Iptc4xmpCore.CreatorContactInfo.CiUrlWork );
    dump(Iptc4xmpCore.IntellectualGenre);
    dump(Iptc4xmpCore.Location);
    dump(Iptc4xmpCore.Scene);
    dump(Iptc4xmpCore.SubjectCode);
    dump(Iptc4xmpExt.AOCopyrightNotice);
    dump(Iptc4xmpExt.AOCreator);
    dump(Iptc4xmpExt.AODateCreated);
    dump(Iptc4xmpExt.AddlModelInfo);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCopyrightNotice);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCreator);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCircaDateCreated);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCircaDateCreated);
    dump(Iptc4xmpExt.ArtworkOrObject.AOContentDescription);
    dump(Iptc4xmpExt.ArtworkOrObject.AOContributionDescription);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCreatorId);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCurrentCopyrightOwnerId);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCurrentCopyrightOwnerName);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCurrentLicensorId);
    dump(Iptc4xmpExt.ArtworkOrObject.AOCurrentLicensorName);
    dump(Iptc4xmpExt.ArtworkOrObject.AOPhysicalDescription);
    dump(Iptc4xmpExt.ArtworkOrObject.AOSource);
    dump(Iptc4xmpExt.ArtworkOrObject.AOSourceInvNo);
    dump(Iptc4xmpExt.ArtworkOrObject.AOSourceInvURL);
    dump(Iptc4xmpExt.ArtworkOrObject.AOStylePeriod);
    dump(Iptc4xmpExt.ArtworkOrObject.AOTitle);
    dump(Iptc4xmpExt.DigImageGUID);
    dump(Iptc4xmpExt.EmbdEncRightsExpr);
    dump(Iptc4xmpExt.DigitalSourceType);
    dump(Iptc4xmpExt.Event);
    dump(Iptc4xmpExt.EventId);
    dump(Iptc4xmpExt.LinkedEncRightsExpr.LinkedRightsExpr);
    dump(Iptc4xmpExt.LinkedEncRightsExpr.RightsExprEncType);
    dump(Iptc4xmpExt.LinkedEncRightsExpr.RightsExprLangId);
    dump(Iptc4xmpExt.LocationCreated.City);
    dump(Iptc4xmpExt.LocationCreated.CountryCode);
    dump(Iptc4xmpExt.LocationCreated.CountryName);
    dump(Iptc4xmpExt.LocationCreated.ProvinceState);
    dump(Iptc4xmpExt.LocationCreated.Sublocation);
    dump(Iptc4xmpExt.LocationCreated.WorldRegion);
    dump(Iptc4xmpExt.LocationShown.City);
    dump(Iptc4xmpExt.LocationShown.CountryCode);
    dump(Iptc4xmpExt.LocationShown.CountryName);
    dump(Iptc4xmpExt.LocationShown.ProvinceState);
    dump(Iptc4xmpExt.LocationShown.Sublocation);
    dump(Iptc4xmpExt.LocationShown.WorldRegion);
    dump(Iptc4xmpExt.MaxAvailHeight);
    dump(Iptc4xmpExt.MaxAvailWidth);
    dump(Iptc4xmpExt.ModelAge);
    dump(Iptc4xmpExt.OrganisationInImageCode);
    dump(Iptc4xmpExt.OrganisationInImageName);
    dump(Iptc4xmpExt.PersonInImage);
    dump(Iptc4xmpExt.PersonCharacteristic);
    dump(Iptc4xmpExt.PersonDescription);
    dump(Iptc4xmpExt.PersonInImageWDetails.PersonCharacteristic);
    dump(Iptc4xmpExt.PersonInImageWDetails.PersonDescription);
    dump(Iptc4xmpExt.PersonInImageWDetails.PersonId);
    dump(Iptc4xmpExt.PersonInImageWDetails.PersonName);
    dump(Iptc4xmpExt.ProductInImage.ProductDescription);
    dump(Iptc4xmpExt.ProductInImage.ProductGTIN);
    dump(Iptc4xmpExt.ProductInImage.ProductId);
    dump(Iptc4xmpExt.ProductInImage.ProductName);
    dump(Iptc4xmpExt.RegistryId.RegItemId);
    dump(Iptc4xmpExt.RegistryId.RegOrgId);
    dump(Iptc4xmpExt.AboutCvTerm.CvId);
    dump(Iptc4xmpExt.AboutCvTerm.CvTermId);
    dump(Iptc4xmpExt.AboutCvTerm.CvTermName);
    dump(Iptc4xmpExt.AboutCvTerm.CvTermRefinedAbout);
    dump(dc.creator);
    dump(dc.date);
    dump(dc.format);
    dump(dc.description);
    dump(dc.rights);
    dump(dc.subject);
    dump(dc.title);
    dump(Lens);
    dump(exif.GPSAltitude);
    dump(exif.GPSAltitudeRef);
    dump(exif.GPSLatitude);
    dump(exif.GPSLongitude);
    dump(CreateDate);
    dump(CreatorTool);
    dump(MetadataDate);
    dump(ModifyDate);
    dump(Rating);
}

// TODO: this is vast and not very useful in generic case: 
// <MicrosoftPhoto:Rating>0</MicrosoftPhoto:Rating>
// <aux:Lens>Samsung Galaxy S7 Rear Camera</aux:Lens>
// <xmpMM:History>
// <xmpRights:Marked>True</xmpRights:Marked>
// <xmpRights:UsageTerms>
// <crs:AlreadyApplied>True</crs:AlreadyApplied>  Adobe Camera Raw Settings
// <photomechanic:ColorClass>0</photomechanic:ColorClass>
// <photomechanic:EditStatus>edit status</photomechanic:EditStatus>
// <photomechanic:PMVersion>PM5</photomechanic:PMVersion>
// <photomechanic:Prefs>0:0:0:-00001</photomechanic:Prefs>
// <photomechanic:Tagged>False</photomechanic:Tagged>
// <photoshop:AuthorsPosition>stf</photoshop:AuthorsPosition>
// <photoshop:CaptionWriter>jp</photoshop:CaptionWriter>
// <photoshop:Category>Category</photoshop:Category>
// <photoshop:City>Anytown</photoshop:City>
// <photoshop:Country>United States</photoshop:Country>
// <photoshop:Credit>credit here</photoshop:Credit>
// <photoshop:DateCreated>2017-05-29T17:19:21-04:00</photoshop:DateCreated>
// <plus:CopyrightOwnerID>Default</plus:CopyrightOwnerID>
// <plus:CopyrightOwnerName>Joe Photographer</plus:CopyrightOwnerName>
// <photoshop:History>

static void init(void) {
    app.title = title;
    app.ui->layout = layout;
    app.ui->paint = paint;
    static uic_text(text, "Custom Photo Processor");
    static uic_t* children[] = { &text.ui, null };
    app.ui->children = children;
    bool test = args.option_bool(&app.argc, app.argv, "--test");
//  traceln("test: %d", test);
    test_xml();
//  if (app.argc > 0) exit(1);
//  if (app.argc > 0) exit(1);
//  test_exif("../metadata_test_file_IIM_XMP_EXIF.jpg");
//  test_exif("../IPTC-PhotometadataRef-Std2022.1.jpg");
    if (app.argc > 0) exit(1);
    if (test && app.argc > 1 && files.exists(app.argv[1]) && !files.is_folder(app.argv[1])) {
        test_exif(app.argv[1]);
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
