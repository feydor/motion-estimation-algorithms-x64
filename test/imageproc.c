/* test/imageproc.c */
#include <unity.h>
#include <stdint.h> /* for int32_t */
#include <stdlib.h> /* for malloc */

#include "../include/imageproc.h"
#include "../include/imageio.h"

/* test prototypes */
void test_closestfrompal(void);
void test_pixelat(void);
void test_bayer_sqrmat(void);

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_closestfrompal);
    RUN_TEST(test_pixelat);
    RUN_TEST(test_bayer_sqrmat);
}

void setUp(void)
{
    // set stuff up here
}

void tearDown(void)
{
    // clean stuff up here
}

void test_closestfrompal(void)
{
    int32_t pal[] = {
        0x000000, 0x008000, 0x00FF00,
        0x0000FF, 0x0080FF, 0x00FFFF,
        0x800000, 0x808000, 0x80FF00,
        0x8000FF, 0x8080FF, 0x80FFFF,
        0xFF0000, 0xFF8000, 0xFFFF00,
        0xFF00FF, 0xFF80FF, 0xFFFFFF
    };
    int32_t closest = 0xFFFFFF;

    // all black returns all black
    closest = closestfrompal(0x000000, pal, sizeof(pal)/sizeof(pal[0]));
    TEST_ASSERT_EQUAL(0x000000, closest);
    
    // all white returns all white
    closest = closestfrompal(0xFFFFFF, pal, sizeof(pal)/sizeof(pal[0]));
    TEST_ASSERT_EQUAL(0xFFFFFF, closest);
    
    // black off by 1 returns black
    closest = closestfrompal(0x000001, pal, sizeof(pal)/sizeof(pal[0]));
    TEST_ASSERT_EQUAL(0x000000, closest);

    // inbetween two pallette colors returns the first one
    closest = closestfrompal(0x004000, pal, sizeof(pal)/sizeof(pal[0]));     
    TEST_ASSERT_EQUAL(0x000000, closest); 
    closest = closestfrompal(0x004F00, pal, sizeof(pal)/sizeof(pal[0]));     
    TEST_ASSERT_EQUAL(0x008000, closest); 
}

void test_pixelat(void)
{
    /**
     * generate a 4x4 byte image buffer (ie a 1x4 pixel buffer)
     */
    struct image32_t img = {0};
    img.w = img.h = 4; // width is guaranteed to be a multiple of 4
    img.buf = calloc(img.w * img.h, 1);
    
    /* normal iteration through every byte */
    TEST_ASSERT_EQUAL(pixel_at(&img, 0, 0), 0);
    TEST_ASSERT_EQUAL(pixel_at(&img, 1, 0), 0);
    TEST_ASSERT_EQUAL(pixel_at(&img, 2, 0), 0);
    TEST_ASSERT_EQUAL(pixel_at(&img, 3, 0), 0);

    TEST_ASSERT_EQUAL(pixel_at(&img, 0, 1), 1);
    TEST_ASSERT_EQUAL(pixel_at(&img, 1, 1), 1);
    TEST_ASSERT_EQUAL(pixel_at(&img, 2, 1), 1);
    TEST_ASSERT_EQUAL(pixel_at(&img, 3, 1), 1);
    
    TEST_ASSERT_EQUAL(pixel_at(&img, 0, 2), 2);
    TEST_ASSERT_EQUAL(pixel_at(&img, 1, 2), 2);
    TEST_ASSERT_EQUAL(pixel_at(&img, 2, 2), 2);
    TEST_ASSERT_EQUAL(pixel_at(&img, 3, 2), 2);
    
    TEST_ASSERT_EQUAL(pixel_at(&img, 0, 3), 3);
    TEST_ASSERT_EQUAL(pixel_at(&img, 1, 3), 3);
    TEST_ASSERT_EQUAL(pixel_at(&img, 2, 3), 3);
    TEST_ASSERT_EQUAL(pixel_at(&img, 3, 3), 3);

    /* error on bound overflow */
    TEST_ASSERT_EQUAL(pixel_at(&img, 4, 0), -1);
    TEST_ASSERT_EQUAL(pixel_at(&img, 0, 4), -1);
    TEST_ASSERT_EQUAL(pixel_at(&img, 4, 4), -1);
    
    /* foreach-type iteration through each pixel */
    int res[4] = {0};
    int count = 0;
    for (size_t y = 0; y < img.h; ++y)
        for (size_t x = 0; x < img.w; x += 4) {
            res[count++] = pixel_at(&img, x, y);
        }

    TEST_ASSERT_EQUAL(res[0], 0);
    TEST_ASSERT_EQUAL(res[1], 1);
    TEST_ASSERT_EQUAL(res[2], 2);
    TEST_ASSERT_EQUAL(res[3], 3);

    free(img.buf);
}

void test_bayer_sqrmat(void)
{
    /* generate an 8x8 Bayer matrix */
    size_t dim = 8;
    int32_t mat[dim * dim];
    bayer_sqrmat(mat, dim);

    int32_t ref[] = {
        0, 32, 8, 40, 2, 34, 10, 42,
        48, 16, 56, 24, 50, 18, 58, 26,
        12, 44, 4, 36, 14, 46, 6, 38, 
        60, 28, 52, 20, 62, 30, 54, 22, 
        3, 35, 11, 43, 1, 33, 9, 41, 
        51, 19, 59, 27, 49, 17, 57, 25, 
        15, 47, 7, 39, 13, 45, 5, 37, 
        63, 31, 55, 23, 61, 29, 53, 21
    };

    TEST_ASSERT_EQUAL_INT32_ARRAY(ref, mat, sizeof(mat) / sizeof(mat[0]));
}

