#include "s3tc.h"

#include <algorithm>

// uint32_t PackRGBA(): Helper method that packs RGBA channels into a single 4 byte pixel.
//
// unsigned char r:     red channel.
// unsigned char g:     green channel.
// unsigned char b:     blue channel.
// unsigned char a:     alpha channel.

std::uint32_t PackRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    return ((r << 24) | (g << 16) | (b << 8) | a);
}

// void DecompressBlockDXT1(): Decompresses one block of a DXT1 texture and stores the resulting pixels at the appropriate offset in 'image'.
//
// uint32_t x:                           x-coordinate of the first pixel in the block.
// uint32_t y:                           y-coordinate of the first pixel in the block.
// uint32_t width:                       width of the texture being decompressed.
// uint32_t height:                      height of the texture being decompressed.
// const unsigned char *blockStorage:   pointer to the block to decompress.
// uint32_t *image:                      pointer to image where the decompressed pixel data should be stored.

void DecompressBlockDXT1(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height,
                         const unsigned char* blockStorage, std::uint32_t* image)
{
    unsigned short color0 = *reinterpret_cast<const unsigned short *>(blockStorage);
    unsigned short color1 = *reinterpret_cast<const unsigned short *>(blockStorage + 2);

    std::uint32_t temp;

    temp = (color0 >> 11) * 255 + 16;
    unsigned char r0 = (unsigned char)((temp / 32 + temp) / 32);
    temp = ((color0 & 0x07E0) >> 5) * 255 + 32;
    unsigned char g0 = (unsigned char)((temp / 64 + temp) / 64);
    temp = (color0 & 0x001F) * 255 + 16;
    unsigned char b0 = (unsigned char)((temp / 32 + temp) / 32);

    temp = (color1 >> 11) * 255 + 16;
    unsigned char r1 = (unsigned char)((temp / 32 + temp) / 32);
    temp = ((color1 & 0x07E0) >> 5) * 255 + 32;
    unsigned char g1 = (unsigned char)((temp / 64 + temp) / 64);
    temp = (color1 & 0x001F) * 255 + 16;
    unsigned char b1 = (unsigned char)((temp / 32 + temp) / 32);

    std::uint32_t code = *reinterpret_cast<const std::uint32_t*>(blockStorage + 4);

    const std::uint32_t blockWidth = std::min(4u, width - x);
    const std::uint32_t blockHeight = std::min(4u, height - y);
    for (std::uint32_t j = 0; j < blockHeight; j++)
    {
        for (std::uint32_t i = 0; i < blockWidth; i++)
        {
            std::uint32_t finalColor = 0;
            unsigned char positionCode = (code >> 2 * (4 * j + i)) & 0x03;

            if (color0 > color1)
            {
                switch (positionCode)
                {
                case 0:
                    finalColor = PackRGBA(r0, g0, b0, 255);
                    break;
                case 1:
                    finalColor = PackRGBA(r1, g1, b1, 255);
                    break;
                case 2:
                    finalColor = PackRGBA((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, 255);
                    break;
                case 3:
                    finalColor = PackRGBA((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, 255);
                    break;
                }
            }
            else
            {
                switch (positionCode)
                {
                case 0:
                    finalColor = PackRGBA(r0, g0, b0, 255);
                    break;
                case 1:
                    finalColor = PackRGBA(r1, g1, b1, 255);
                    break;
                case 2:
                    finalColor = PackRGBA((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2, 255);
                    break;
                case 3:
                    finalColor = PackRGBA(0, 0, 0, 255);
                    break;
                }
            }

            image[(y + j)*width + (x + i)] = finalColor;
        }
    }
}

// void BlockDecompressImageDXT1(): Decompresses all the blocks of a DXT1 compressed texture and stores the resulting pixels in 'image'.
//
// uint32_t width:                       Texture width.
// uint32_t height:                      Texture height.
// const unsigned char *blockStorage:   pointer to compressed DXT1 blocks.
// uint32_t *image:                      pointer to the image where the decompressed pixels will be stored.

void BlockDecompressImageDXT1(std::uint32_t width, std::uint32_t height, const unsigned char* blockStorage,
                              std::uint32_t* image)
{
    std::uint32_t blockCountX = (width + 3) / 4;
    std::uint32_t blockCountY = (height + 3) / 4;

    for (std::uint32_t j = 0; j < blockCountY; j++)
    {
        for (std::uint32_t i = 0; i < blockCountX; i++)
            DecompressBlockDXT1(i * 4, j * 4, width, height, blockStorage + i * 8, image);
        blockStorage += blockCountX * 8;
    }
}

// void DecompressBlockDXT5(): Decompresses one block of a DXT5 texture and stores the resulting pixels at the appropriate offset in 'image'.
//
// uint32_t x:                           x-coordinate of the first pixel in the block.
// uint32_t y:                           y-coordinate of the first pixel in the block.
// uint32_t width:                       width of the texture being decompressed.
// uint32_t height:                      height of the texture being decompressed.
// const unsigned char *blockStorage:   pointer to the block to decompress.
// uint32_t *image:                      pointer to image where the decompressed pixel data should be stored.

void DecompressBlockDXT5(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height,
                         const unsigned char* blockStorage, std::uint32_t* image)
{
    unsigned char alpha0 = *reinterpret_cast<const unsigned char *>(blockStorage);
    unsigned char alpha1 = *reinterpret_cast<const unsigned char *>(blockStorage + 1);

    const unsigned char *bits = blockStorage + 2;
    std::uint32_t alphaCode1 = bits[2] | (bits[3] << 8) | (bits[4] << 16) | (bits[5] << 24);
    unsigned short alphaCode2 = bits[0] | (bits[1] << 8);

    unsigned short color0 = *reinterpret_cast<const unsigned short *>(blockStorage + 8);
    unsigned short color1 = *reinterpret_cast<const unsigned short *>(blockStorage + 10);

    std::uint32_t temp;

    temp = (color0 >> 11) * 255 + 16;
    unsigned char r0 = (unsigned char)((temp / 32 + temp) / 32);
    temp = ((color0 & 0x07E0) >> 5) * 255 + 32;
    unsigned char g0 = (unsigned char)((temp / 64 + temp) / 64);
    temp = (color0 & 0x001F) * 255 + 16;
    unsigned char b0 = (unsigned char)((temp / 32 + temp) / 32);

    temp = (color1 >> 11) * 255 + 16;
    unsigned char r1 = (unsigned char)((temp / 32 + temp) / 32);
    temp = ((color1 & 0x07E0) >> 5) * 255 + 32;
    unsigned char g1 = (unsigned char)((temp / 64 + temp) / 64);
    temp = (color1 & 0x001F) * 255 + 16;
    unsigned char b1 = (unsigned char)((temp / 32 + temp) / 32);

    std::uint32_t code = *reinterpret_cast<const std::uint32_t*>(blockStorage + 12);

    const std::uint32_t blockWidth = std::min(4u, width - x);
    const std::uint32_t blockHeight = std::min(4u, height - y);
    for (std::uint32_t j = 0; j < blockHeight; j++)
    {
        for (std::uint32_t i = 0; i < blockWidth; i++)
        {
            int alphaCodeIndex = 3 * (4 * j + i);
            int alphaCode;

            if (alphaCodeIndex <= 12)
            {
                alphaCode = (alphaCode2 >> alphaCodeIndex) & 0x07;
            }
            else if (alphaCodeIndex == 15)
            {
                alphaCode = (alphaCode2 >> 15) | ((alphaCode1 << 1) & 0x06);
            }
            else // alphaCodeIndex >= 18 && alphaCodeIndex <= 45
            {
                alphaCode = (alphaCode1 >> (alphaCodeIndex - 16)) & 0x07;
            }

            unsigned char finalAlpha;
            if (alphaCode == 0)
            {
                finalAlpha = alpha0;
            }
            else if (alphaCode == 1)
            {
                finalAlpha = alpha1;
            }
            else
            {
                if (alpha0 > alpha1)
                {
                    finalAlpha = ((8 - alphaCode)*alpha0 + (alphaCode - 1)*alpha1) / 7;
                }
                else
                {
                    if (alphaCode == 6)
                        finalAlpha = 0;
                    else if (alphaCode == 7)
                        finalAlpha = 255;
                    else
                        finalAlpha = ((6 - alphaCode)*alpha0 + (alphaCode - 1)*alpha1) / 5;
                }
            }

            unsigned char colorCode = (code >> 2 * (4 * j + i)) & 0x03;

            std::uint32_t finalColor;
            switch (colorCode)
            {
            case 0:
                finalColor = PackRGBA(r0, g0, b0, finalAlpha);
                break;
            case 1:
                finalColor = PackRGBA(r1, g1, b1, finalAlpha);
                break;
            case 2:
                finalColor = PackRGBA((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, finalAlpha);
                break;
            case 3:
                finalColor = PackRGBA((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, finalAlpha);
                break;
            }

            image[(y + j)*width + (x + i)] = finalColor;
        }
    }
}

// void BlockDecompressImageDXT5(): Decompresses all the blocks of a DXT5 compressed texture and stores the resulting pixels in 'image'.
//
// uint32_t width:                       Texture width.
// uint32_t height:                      Texture height.
// const unsigned char *blockStorage:   pointer to compressed DXT5 blocks.
// uint32_t *image:                      pointer to the image where the decompressed pixels will be stored.

void BlockDecompressImageDXT5(std::uint32_t width, std::uint32_t height, const unsigned char* blockStorage,
                              std::uint32_t* image)
{
    std::uint32_t blockCountX = (width + 3) / 4;
    std::uint32_t blockCountY = (height + 3) / 4;

    for (std::uint32_t j = 0; j < blockCountY; j++)
    {
        for (std::uint32_t i = 0; i < blockCountX; i++)
            DecompressBlockDXT5(i * 4, j * 4, width, height, blockStorage + i * 16, image);
        blockStorage += blockCountX * 16;
    }
}
