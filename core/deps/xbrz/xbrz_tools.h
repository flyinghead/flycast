// ****************************************************************************
// * This file is part of the HqMAME project. It is distributed under         *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0         *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved          *
// *                                                                          *
// * Additionally and as a special exception, the author gives permission     *
// * to link the code of this program with the MAME library (or with modified *
// * versions of MAME that use the same license as MAME), and distribute      *
// * linked combinations including the two. You must obey the GNU General     *
// * Public License in all respects for all of the code used other than MAME. *
// * If you modify this file, you may extend this exception to your version   *
// * of the file, but you are not obligated to do so. If you do not wish to   *
// * do so, delete this exception statement from your version.                *
// ****************************************************************************

#ifndef XBRZ_TOOLS_H_825480175091875
#define XBRZ_TOOLS_H_825480175091875

#include <cassert>
#include <algorithm>
#include <type_traits>


namespace xbrz
{
template <uint32_t N> inline
unsigned char getByte(uint32_t val) { return static_cast<unsigned char>((val >> (8 * N)) & 0xff); }
// Note that we use RGBA, not ARGB
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ && defined(GLES)
inline unsigned char getAlpha(uint32_t pix) { return getByte<3>(pix); }
inline unsigned char getRed  (uint32_t pix) { return getByte<0>(pix); }
inline unsigned char getGreen(uint32_t pix) { return getByte<1>(pix); }
inline unsigned char getBlue (uint32_t pix) { return getByte<2>(pix); }

inline uint32_t makePixel(unsigned char a, unsigned char r, unsigned char g, unsigned char b) { return (a << 24)  | (b << 16) | (g << 8) | r; }
inline uint32_t makePixel(                 unsigned char r, unsigned char g, unsigned char b) { return 0xFF000000 | (b << 16) | (g << 8) | r; }
#else
inline unsigned char getAlpha(uint32_t pix) { return getByte<0>(pix); }
inline unsigned char getRed  (uint32_t pix) { return getByte<3>(pix); }
inline unsigned char getGreen(uint32_t pix) { return getByte<2>(pix); }
inline unsigned char getBlue (uint32_t pix) { return getByte<1>(pix); }

inline uint32_t makePixel(unsigned char a, unsigned char r, unsigned char g, unsigned char b) { return (r << 24) | (g << 16) | (b << 8) | a; }
inline uint32_t makePixel(                 unsigned char r, unsigned char g, unsigned char b) { return (r << 24) | (g << 16) | (b << 8) | 0xFF; }
#endif

inline uint32_t rgb555to888(uint16_t pix) { return ((pix & 0x7C00) << 9) | ((pix & 0x03E0) << 6) | ((pix & 0x001F) << 3); }
inline uint32_t rgb565to888(uint16_t pix) { return ((pix & 0xF800) << 8) | ((pix & 0x07E0) << 5) | ((pix & 0x001F) << 3); }

inline uint16_t rgb888to555(uint32_t pix) { return static_cast<uint16_t>(((pix & 0xF80000) >> 9) | ((pix & 0x00F800) >> 6) | ((pix & 0x0000F8) >> 3)); }
inline uint16_t rgb888to565(uint32_t pix) { return static_cast<uint16_t>(((pix & 0xF80000) >> 8) | ((pix & 0x00FC00) >> 5) | ((pix & 0x0000F8) >> 3)); }


template <class Pix> inline
Pix* byteAdvance(Pix* ptr, int bytes)
{
    using PixNonConst = typename std::remove_cv<Pix>::type;
    using PixByte     = typename std::conditional<std::is_same<Pix, PixNonConst>::value, char, const char>::type;

    static_assert(std::is_integral<PixNonConst>::value, "Pix* is expected to be cast-able to char*");

    return reinterpret_cast<Pix*>(reinterpret_cast<PixByte*>(ptr) + bytes);
}


//fill block  with the given color
template <class Pix> inline
void fillBlock(Pix* trg, int pitch, Pix col, int blockWidth, int blockHeight)
{
    //for (int y = 0; y < blockHeight; ++y, trg = byteAdvance(trg, pitch))
    //    std::fill(trg, trg + blockWidth, col);

    for (int y = 0; y < blockHeight; ++y, trg = byteAdvance(trg, pitch))
        for (int x = 0; x < blockWidth; ++x)
            trg[x] = col;
}


enum class SliceType
{
    SOURCE,
    TARGET,
};

template <class PixSrc, class PixTrg, class PixConverter>
void nearestNeighborScale(const PixSrc* src, int srcWidth, int srcHeight, int srcPitch,
                          /**/  PixTrg* trg, int trgWidth, int trgHeight, int trgPitch,
                          SliceType st, int yFirst, int yLast, PixConverter pixCvrt /*convert PixSrc to PixTrg*/)
{
    static_assert(std::is_integral<PixSrc>::value, "PixSrc* is expected to be cast-able to char*");
    static_assert(std::is_integral<PixTrg>::value, "PixTrg* is expected to be cast-able to char*");

    static_assert(std::is_same<decltype(pixCvrt(PixSrc())), PixTrg>::value, "PixConverter returning wrong pixel format");

    if (srcPitch < srcWidth * static_cast<int>(sizeof(PixSrc))  ||
        trgPitch < trgWidth * static_cast<int>(sizeof(PixTrg)))
    {
        assert(false);
        return;
    }

    switch (st)
    {
        case SliceType::SOURCE:
            //nearest-neighbor (going over source image - fast for upscaling, since source is read only once
            yFirst = std::max(yFirst, 0);
            yLast  = std::min(yLast, srcHeight);
            if (yFirst >= yLast || trgWidth <= 0 || trgHeight <= 0) return;

            for (int y = yFirst; y < yLast; ++y)
            {
                //mathematically: ySrc = floor(srcHeight * yTrg / trgHeight)
                // => search for integers in: [ySrc, ySrc + 1) * trgHeight / srcHeight

                //keep within for loop to support MT input slices!
                const int yTrg_first = ( y      * trgHeight + srcHeight - 1) / srcHeight; //=ceil(y * trgHeight / srcHeight)
                const int yTrg_last  = ((y + 1) * trgHeight + srcHeight - 1) / srcHeight; //=ceil(((y + 1) * trgHeight) / srcHeight)
                const int blockHeight = yTrg_last - yTrg_first;

                if (blockHeight > 0)
                {
                    const PixSrc* srcLine = byteAdvance(src, y * srcPitch);
                    /**/  PixTrg* trgLine = byteAdvance(trg, yTrg_first * trgPitch);
                    int xTrg_first = 0;

                    for (int x = 0; x < srcWidth; ++x)
                    {
                        const int xTrg_last = ((x + 1) * trgWidth + srcWidth - 1) / srcWidth;
                        const int blockWidth = xTrg_last - xTrg_first;
                        if (blockWidth > 0)
                        {
                            xTrg_first = xTrg_last;

                            const auto trgPix = pixCvrt(srcLine[x]);
                            fillBlock(trgLine, trgPitch, trgPix, blockWidth, blockHeight);
                            trgLine += blockWidth;
                        }
                    }
                }
            }
            break;

        case SliceType::TARGET:
            //nearest-neighbor (going over target image - slow for upscaling, since source is read multiple times missing out on cache! Fast for similar image sizes!)
            yFirst = std::max(yFirst, 0);
            yLast  = std::min(yLast, trgHeight);
            if (yFirst >= yLast || srcHeight <= 0 || srcWidth <= 0) return;

            for (int y = yFirst; y < yLast; ++y)
            {
                PixTrg* trgLine = byteAdvance(trg, y * trgPitch);
                const int ySrc = srcHeight * y / trgHeight;
                const PixSrc* srcLine = byteAdvance(src, ySrc * srcPitch);
                for (int x = 0; x < trgWidth; ++x)
                {
                    const int xSrc = srcWidth * x / trgWidth;
                    trgLine[x] = pixCvrt(srcLine[xSrc]);
                }
            }
            break;
    }
}
}

#endif //XBRZ_TOOLS_H_825480175091875
