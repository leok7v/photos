#pragma once
/*
  TinyEXIF.h -- A simple ISO C++ library to parse basic EXIF and XMP
                information from a JPEG file.
  https://github.com/cdcseacave/TinyEXIF
  Copyright (c) 2015-2017 Seacave
  cdc.seacave@gmail.com
  All rights reserved.

  Based on the easyexif library (2013 version)
    https://github.com/mayanklahiri/easyexif
  of Mayank Lahiri (mlahiri@gmail.com).

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
  NO EVENT SHALL THE FREEBSD PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdbool.h>
#include <stdint.h>

enum ErrorCode {
    EXIF_PARSE_SUCCESS           = 0, // Parse EXIF and/or XMP was successful
    EXIF_PARSE_INVALID_JPEG      = 1, // No JPEG markers found in buffer, possibly invalid JPEG file
    EXIF_PARSE_UNKNOWN_BYTEALIGN = 2, // Byte alignment specified in EXIF file was unknown (neither Motorola nor Intel)
    EXIF_PARSE_ABSENT_DATA       = 3, // No EXIF and/or XMP data found in JPEG file
    EXIF_PARSE_CORRUPT_DATA      = 4, // EXIF and/or XMP header was found, but data was corrupted
};

enum FieldCode {
    EXIF_FIELD_NA                = 0, // No EXIF or XMP data
    EXIF_FIELD_EXIF              = (1 << 0), // EXIF data available
    EXIF_FIELD_XMP               = (1 << 1), // XMP data available
    EXIF_FIELD_ALL               = EXIF_FIELD_EXIF|EXIF_FIELD_XMP
};

typedef struct exif_stream_s exif_stream_t;

typedef struct exif_stream_s {
    // Return the pointer to the beginning of the desired size buffer
    // following current buffer position.
    const uint8_t* (*get)(exif_stream_t* stream, uint32_t bytes);
    // Advance current buffer position with the desired size;
    // return false if stream ends in less than the desired size.
    bool (*skip)(exif_stream_t* stream, uint32_t bytes);
} exif_stream_t;

// https://stackoverflow.com/questions/29001433/how-rdfbag-rdfseq-and-rdfalt-is-different-while-using-them

// xml_rdf_t represents rdf:Bag rdf:Seq and rdf:Alt
// interpretation is left to the client.
// UTF-8 "\x00\x00" terminated list of "\x00" terminated strings

typedef const char* exif_str_t;
typedef const char* exif_rdf_t; // similar to getenv() result

// DBL_MAX for all absent fields

typedef struct exif_info_s {
    bool has_app1;
    bool has_xmp;
    bool not_enough_memory;         // examine on error and resize something...
    bool dump;                      // client can set to true prio calling to debug
    char strings[64 * 1024];        // EXIF UTF-8 string storage (max APP1 segment is 64KB)
    char* next;                     // next unused EXIF UTF-8 string storage
    // Data fields
    uint32_t Fields;                // Store if EXIF and/or XMP data fields are available
    uint32_t ExifVersion;           // 4 ASCII bytes like "0220" with undefined type
    uint32_t ImageWidth;            // Image width reported in EXIF data
    uint32_t ImageHeight;           // Image height reported in EXIF data
    uint32_t RelatedImageWidth;     // Original image width reported in EXIF data
    uint32_t RelatedImageHeight;    // Original image height reported in EXIF data
    exif_str_t ImageDescription;    // Image description
    exif_str_t Artist;              // Artist
    exif_str_t UserComment;         // User Comment
    exif_str_t Make;                // Camera manufacturer's name
    exif_str_t Model;               // Camera model
    exif_str_t SerialNumber;        // Serial number of the body of the camera
    uint16_t Orientation;           // Image orientation, start of data corresponds to
                                    // 0: unspecified in EXIF data
                                    // 1: upper left of image
                                    // 3: lower right of image
                                    // 6: upper right of image
                                    // 8: lower left of image
                                    // 9: undefined
    double XResolution;             // Number of pixels per ResolutionUnit in the ImageWidth direction
    double YResolution;             // Number of pixels per ResolutionUnit in the ImageLength direction
    uint16_t ResolutionUnit;        // Unit of measurement for XResolution and YResolution
                                    // 1: no absolute unit of measurement. Used for images that may have
                                    //    a non-square aspect ratio, but no meaningful absolute dimensions
                                    // 2: inch
                                    // 3: centimeter
    uint16_t   BitsPerSample;       // Number of bits per component
    exif_str_t Software;            // Software used
    exif_str_t DateTime;            // File change date and time
    exif_str_t DateTimeOriginal;    // Original file date and time (may not be present)
    exif_str_t DateTimeDigitized;   // Digitization date and time (may not be present)
    exif_str_t SubSecTimeOriginal;  // Sub-second time that original picture was taken
    exif_str_t Copyright;           // File copyright information
    exif_str_t OffsetTimeOriginal;  // "+00:00" (~timezone)
    uint32_t   FlashPixVersion;     // undefined type 4 bytes, The Flashpix format version supported by a FPXR file
    double     ExposureTime;        // Exposure time in seconds
    double     FNumber;             // F/stop
    uint16_t   ExposureProgram;     // Exposure program
                                    // 0: not defined
                                    // 1: manual
                                    // 2: normal program
                                    // 3: aperture priority
                                    // 4: shutter priority
                                    // 5: creative program
                                    // 6: action program
                                    // 7: portrait mode
                                    // 8: landscape mode
    uint16_t ColorSpace;            // ==1 for sRGB
    uint16_t SceneType;             // If a DSC recorded the image, this tag value shall
                                    // always be set to 1, indicating that the image was
                                    // directly photographed.
    uint16_t ExposureMode;          // 	Indicates the exposure mode set when the image was shot.
                                    // 0 = Auto exposure
                                    // 1 = Manual exposure
                                    // 2 = Auto bracket
    uint16_t WhiteBalance;          // 0 = Auto white balance
                                    // 1 = Manual white balance
    uint16_t CaptureType;           // 0 = Standard
                                    // 1 = Landscape
                                    // 2 = Portrait
                                    // 3 = Night scene
    uint16_t ComponentConfig;       // 0 = -
                                    // 1 = Y
                                    // 2 = Cb
                                    // 3 = Cr
                                    // 4 = R
                                    // 5 = G
                                    // 6 = B
    uint16_t YCCPositioning;        // Specifies the positioning of subsampled chrominance
                                    // components relative to luminance samples.
    uint16_t SensingMethod;         // Indicates the image sensor type on the camera or input device.
                                    // 1 = Not defined
                                    // 2 = One-chip color area sensor
                                    // 3 = Two-chip color area sensor
                                    // 4 = Three-chip color area sensor
                                    // 5 = Color sequential area sensor
                                    // 7 = Trilinear sensor
                                    // 8 = Color sequential linear sensor
    uint16_t ISOSpeedRatings;       // ISO speed
    double ShutterSpeedValue;       // Shutter speed (reciprocal of exposure time)
    double MaxAperture;             // Maximu lens aperture
    double ApertureValue;           // The lens aperture
    double BrightnessValue;         // The value of brightness
    double ExposureBiasValue;       // Exposure bias value in EV
    double SubjectDistance;         // Distance to focus point in meters
    double FocalLength;             // Focal length of lens in millimeters
    uint16_t Flash;                 // Flash info
                                    // Flash used (Flash&1)
                                    // 0: no flash, >0: flash used
                                    // Flash returned light status ((Flash & 6) >> 1)
                                    // 0: no strobe return detection function
                                    // 1: reserved
                                    // 2: strobe return light not detected
                                    // 3: strobe return light detected
                                    // Flash mode ((Flash & 24) >> 3)
                                    // 0: unknown
                                    // 1: compulsory flash firing
                                    // 2: compulsory flash suppression
                                    // 3: auto mode
                                    // Flash function ((Flash & 32) >> 5)
                                    // 0: flash function present, >0: no flash function
                                    // Flash red-eye ((Flash & 64) >> 6)
                                    // 0: no red-eye reduction mode or unknown, >0: red-eye reduction supported
    uint16_t MeteringMode;          // Metering mode
                                    // 0: unknown
                                    // 1: average
                                    // 2: center weighted average
                                    // 3: spot
                                    // 4: multi-spot
                                    // 5: pattern
                                    // 6: partial
    uint16_t LightSource;           // Kind of light source
                                    // 0: unknown
                                    // 1: daylight
                                    // 2: fluorescent
                                    // 3: tungsten (incandescent light)
                                    // 4: flash
                                    // 9: fine weather
                                    // 10: cloudy weather
                                    // 11: shade
                                    // 12: daylight fluorescent (D 5700 - 7100K)
                                    // 13: day white fluorescent (N 4600 - 5400K)
                                    // 14: cool white fluorescent (W 3900 - 4500K)
                                    // 15: white fluorescent (WW 3200 - 3700K)
                                    // 17: standard light A
                                    // 18: standard light B
                                    // 19: standard light C
                                    // 20: D55
                                    // 21: D65
                                    // 22: D75
                                    // 23: D50
                                    // 24: ISO studio tungsten
    uint16_t ProjectionType;        // Projection type
                                    // 0: unknown projection
                                    // 1: perspective projection
                                    // 2: equirectangular/spherical projection
    uint16_t SubjectAreas;          // number of subject areas https://www.awaresystems.be/imaging/tiff/tifftags/privateifd/exif/subjectarea.html
                                    // 0: unknown
                                    // 2: location of the main subject as coordinates
                                    //    (first value is the X coordinate and second is the Y coordinate)
                                    // 3: area of the main subject as a circle (first value is the center
                                    //    X coordinate, second is the center Y coordinate, and third is the diameter)
                                    // 4: area of the main subject as a rectangle (first value is the center
                                    //    X coordinate, second is the center Y coordinate, third is the width
                                    //    of the area, and fourth is the height of the area)
    uint16_t SubjectArea[4];        // Location and area of the main subject in the overall scene expressed
                                    // in relation to the upper left as origin, prior to rotation

    struct Calibration_t {          // Camera calibration information
        double FocalLength;         // Focal length (pixels)
        double OpticalCenterX;      // Principal point X (pixels)
        double OpticalCenterY;      // Principal point Y (pixels)
    } Calibration;

    struct LensInfo_t {                    // Lens information
        double FStopMin;                   // Min aperture (f-stop)
        double FStopMax;                   // Max aperture (f-stop)
        double FocalLengthMin;             // Min focal length (mm)
        double FocalLengthMax;             // Max focal length (mm)
        double DigitalZoomRatio;           // Digital zoom ratio when the image was shot
        double FocalLengthIn35mm;          // Focal length in 35mm film
        double FocalPlaneXResolution;      // Number of pixels in the image width (X) direction
                                           // per FocalPlaneResolutionUnit on the camera focal plane (may not be present)
        double FocalPlaneYResolution;      // Number of pixels in the image width (Y) direction per FocalPlaneResolutionUnit
                                          // on the camera focal plane (may not be present)
        uint16_t FocalPlaneResolutionUnit; // Unit for measuring FocalPlaneXResolution and
                                           // FocalPlaneYResolution (may not be present)
                                           // 0: unspecified in EXIF data
                                           // 1: no absolute unit of measurement
                                           // 2: inch
                                           // 3: centimeter
        exif_str_t Make;                   // Lens manufacturer
        exif_str_t Model;                  // Lens model
    } LensInfo;

    struct Geolocation_t {          // GPS information embedded in file
        double Latitude;            // Image latitude expressed as decimal
        double Longitude;           // Image longitude expressed as decimal
        double Altitude;            // Altitude in meters, relative to sea level
        int8_t AltitudeRef;         // 0: above sea level, -1: below sea level
        double RelativeAltitude;    // Relative altitude in meters
        double RollDegree;          // Flight roll in degrees
        double PitchDegree;         // Flight pitch in degrees
        double YawDegree;           // Flight yaw in degrees
        double SpeedX;              // Flight speed on X in meters/second
        double SpeedY;              // Flight speed on Y in meters/second
        double SpeedZ;              // Flight speed on Z in meters/second
        double AccuracyXY;          // GPS accuracy on XY in meters
        double AccuracyZ;           // GPS accuracy on Z in meters
        double GPSDOP;              // GPS DOP (data degree of precision)
        uint16_t GPSDifferential;   // Differential correction applied to the GPS receiver (may not be present)
                                    // 0: measurement without differential correction
                                    // 1: differential correction applied
        exif_str_t GPSMapDatum;     // Geodetic survey data (may not be present)
        exif_str_t GPSTimeStamp;    // Time as UTC (Coordinated Universal Time) (may not be present)
        exif_str_t GPSDateStamp;    // A character string recording date and time information relative
                                    // to UTC (Coordinated Universal Time) YYYY:MM:DD (may not be present)
        struct Coord_t {
            double degrees;
            double minutes;
            double seconds;
            uint8_t direction;
        } LatComponents, LonComponents; // Latitude/Longitude expressed in deg/min/sec
    } GeoLocation;

    struct GPano_t {                    // Spherical metadata. https://developers.google.com/streetview/spherical-metadata
        double PosePitchDegrees;        // Pitch, measured in degrees above the horizon, for the center in the image.
                                        // Value must be >= -90 and <= 90.
        double PoseRollDegrees;         // Roll, measured in degrees, of the image where level with the horizon is 0.
                                        // As roll increases, the horizon rotates counterclockwise in the image.
                                        // Value must be > -180 and <= 180.
    } GPano;

    struct MicroVideo_t {               // Google camera video file in metadata
        uint32_t HasMicroVideo;         // not zero if exists
        uint32_t MicroVideoVersion;     // just regularinfo
        uint32_t MicroVideoOffset;      // offset from end of file
    } MicroVideo;

    struct {
        struct {
            exif_str_t CountryCode;
            struct {
                exif_str_t CiAdrCity;
                exif_str_t CiAdrCtry;
                exif_str_t CiAdrExtadr;
                exif_str_t CiAdrPcode;
                exif_str_t CiAdrRegion;
                exif_str_t CiEmailWork;
                exif_str_t CiTelWork;
                exif_str_t CiUrlWork;
            } CreatorContactInfo;
            exif_str_t IntellectualGenre;
            exif_str_t Location;
            exif_str_t Scene;
            exif_str_t SubjectCode;
        } Iptc4xmpCore;
        struct {
            // see: https://github.com/Exiv2/exiv2/issues/1959 for the problems
            // https://www.iptc.org/std/photometadata/examples/
            // with ArtworkOrObject disambiguations...
            exif_str_t AOCopyrightNotice;
            exif_rdf_t AOCreator;
            exif_str_t AODateCreated;  // "2017-05-29T17:19:21-0400"
            exif_str_t AddlModelInfo;
            struct {
                exif_str_t AOCircaDateCreated;
                exif_rdf_t AOContentDescription;
                exif_rdf_t AOContributionDescription;
                exif_str_t AOCopyrightNotice;
                exif_rdf_t AOCreator;
                exif_rdf_t AOCreatorId;
                exif_str_t AOCurrentCopyrightOwnerId;
                exif_str_t AOCurrentCopyrightOwnerName;
                exif_str_t AOCurrentLicensorId;
                exif_str_t AOCurrentLicensorName;
                exif_rdf_t AOPhysicalDescription;
                exif_str_t AOSource;
                exif_str_t AOSourceInvNo;
                exif_str_t AOSourceInvURL;
                exif_rdf_t AOStylePeriod;
                exif_rdf_t AOTitle;
            } ArtworkOrObject;
            exif_str_t DigImageGUID;
            exif_rdf_t EmbdEncRightsExpr;
            exif_str_t DigitalSourceType;
            exif_rdf_t Event;
            exif_rdf_t EventId;
            struct { // TODO: this is actually bag of triplets
                exif_str_t LinkedRightsExpr;
                exif_str_t RightsExprEncType;
                exif_str_t RightsExprLangId;
            } LinkedEncRightsExpr;
            struct {
                exif_str_t City;
                exif_str_t CountryCode;
                exif_str_t CountryName;
                exif_str_t ProvinceState;
                exif_str_t Sublocation;
                exif_str_t WorldRegion;
            } LocationCreated;
            struct {
                exif_str_t City;
                exif_str_t CountryCode;
                exif_str_t CountryName;
                exif_str_t ProvinceState;
                exif_str_t Sublocation;
                exif_str_t WorldRegion;
            } LocationShown;
            exif_str_t MaxAvailHeight;
            exif_str_t MaxAvailWidth;
            exif_rdf_t ModelAge;
            exif_rdf_t OrganisationInImageCode;
            exif_rdf_t OrganisationInImageName;
            exif_rdf_t PersonInImage;
            exif_rdf_t PersonCharacteristic;
            exif_rdf_t PersonDescription;
            struct { // TODO: should be PersonInImageDetails[count]
                exif_rdf_t PersonCharacteristic;
                exif_rdf_t PersonDescription;
                exif_rdf_t PersonId;
                exif_rdf_t PersonName;
            } PersonInImageWDetails;
            struct { // TODO: should be PersonInImageDetails[count]
                exif_rdf_t ProductDescription;
                exif_str_t ProductGTIN;
                exif_str_t ProductId;
                exif_rdf_t ProductName;
            } ProductInImage;
            struct {
                exif_str_t RegItemId;
                exif_str_t RegOrgId;
            } RegistryId;
            struct {
                exif_str_t CvId;
                exif_str_t CvTermId;
                exif_rdf_t CvTermName;
                exif_str_t CvTermRefinedAbout;
            } AboutCvTerm;
        } Iptc4xmpExt;
        struct {
            exif_str_t creator;
            exif_str_t date;   // "2017-05-29T17:19:21-0400"
            exif_str_t format; // "image/jpeg"
            exif_str_t description;
            exif_str_t rights;
            exif_rdf_t title;
            exif_rdf_t subject;
        } dc;
        struct {
            exif_str_t GPSAltitude;    // "0/10"
            exif_str_t GPSAltitudeRef; // "0"
            exif_str_t GPSLatitude;    // "26,34.951N"
            exif_str_t GPSLongitude;   // "80,12.014W"
        } exif;
//      struct {                      // TODO: complicated
//          int32_n count;
//          struct {
//              exif_rdf_t Name;
//              exif_rdf_t Role;
//          } contributors;
//      } Contributor;
        exif_str_t CreateDate;
        exif_str_t CreatorTool;
        exif_str_t MetadataDate;
        exif_str_t ModifyDate;
        exif_str_t Rating;
        exif_str_t Lens; // <aux:Lens>Samsung Galaxy S7 Rear Camera</aux:Lens>
    } xmp;
} exif_info_t;

#ifdef __cplusplus
extern "C" {
#endif

// return 0 on success
int exif_from_memory(exif_info_t* ei, const uint8_t* data, uint32_t bytes);
int exif_from_stream(exif_info_t* ei, exif_stream_t* stream);

#ifdef __cplusplus
}
#endif

