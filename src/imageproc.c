    /* imageproc.c - functions for image processing */
#include <assert.h> /* for assert */
#include <errno.h> /* for errno */
#include <limits.h> /* for UINT_MAX */ 
#include <math.h> /* for sqrt */
#include <stdint.h> /* for uint16_t */
#include <stdlib.h> /* for malloc, exit, abs */
#include <stdio.h> /* for printf */
#include "../include/imageproc.h"
#include "../include/bmp.h"

extern int errno; /* these functions set errno on errors */

static int apply_threshold(int32_t color, int32_t matval, int32_t offset,
                int32_t *thresholds); 

/**
 * ordered dithering with Bayer matrices
 * eq: c' = closestfrompal(c + r * (M(x%n, y%n) - 0.5))
 * r = 256 / N - 1 (N^3 evenly choosen colors in palette)
 */
int ordered_dithering(struct image32_t *image)
{
    if (!image || !image->buf)
        return -1;

    int32_t pal[] = {
        0x000000, 0x008000, 0x00FF00,
        0x0000FF, 0x0080FF, 0x00FFFF,
        0x800000, 0x808000, 0x80FF00,
        0x8000FF, 0x8080FF, 0x80FFFF,
        0xFF0000, 0xFF8000, 0xFFFF00,
        0xFF00FF, 0xFF80FF, 0xFFFFFF
    };
    size_t npal = sizeof(pal) / sizeof(pal[0]);

    /* using an 8x8 Bayer matrix */
    const size_t dim = 4;
    int32_t mat[] = {
        0, 8, 2, 10,
        12, 4, 14, 6,
        3, 11, 1, 9,
        15, 7, 13, 5
    };
    const size_t nmat = sizeof(mat) / sizeof(mat[0]);
    int32_t thresholds[] = { 256/4, 256/4, 256/4 };

    int32_t offset = (dim * (dim / 2)) - 0.5;
    // bayer_sqrmat(mat, dim);

    /* iterate over 3 packed pixels at a time, unpacking them into 4 */
    int32_t packed[3];
    int32_t factors[4];
    int32_t unpacked[4];
    int32_t processed[4];
    int32_t closest[4];
    int32_t nextclosest[4];
    for (size_t i = 0; i < image->h * image->w / PXLSIZE; i += 3) {
        factors[0] = mat[i % nmat];
        factors[1] = mat[(i+1) % nmat];
        factors[2] = mat[(i+2) % nmat];
        // a hacky way of using the next pixel's factor rightnow
        if (i+3 < image->h * image->w / PXLSIZE)
            factors[3] = mat[(i+3) % nmat];
        else
            factors[3] = mat[(i+2) % nmat];

        packed[0] = image->buf[i];
        packed[1] = image->buf[i+1];
        packed[2] = image->buf[i+2];
        unpackthree(unpacked, packed);

        processed[0] = apply_threshold(unpacked[0], factors[0], offset, thresholds);
        processed[1] = apply_threshold(unpacked[1], factors[1], offset, thresholds);
        processed[2] = apply_threshold(unpacked[2], factors[2], offset, thresholds);
        processed[3] = apply_threshold(unpacked[3], factors[3], offset, thresholds);

        closest[0] = closestfrompal(processed[0], pal, npal);
        closest[1] = closestfrompal(processed[1], pal, npal);
        closest[2] = closestfrompal(processed[2], pal, npal);
        closest[3] = closestfrompal(processed[3], pal, npal);

        packthree(processed, packed);

        image->buf[i] = packed[0];
        image->buf[i+1] = packed[1];
        image->buf[i+2] = packed[2];
    }
    return 1;
}

/**
 * returns a Bayer matrix of X width and Y height
 * Algorithm for assigning slot (x, y):
 * 1. Take two values: the y coordinate and the XOR of the x and y coordinates,
 * 2. Interleave their bits in reverse order,
 * 3. Floating point divide the result by N (x * y)
 */
void
bayer_sqrmat(int32_t *mat, size_t dim)
{
    printf(" X=%lu, Y=%lu:\n", dim, dim);
    for (size_t y = 0; y < dim; ++y) {
        printf("    ");
        for (size_t x = 0; x < dim; ++x) {
            uint32_t xc = x ^ y, yc = y;
            uint32_t res = 0;
            uint32_t mask = 2;
            for(uint32_t bit = 0; bit < 6; --mask) {
                res |= ((yc >> mask) & 1) << bit++;
                res |= ((xc >> mask) & 1) << bit++;
            }
            mat[x + y * dim] = res;
            printf("%4d", res);
        }
        printf(" |");
        if (y == 0)
            printf(" 1/%lu", dim * dim);
        printf("\n");
    }
}

int 
setpixel(struct image32_t *image, int32_t pixel, size_t x, size_t y)
{
    image->buf[pixel_at(image, x, y)] = pixel;
    return 1;
}

/**
 * ignores the two leftmost bytes (MSB + it's neighbor)
 */
int32_t
closestfrompal(int32_t color, int32_t *pal, size_t n)
{
    // use euclidean RGB distances to determine closeness
    int32_t d = INT32_MAX - 1, min = INT32_MAX, res = INT32_MAX;
    int32_t r = 0, g = 0, b = 0;
    for (size_t i = 0; i < n; ++i) {
        b = (pal[i] & 0xFF0000) >> 16;
        g = (pal[i] & 0x00FF00) >> 8;
        r = pal[i] & 0x0000FF;
        d = sqrt( pow(abs(b - ((color & 0xFF0000) >> 16)), 2) +
                  pow(abs(g - ((color & 0x00FF00) >> 8)), 2) +
                  pow(abs(r - (color & 0x0000FF)), 2)
            );

        if (d < min) {
           min = d;
           res = pal[i];
        }
    }
    return res;
}

/**
 * applies the following equation on the passed in color:
 * color = color + factor * threshold, on each BGR component
 * NOTE: threshold size must be 3
 */
static int32_t
apply_threshold(int32_t color, int32_t matval, int32_t offset,
                int32_t *thresholds) 
{
    assert(thresholds && "Is validated by the caller.");

    float r, g, b;
    b = (((color & 0x00FF0000) >> 16) + thresholds[0] * (matval - offset));
    g = (((color & 0x0000FF00) >> 8) + thresholds[1] * (matval - offset));
    r = ((color & 0x000000FF) + thresholds[2] * (matval - offset));

    r = round(r / 255) * 255;
    g = round(g / 255) * 255;
    b = round(b / 255) * 255;
    

    return 0 | ((uint8_t)(b) << 16) | ((uint8_t)g << 8) | (uint8_t)r;
}

/**
 * returns the ith pixel at the given x and y byte coordinate
 * on error, returns -1
 *
 * example (4x4 byte buffer/1x4 pixel buffer using 4 byte pixels):
 * for (size_t y = 0; y < 4; ++y)
 *    for (size_t x = 0; x < 4; ++x)
 *        printf("i = %d\n", pixel_at(image, x, y));
 * prints: 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4 
 */
int
pixel_at(const struct image32_t *image, size_t x, size_t y)
{
    if (x < image->w && y < image->h)
        return x / PXLSIZE + y * image->w / PXLSIZE; // int division drops decimals
    return -1;
}

/*
 * starting from the left hand side, 
 * swaps the ith byte with the jth byte in a
 * example: swap the 0th byte with the 2nd byte
 * swap(a, 0, 2) => 0x00112233 => 0x22110033
 * returns: a byteswapped, -1 otherwise
 * NOTE:i and j must be less than 4
 *      a must be non-negative
 */
int32_t
swapbytes(uint32_t a, unsigned i, unsigned j)
{
    if (i > 3 || j > 3)
        return -1;
    
    assert(a >= 0 && "Must not be a negative number.");

    unsigned temp = ((a >> CHAR_BIT*i) ^ (a >> CHAR_BIT*j)) & 
        ((1U << CHAR_BIT) - 1);
    return a ^ ((temp << CHAR_BIT*i) | (temp << CHAR_BIT*j));
}

/**
 * takes 3 packed pixels, and returns 4 unpacked pixels
 * i0: 0x(bbggrr)(bb,
 * i1: 0xggrr)(bbgg,
 * i2: 0xrr)(bbggrr)
 * the output is the grouped bytes with the leftmost two bytes set to 00
 */
int
unpackthree(int32_t *unpacked, const int32_t *packed)
{
    assert(unpacked && "Is validated by the caller.");
    assert(packed && "Is validated by the caller.");

    uint8_t b1 = 0, b2 = 0, b3 = 0, b4 = 0;
    for (size_t i = 0; i < 3; ++i)
    {
        b1 = (uint8_t)((packed[i] & 0xFF000000) >> 24);
        b2 = (uint8_t)((packed[i] & 0x00FF0000) >> 16);
        b3 = (uint8_t)((packed[i] & 0x0000FF00) >> 8);
        b4 = (uint8_t)(packed[i] & 0x000000FF);

        if (i == 0) {
            unpacked[0] = 0 | (b1 << 16) | (b2 << 8) | b3;
            unpacked[1] = b4 << 16;
        } else if (i == 1) {
            unpacked[1] |= (b1 << 8) | b2;
            unpacked[2] = 0 | (b3 << 16) | (b4 << 8);
        } else if (i == 2) {
            unpacked[2] |= b1;
            unpacked[3] = 0 | (b2 << 16) | (b3 << 8) | b4;
        }
    }
    return 1;
}

int
packthree(const int32_t *unpacked, int32_t *packed)
{
    assert(unpacked && "Is validated by the caller.");
    assert(packed && "Is validated by the caller.");

    uint8_t b0[3], b1[3], b2[3], b3[3];
       
    b0[0] = (uint8_t)((unpacked[0] & 0x00FF0000) >> 16); 
    b0[1] = (uint8_t)((unpacked[0] & 0x0000FF00) >> 8); 
    b0[2] = (uint8_t)(unpacked[0] & 0x000000FF); 
    b1[0] = (uint8_t)((unpacked[1] & 0x00FF0000) >> 16); 
    b1[1] = (uint8_t)((unpacked[1] & 0x0000FF00) >> 8); 
    b1[2] = (uint8_t)(unpacked[1] & 0x000000FF); 
    b2[0] = (uint8_t)((unpacked[2] & 0x00FF0000) >> 16); 
    b2[1] = (uint8_t)((unpacked[2] & 0x0000FF00) >> 8); 
    b2[2] = (uint8_t)(unpacked[2] & 0x000000FF); 
    b3[0] = (uint8_t)((unpacked[3] & 0x00FF0000) >> 16); 
    b3[1] = (uint8_t)((unpacked[3] & 0x0000FF00) >> 8); 
    b3[2] = (uint8_t)(unpacked[3] & 0x000000FF); 

    packed[0] = (b0[0] << 24) | (b0[1] << 16) | (b0[2] << 8) | b1[0];
    packed[1] = (b1[1] << 24) | (b1[2] << 16) | (b2[0] << 8) | b2[1];
    packed[2] = (b2[2] << 24) | (b3[0] << 16) | (b3[1] << 8) | b3[2];
    return 1;
}
