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

#ifndef __TINYEXIF_H__
#define __TINYEXIF_H__

#include <string>
#include <vector>

#define TINYEXIF_MAJOR_VERSION 1
#define TINYEXIF_MINOR_VERSION 0
#define TINYEXIF_PATCH_VERSION 1

#define TINYEXIF_NO_XMP_SUPPORT

enum ErrorCode {
    PARSE_SUCCESS           = 0, // Parse EXIF and/or XMP was successful
    PARSE_INVALID_JPEG      = 1, // No JPEG markers found in buffer, possibly invalid JPEG file
    PARSE_UNKNOWN_BYTEALIGN = 2, // Byte alignment specified in EXIF file was unknown (neither Motorola nor Intel)
    PARSE_ABSENT_DATA       = 3, // No EXIF and/or XMP data found in JPEG file
    PARSE_CORRUPT_DATA      = 4, // EXIF and/or XMP header was found, but data was corrupted
};

enum FieldCode {
    FIELD_NA                 = 0, // No EXIF or XMP data
    FIELD_EXIF               = (1 << 0), // EXIF data available
    FIELD_XMP                = (1 << 1), // XMP data available
    FIELD_ALL                = FIELD_EXIF|FIELD_XMP
};

class EntryParser;

//
// Interface class responsible for fetching stream data to be parsed
//
class EXIFStream {
public:
    virtual ~EXIFStream() {}

    // Check the state of the stream.
    virtual bool IsValid() const = 0;

    // Return the pointer to the beginning of the desired size buffer
    // following current buffer position.
    virtual const uint8_t* GetBuffer(unsigned desiredLength) = 0;

    // Advance current buffer position with the desired size;
    // return false if stream ends in less than the desired size.
    virtual bool SkipBuffer(unsigned desiredLength) = 0;
};

//
// Class responsible for storing and parsing EXIF & XMP metadata from a JPEG stream
//
class EXIFInfo {

public:
    EXIFInfo();
    EXIFInfo(EXIFStream& stream);
    EXIFInfo(std::istream& stream); // NB: the stream must have been opened in binary mode
    EXIFInfo(const uint8_t* data, unsigned length);

    // Parsing function for an entire JPEG image stream.
    //
    // PARAM 'stream': Interface to fetch JPEG image stream.
    // PARAM 'data': A pointer to a JPEG image.
    // PARAM 'length': The length of the JPEG image.
    // RETURN:  PARSE_SUCCESS (0) on success with 'result' filled out
    //          error code otherwise, as defined by the PARSE_* macros
    int parseFrom(EXIFStream& stream);
    int parseFrom(std::istream& stream); // NB: the stream must have been opened in binary mode
    int parseFrom(const uint8_t* data, unsigned length);

    // Parsing function for an EXIF segment. This is used internally by parseFrom()
    // but can be called for special cases where only the EXIF section is 
    // available (i.e., a blob starting with the bytes "Exif\0\0").
    int parseFromEXIFSegment(const uint8_t* buf, unsigned len);

#ifndef TINYEXIF_NO_XMP_SUPPORT
    // Parsing function for an XMP segment. This is used internally by parseFrom()
    // but can be called for special cases where only the XMP section is 
    // available (i.e., a blob starting with the bytes "http://ns.adobe.com/xap/1.0/\0").
    int parseFromXMPSegment(const uint8_t* buf, unsigned len);
    int parseFromXMPSegmentXML(const char* szXML, unsigned len);
#endif // TINYEXIF_NO_XMP_SUPPORT

    // Set all data members to default values.
    // Should be called before parsing a new stream.
    void clear();

private:
    // Parse tag as Image IFD.
    void parseIFDImage(EntryParser&, unsigned&, unsigned&);
    // Parse tag as Exif IFD.
    void parseIFDExif(EntryParser&);
    // Parse tag as GPS IFD.
    void parseIFDGPS(EntryParser&);
    // Parse tag as MakerNote IFD.
    void parseIFDMakerNote(EntryParser&);

public:
    // Data fields
    uint32_t Fields = 0;                // Store if EXIF and/or XMP data fields are available
    uint32_t ImageWidth = 0;            // Image width reported in EXIF data
    uint32_t ImageHeight = 0;           // Image height reported in EXIF data
    uint32_t RelatedImageWidth = 0;     // Original image width reported in EXIF data
    uint32_t RelatedImageHeight = 0;    // Original image height reported in EXIF data
    std::string ImageDescription;       // Image description
    std::string Make;                   // Camera manufacturer's name
    std::string Model;                  // Camera model
    std::string SerialNumber;           // Serial number of the body of the camera
    uint16_t Orientation = 0;           // Image orientation, start of data corresponds to
                                        // 0: unspecified in EXIF data
                                        // 1: upper left of image
                                        // 3: lower right of image
                                        // 6: upper right of image
                                        // 8: lower left of image
                                        // 9: undefined
    double XResolution = 0;             // Number of pixels per ResolutionUnit in the ImageWidth direction
    double YResolution = 0;             // Number of pixels per ResolutionUnit in the ImageLength direction
    uint16_t ResolutionUnit = 0;        // Unit of measurement for XResolution and YResolution
                                        // 1: no absolute unit of measurement. Used for images that may have a non-square aspect ratio, but no meaningful absolute dimensions
                                        // 2: inch
                                        // 3: centimeter
    uint16_t BitsPerSample = 0;         // Number of bits per component
    std::string Software;               // Software used
    std::string DateTime;               // File change date and time
    std::string DateTimeOriginal;       // Original file date and time (may not exist)
    std::string DateTimeDigitized;      // Digitization date and time (may not exist)
    std::string SubSecTimeOriginal;     // Sub-second time that original picture was taken
    std::string Copyright;              // File copyright information
    double ExposureTime = 0;            // Exposure time in seconds
    double FNumber = 0;                 // F/stop
    uint16_t ExposureProgram = 0;       // Exposure program
                                        // 0: not defined
                                        // 1: manual
                                        // 2: normal program
                                        // 3: aperture priority
                                        // 4: shutter priority
                                        // 5: creative program
                                        // 6: action program
                                        // 7: portrait mode
                                        // 8: landscape mode
    uint16_t ISOSpeedRatings = 0;       // ISO speed
    double ShutterSpeedValue = 0;       // Shutter speed (reciprocal of exposure time)
    double ApertureValue = 0;           // The lens aperture
    double BrightnessValue = 0;         // The value of brightness
    double ExposureBiasValue = 0;       // Exposure bias value in EV
    double SubjectDistance = 0;         // Distance to focus point in meters
    double FocalLength = 0;             // Focal length of lens in millimeters
    uint16_t Flash = 0;                 // Flash info
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
    uint16_t MeteringMode = 0;          // Metering mode
                                        // 0: unknown
                                        // 1: average
                                        // 2: center weighted average
                                        // 3: spot
                                        // 4: multi-spot
                                        // 5: pattern
                                        // 6: partial
    uint16_t LightSource = 0;           // Kind of light source
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
    uint16_t ProjectionType = 0;        // Projection type
                                        // 0: unknown projection
                                        // 1: perspective projection
                                        // 2: equirectangular/spherical projection
    std::vector<uint16_t> SubjectArea;  // Location and area of the main subject in the overall scene expressed in relation to the upper left as origin, prior to rotation
                                        // 0: unknown
                                        // 2: location of the main subject as coordinates (first value is the X coordinate and second is the Y coordinate)
                                        // 3: area of the main subject as a circle (first value is the center X coordinate, second is the center Y coordinate, and third is the diameter)
                                        // 4: area of the main subject as a rectangle (first value is the center X coordinate, second is the center Y coordinate, third is the width of the area, and fourth is the height of the area)
    struct Calibration_t {              // Camera calibration information
        double FocalLength = 0;         // Focal length (pixels)
        double OpticalCenterX = 0;      // Principal point X (pixels)
        double OpticalCenterY = 0;      // Principal point Y (pixels)
    } Calibration;
    struct LensInfo_t {                        // Lens information
        double FStopMin = 0;                   // Min aperture (f-stop)
        double FStopMax = 0;                   // Max aperture (f-stop)
        double FocalLengthMin = 0;             // Min focal length (mm)
        double FocalLengthMax = 0;             // Max focal length (mm)
        double DigitalZoomRatio = 0;           // Digital zoom ratio when the image was shot
        double FocalLengthIn35mm = 0;          // Focal length in 35mm film
        double FocalPlaneXResolution = 0;      // Number of pixels in the image width (X) direction per FocalPlaneResolutionUnit on the camera focal plane (may not exist)
        double FocalPlaneYResolution = 0;      // Number of pixels in the image width (Y) direction per FocalPlaneResolutionUnit on the camera focal plane (may not exist)
        uint16_t FocalPlaneResolutionUnit = 0; // Unit for measuring FocalPlaneXResolution and FocalPlaneYResolution (may not exist)
                                               // 0: unspecified in EXIF data
                                               // 1: no absolute unit of measurement
                                               // 2: inch
                                               // 3: centimeter
        std::string Make;                      // Lens manufacturer
        std::string Model;                     // Lens model
    } LensInfo;
    struct Geolocation_t {              // GPS information embedded in file
        double Latitude = 0;            // Image latitude expressed as decimal
        double Longitude = 0;           // Image longitude expressed as decimal
        double Altitude = 0;            // Altitude in meters, relative to sea level
        int8_t AltitudeRef = 0;         // 0: above sea level, -1: below sea level
        double RelativeAltitude = 0;    // Relative altitude in meters
        double RollDegree = 0;          // Flight roll in degrees
        double PitchDegree = 0;         // Flight pitch in degrees
        double YawDegree = 0;           // Flight yaw in degrees
        double SpeedX = 0;              // Flight speed on X in meters/second
        double SpeedY = 0;              // Flight speed on Y in meters/second
        double SpeedZ = 0;              // Flight speed on Z in meters/second
        double AccuracyXY = 0;          // GPS accuracy on XY in meters
        double AccuracyZ = 0;           // GPS accuracy on Z in meters
        double GPSDOP = 0;              // GPS DOP (data degree of precision)
        uint16_t GPSDifferential = 0;   // Differential correction applied to the GPS receiver (may not exist)
                                        // 0: measurement without differential correction
                                        // 1: differential correction applied 
        std::string GPSMapDatum;        // Geodetic survey data (may not exist)
        std::string GPSTimeStamp;       // Time as UTC (Coordinated Universal Time) (may not exist)
        std::string GPSDateStamp;       // A character string recording date and time information relative to UTC (Coordinated Universal Time) YYYY:MM:DD (may not exist)
        struct Coord_t {
            double degrees = 0;
            double minutes = 0;
            double seconds = 0;
            uint8_t direction = 0;
        } LatComponents, LonComponents;     // Latitude/Longitude expressed in deg/min/sec
        void parseCoords();                 // Convert Latitude/Longitude from deg/min/sec to decimal
        bool hasLatLon() const;             // Return true if (lat,lon) is available
        bool hasAltitude() const;           // Return true if (alt) is available
        bool hasRelativeAltitude()const;    // Return true if (rel_alt) is available
        bool hasOrientation() const;        // Return true if (roll,yaw,pitch) is available
        bool hasSpeed() const;              // Return true if (speedX,speedY,speedZ) is available
    } GeoLocation;
    struct GPano_t {                        // Spherical metadata. https://developers.google.com/streetview/spherical-metadata
        double PosePitchDegrees = 0;        // Pitch, measured in degrees above the horizon, for the center in the image. Value must be >= -90 and <= 90.
        double PoseRollDegrees = 0;         // Roll, measured in degrees, of the image where level with the horizon is 0. As roll increases, the horizon rotates counterclockwise in the image. Value must be > -180 and <= 180.
        bool hasPosePitchDegrees() const;   // Return true if PosePitchDegrees is available
        bool hasPoseRollDegrees() const;    // Return true if PoseRollDegrees is available
    } GPano;
    struct MicroVideo_t {                   // Google camera video file in metadata
        uint32_t HasMicroVideo = 0;         // not zero if exists
        uint32_t MicroVideoVersion = 0;     // just regularinfo
        uint32_t MicroVideoOffset = 0;      // offset from end of file
    } MicroVideo;
};

#endif // __TINYEXIF_H__