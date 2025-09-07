#include <exoquant.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_STATIC
#include <stb_image_resize2.h>

#define HIGH_QUALITY 1

typedef struct {
    int col0, col1;
    unsigned char mask; // 8 pixels => 8 bits
    int error;
} Mo5Block;


char resultingFilename[128] = {0};
char tempStr[128] = {0};
int numColors = 16;


// Palette Thomson MO5 - ordre officiel (RGBA 8 bits)
unsigned char mo5_palette[16 * 4] = {
    0x00, 0x00, 0x00, 0xFF, // 0  noir
    0xFF, 0x00, 0x00, 0xFF, // 1  rouge
    0x00, 0xAA, 0x00, 0xFF, // 2  vert
    0xFF, 0xFF, 0x00, 0xFF, // 3  jaune
    0x00, 0x00, 0xAA, 0xFF, // 4  bleu
    0xAA, 0x00, 0xAA, 0xFF, // 5  magenta
    0x00, 0xAA, 0xAA, 0xFF, // 6  cyan
    0xFF, 0xFF, 0xFF, 0xFF, // 7  blanc
    0xAA, 0xAA, 0xAA, 0xFF, // 8  gris
    0xFF, 0x88, 0x88, 0xFF, // 9  vieux rose
    0x88, 0xFF, 0x88, 0xFF, // 10 vert clair
    0xFF, 0xFF, 0x88, 0xFF, // 11 sable
    0x88, 0x88, 0xFF, 0xFF, // 12 bleu ciel
    0xFF, 0x88, 0xFF, 0xFF, // 13 magenta clair
    0x88, 0xFF, 0xFF, 0xFF, // 14 cyan clair
    0xFF, 0x88, 0x00, 0xFF  // 15 orange
};

static inline int color_error(const unsigned char *pix, const unsigned char *pal) {
    int dr = (int)pix[0] - (int)pal[0];
    int dg = (int)pix[1] - (int)pal[1];
    int db = (int)pix[2] - (int)pal[2];
    return dr*dr + dg*dg + db*db;
}


Mo5Block analyze_block(const unsigned char *rgba, int stride) {
    Mo5Block best = {0, 0, 0, INT_MAX};

    for (int c0 = 0; c0 < 16; c0++) {
        for (int c1 = c0+1; c1 < 16; c1++) {
            int total_err = 0;
            unsigned char mask = 0;

            for (int i = 0; i < 8; i++) {
                const unsigned char *pix = rgba + i*stride;
                const unsigned char *p0 = &mo5_palette[c0*4];
                const unsigned char *p1 = &mo5_palette[c1*4];

                int e0 = color_error(pix, p0);
                int e1 = color_error(pix, p1);

                if (e1 < e0) {
                    total_err += e1;
                    mask |= (1 << i);
                } else {
                    total_err += e0;
                }
            }

            if (total_err < best.error) {
                best.col0 = c0;
                best.col1 = c1;
                best.mask = mask;
                best.error = total_err;
            }
        }
    }
    return best;
}


void convert_image(const unsigned char *rgba, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 8) {
            Mo5Block blk = analyze_block(&rgba[(y*width + x)*4], 4);
            printf("Bloc (%d,%d): col0=%d col1=%d mask=%02X err=%d\n",
                   x, y, blk.col0, blk.col1, blk.mask, blk.error);
        }
    }
}


unsigned char *shrink_if_necessary(const unsigned char *inputImage, const int ix, const int iy, unsigned char *resizedImage, int *ox, int *oy)
{
    float ratioX = 0, ratioY = 0, ratio;
    int doResize = 0;
    
    ratioX = ix / 320.0;
    printf("ratio x -> %f\n", ratioX);
    doResize = 1;
    
    if (iy > 200) {
        ratioY = iy / 200.0;
        printf("ratio y -> %f\n", ratioY);
        doResize = 1;
    }
    
    if (doResize) {
        ratio = fmax(ratioX, ratioY);
        printf("ratio -> %f\n", ratio);
        
        int xx, yy;
        xx = ix / ratio;
        yy = iy / ratio;
        
        printf("new dimensions -> %d*%d\n", xx, yy);
        
        resizedImage = malloc(xx * yy * 4);
        stbir_resize_uint8_linear(inputImage, ix, iy, 4 * ix, resizedImage, xx, yy, xx * 4, 4);
//        stbi_write_png("rsz.png", xx, yy, 4, resizedImage, 4 * xx);
        *ox = xx;
        *oy = yy;
        return resizedImage;
    }
    return NULL;
}

unsigned char *put_into_canvas(const unsigned char *inputData, int ix, int iy, unsigned char *outputData, int *ox, int *oy)
{
    int targetw = 320;
    int targeth = 200;
    outputData = malloc(targetw * targeth * 4);
    memset(outputData, 0, targetw * targeth * 4);
    int k = 0, l = 0;
    for (int j = 0; j < iy; j++) {
        for (int i = 0; i < ix; i++) {
            if (j < targeth && i < targetw) {
                outputData[(k * targetw + l) * 4] = inputData[(j * ix + i) * 4];
                outputData[(k * targetw + l) * 4 + 1] = inputData[(j * ix + i) * 4 + 1];
                outputData[(k * targetw + l) * 4 + 2] = inputData[(j * ix + i) * 4 + 2];
                outputData[(k * targetw + l) * 4 + 3] = inputData[(j * ix + i) * 4 + 3];
            }
            l++;
        }
        l = 0;
        k++;
    }
    *ox = targetw;
    *oy = targeth;
    return outputData;
}


int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("argc:%d\n", argc);
        printf("arguments must be exactly [filename] [graphics mode]\n");
        return 0;
    }

    char *fname = argv[1];      // filename

    int x, y, c;

    unsigned char *data = stbi_load(fname, &x, &y, &c, 4);

    printf("Image size:%d*%d*%d\n", x, y, c);
    unsigned char *indexedPaletteData = malloc(x * y);

    // resize ?
    int xx, yy;
    unsigned char *dataResized = NULL;
    dataResized = shrink_if_necessary(data, x, y, dataResized, &xx, &yy);
    if (!dataResized) {
        xx = x;
        yy = y;
        dataResized = malloc(x * y * 4);
    }

    // find optimal palette
    exq_data *pExq;
    pExq = exq_init();
    exq_no_transparency(pExq);
    exq_feed(pExq, dataResized, xx * yy);
//    exq_quantize(pExq, numColors);
//    exq_quantize_hq(pExq, numColors);
//    exq_quantize_ex(pExq, numColors, HIGH_QUALITY);
//    exq_get_palette(pExq, pPalette, numColors);

    exq_set_palette(pExq, mo5_palette, 16);
    
    // dithering
    exq_map_image(pExq, xx * yy, dataResized, indexedPaletteData);
    exq_map_image_ordered(pExq, xx, yy, dataResized, indexedPaletteData);
    // exq_map_image_random(pExq, xx * yy, dataResized, indexedPaletteData);

    for (int i = 0, j = 0; i < xx * yy * 4; i += 4, j++) {
        dataResized[i] =  *(mo5_palette + indexedPaletteData[j] * 4);
        dataResized[i + 1] =  *(mo5_palette + indexedPaletteData[j] * 4 + 1);
        dataResized[i + 2] =  *(mo5_palette + indexedPaletteData[j] * 4 + 2);
        dataResized[i + 3] =  *(mo5_palette + indexedPaletteData[j] * 4 + 3);
    }

    // sometimes resized image doesn't fit on mo5 screen exactly
    unsigned char *canvasData = NULL;
    int ox, oy;
    canvasData = put_into_canvas(dataResized, xx, yy, canvasData, &ox, &oy);
    xx = ox;
    yy = oy;
    free(dataResized);
    dataResized = canvasData;
    printf("canvas size -> %d*%d\n", xx, yy);

    exq_free(pExq);
    stbi_write_png("dither.png", xx, yy, 4, dataResized, 4 * xx);
    
    
    
    
    // color clash
    unsigned char *out = malloc(xx * yy * 3);

    for (int y = 0; y < yy; y++) {
        for (int x = 0; x < xx; x += 8) {
            Mo5Block blk = analyze_block(&dataResized[(y*xx + x)*4], 4);

            // Recrée le bloc avec la palette MO5
            for (int i = 0; i < 8; i++) {
                int use_col1 = (blk.mask >> i) & 1;
                int col = use_col1 ? blk.col1 : blk.col0;

                int idx = (y*xx + (x+i))*3;
                out[idx+0] = mo5_palette[col*4 + 0];
                out[idx+1] = mo5_palette[col*4 + 1];
                out[idx+2] = mo5_palette[col*4 + 2];
            }
        }
    }

    stbi_write_png("mo5_dither.png", xx, yy, 3, out, xx*3);
    
    // cleanings
    stbi_image_free(data);
    free(indexedPaletteData);
    free(dataResized);
    free(out);
    
    
    return 0;
}
