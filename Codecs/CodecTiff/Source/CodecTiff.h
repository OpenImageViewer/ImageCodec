#pragma once

#include <IImagePlugin.h>

#include <tiffio.h>
#include <tiffio.hxx>
#include "TiffClientFunctions.h"
#include "TiffFile.h"
#include <LLUtils/Exception.h>

namespace IMCodec
{
    class CodecTiff : public IImagePlugin
    {
    private:
        PluginProperties mPluginProperties;
    public:

        CodecTiff() : mPluginProperties({ "LibTiff image codec","tif;tiff" })
        {

        }

        PluginProperties& GetPluginProperties() override
        {
            return mPluginProperties;
        }

        TexelFormat GetTexelFormat(uint16_t sampleFormat, uint16 bitsPerSample) const
        {
            TexelFormat texelFormat = TexelFormat::UNKNOWN;
            switch (sampleFormat)
            {
            case SAMPLEFORMAT_IEEEFP:

                switch (bitsPerSample)
                {
                case 16:
                    texelFormat = TexelFormat::F_X16;
                    break;
                case 24:
                    texelFormat = TexelFormat::F_X24;
                    break;
                case 32:
                    texelFormat = TexelFormat::F_X32;
                    break;
                default:
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, "CodecTiff: unsupported floating point format.");
                }
                break;


            case SAMPLEFORMAT_UINT:
                switch (bitsPerSample)
                {
                case 8:
                    texelFormat = TexelFormat::I_X8;
                    break;
                default:
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, "CodecTiff: unsupported integer format.");
                }
                break;
            default:
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, "CodecTiff: unsupported type format.");
            }

            return texelFormat;
        }
        //Base abstract methods
        bool LoadImage(const uint8_t* buffer, std::size_t size, ImageDescriptor& out_properties) override
        {
            using namespace std;

            bool success = false;
            TiffFile tifFile(buffer, size);
            TIFF* tiff = tifFile.GetTiff();

            if (tiff != nullptr)
            {
                uint16_t sampleFormat = 0;

                uint32 width, height;
                uint16 bitsPerSample;
                uint16 compression;
                uint16 photoMetric;
                uint16 samplesPerPixel;

                TexelFormat texelFormat = TexelFormat::UNKNOWN;
                uint32_t rowPitch;
                uint32 rowsPerStrip;
                uint16_t stripRowCount = 0;


                TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
                TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
                TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
                TIFFGetField(tiff, TIFFTAG_COMPRESSION, &compression);
                TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &photoMetric);
                TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
                
                uint32_t bytesPerSample = bitsPerSample / 8;

                //TODO: use this field to read extra sample info e.g. transparency channel
                //TIFFGetField(tiff, TIFFTAG_EXTRASAMPLES, &extra);
                
                /* Default sample format is unsigned integer.
                   From tiff specification: 
                   Section 19: Data Sample Format
                    This section describes a scheme for specifying data sample type information.
                    TIFF implicitly types all data samples as unsigned integer values.Certain applications,
                    however, require the ability to store image - related data in other formats
                    such as floating point.This section presents a scheme for describing a variety of
                    data sample formats.*/

                if (TIFFGetField(tiff, TIFFTAG_SAMPLEFORMAT, &sampleFormat) == 0)
                    sampleFormat = SAMPLEFORMAT_UINT;
                
                TIFFGetField(tiff, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip);
                TIFFGetField(tiff, TIFFTAG_STRIPROWCOUNTS, &stripRowCount);

                uint32_t numberOfStripts = TIFFNumberOfStrips(tiff);
                tmsize_t stripSize = TIFFStripSize(tiff);
                rowPitch = static_cast<uint32_t>(TIFFScanlineSize(tiff));
                
                switch (photoMetric)
                {
                case PHOTOMETRIC_RGB:
                    texelFormat = TexelFormat::I_R8_G8_B8_A8;
                    //override target row pitch - always 32bpp
                    rowPitch = width * 4;
                    out_properties.fData.Allocate(height * rowPitch);
                    
                    TIFFReadRGBAImage(tiff, width, height, reinterpret_cast<uint32*>(out_properties.fData.data()));
                    break;
                case PHOTOMETRIC_MINISWHITE:
                case PHOTOMETRIC_MINISBLACK:
                        texelFormat = GetTexelFormat(sampleFormat, bitsPerSample);
                        out_properties.fData.Allocate(height * rowPitch);

                        uint8_t* currensPos = reinterpret_cast<uint8_t*>(out_properties.fData.data());

                        if (stripSize != rowPitch * rowsPerStrip)
                            LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, "CodecTiff: unsupported strip size.");

                        for (uint32_t i = 0; i < numberOfStripts; i++)
                        {
                            TIFFReadEncodedStrip(tiff, i, currensPos, stripSize);
                            currensPos += rowPitch * rowsPerStrip;
                        }
                        
                        

                    break;
                }

                success = true;
                out_properties.fProperties.Width = width;
                out_properties.fProperties.Height = height;
                out_properties.fProperties.NumSubImages = 0;
                out_properties.fProperties.TexelFormatDecompressed = texelFormat;
                out_properties.fProperties.RowPitchInBytes = rowPitch;

				// Two or more image channels are interleaved
                // Extract the first one, currently discard the others.
                // TODO: return extra channel as subimage.				
                if (samplesPerPixel > 1 
                    && (photoMetric == PHOTOMETRIC_MINISWHITE || photoMetric == PHOTOMETRIC_MINISBLACK))
                {

                    //if (photoMetric != PHOTOMETRIC_MINISWHITE && photoMetric != PHOTOMETRIC_MINISBLACK)
                      //  LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, "Extracing channel for non grayscale image is yet to be implemented");
                        

                    LLUtils::Buffer image(rowPitch / samplesPerPixel * height);
                    size_t destIndex = 0;

                    const size_t samplesPerSingleCHannel = rowPitch * height / bytesPerSample / samplesPerPixel;
                    for (size_t i = 0; i < samplesPerSingleCHannel; i++)
                    {
                        const size_t sourceIndex = i * samplesPerPixel * bytesPerSample;
                        memcpy(reinterpret_cast<uint8_t*>(image.data()) + destIndex, reinterpret_cast<uint8_t*>(out_properties.fData.data()) + sourceIndex, bytesPerSample);
                        destIndex += bytesPerSample;
                    }
                    out_properties.fData = std::move(image);
                    out_properties.fProperties.RowPitchInBytes /= samplesPerPixel;
                }

            }
            return success;
        }
    };
}
