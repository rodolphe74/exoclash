#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "bayer.h"
#include "color.h"

#define PALETTE_SIZE 16
#define COLOR_ONE 255.0f

#define MAX_HISTO   8
#define MAX_COUPLES 16
#define MAX_CACHE   10000

typedef struct { int c1, c2; } Couple;

// Histogramme : h[1..16] = nombre d’occurrences
// Couples : tableau dynamique
typedef struct {
    Couple couples[MAX_COUPLES];
    int count;
} CoupleList;

typedef struct {
    char key[32];
    CoupleList list;
} CoupleCacheEntry;

static CoupleCacheEntry couple_cache[MAX_CACHE];
static int couple_cache_count = 0;


// Palette Thomson codée en 0xBGR (4 bits par canal)
//const uint16_t thomson_palette[16] = {
//    0x000, //  0 noir          → R=0x00 G=0x00 B=0x00
//    0x00F, //  1 rouge         → R=0x0F G=0x00 B=0x00
//    0xA00, //  2 vert          → R=0x00 G=0x0A B=0x00
//    0xAFF, //  3 jaune         → R=0x0F G=0x0F B=0x0A
//    0x00A, //  4 bleu          → R=0x00 G=0x00 B=0x0A
//    0xA0A, //  5 magenta       → R=0x0A G=0x00 B=0x0A
//    0xAA0, //  6 cyan          → R=0x00 G=0x0A B=0x0A
//    0xFFF, //  7 blanc         → R=0x0F G=0x0F B=0x0F
//    0xAAA, //  8 gris          → R=0x0A G=0x0A B=0x0A
//    0x885, //  9 vieux rose    → R=0x08 G=0x08 B=0x0F
//    0xF88, // 10 vert clair    → R=0x08 G=0x0F B=0x08
//    0xFF8, // 11 sable         → R=0x08 G=0x0F B=0x0F
//    0x88F, // 12 bleu ciel     → R=0x0F G=0x08 B=0x08
//    0xF8F, // 13 magenta clair → R=0x0F G=0x08 B=0x0F
//    0x8FF, // 14 cyan clair    → R=0x0F G=0x0F B=0x08
//    0x80F  // 15 orange        → R=0x0F G=0x08 B=0x00
//};


//int thomson_palette[16][3] = {
//    // Couleurs sombres 0–7
//    { 0,  0,  0},   // 0 - noir
//    {15,  0,  0},   // 1 - rouge
//    { 0, 10,  0},   // 2 - vert
//    {15, 10,  0},   // 3 - jaune
//    { 0,  0, 15},   // 4 - bleu
//    {15,  0, 15},   // 5 - magenta
//    { 0, 10, 15},   // 6 - cyan
//    {15, 15, 15},   // 7 - blanc
//    
//    // Couleurs claires 8–15 (plus claires que 0–7)
//    { 4,  4,  4},   // 8 - gris clair
//    {10,  4,  4},   // 9 - rouge clair
//    { 4, 10,  4},   //10 - vert clair
//    {10, 10,  4},   //11 - jaune clair
//    { 4,  4, 10},   //12 - bleu clair
//    {10,  4, 10},   //13 - magenta clair
//    { 4, 10, 10},   //14 - cyan clair
//    {10,  8,  0}    //15 - orange clair
//};

int thomson_palette[16][3] = {
    {0,  0,  0},    // 0 noir
    {15, 0,  0},    // 1 rouge
    {0,  15, 0},    // 2 vert
    {15, 15, 0},    // 3 jaune
    {0,  0,  15},   // 4 bleu
    {15, 0,  15},   // 5 magenta
    {0,  15, 15},   // 6 cyan
    {15, 15, 15},   // 7 blanc
    {8,  8,  8},    // 8 gris
    {15, 8,  8},    // 9 vieux rose
    {8,  15, 8},    // 10 vert clair
    {15, 15, 8},    // 11 sable
    {8,  8,  15},   // 12 bleu ciel
    {15, 8,  15},   // 13 magenta clair
    {8,  15, 15},   // 14 cyan clair
    {15, 8,  0}     // 15 orange
};





unsigned char *image_buffer = NULL;
int image_width = 0;
int image_height = 0;

#include <stdint.h>

#define OUT_WIDTH  320
#define OUT_HEIGHT 200

static uint8_t output_buffer[OUT_WIDTH * OUT_HEIGHT * 3]; // RGB 8 bits


int load_image(const char *filename) {
    int w, h, channels;
    unsigned char *data = stbi_load(filename, &w, &h, &channels, 3); // force RGB

    if (!data) {
        fprintf(stderr, "Erreur chargement image : %s\n", filename);
        return 0;
    }

    size_t size = w * h * 3;
    image_buffer = (unsigned char *)malloc(size);
    if (!image_buffer) {
        fprintf(stderr, "Erreur allocation mémoire (%zu octets)\n", size);
        stbi_image_free(data);
        return 0;
    }

    memcpy(image_buffer, data, size);
    image_width = w;
    image_height = h;

    stbi_image_free(data);
    return 1;
}

int CENTERED = 1;
void thom2screen(int x, int y, int *i, int *j) {
    if ((float)image_width / image_height < 1.6f) {
        float o = CENTERED ? (image_width - image_height * 1.6f) / 2.0f : 0.0f;
        *i = (int)(x * image_height / 200.0f + o);
        *j = (int)(y * image_height / 200.0f);
    } else {
        float o = CENTERED ? (image_height - image_width / 1.6f) / 2.0f : 0.0f;
        *i = (int)(x * image_width / 320.0f);
        *j = (int)(y * image_width / 320.0f + o);
    }
}


Color getLinearPixel(int x, int y) {
    int x1, y1, x2, y2;
    thom2screen(x, y,     &x1, &y1);
    thom2screen(x + 1, y + 1, &x2, &y2);

    if (x2 == x1) x2 = x1 + 1;
    if (y2 == y1) y2 = y1 + 1;

    Color sum = {0.0f, 0.0f, 0.0f};
    int count = 0;

    for (int i = x1; i < x2; i++) {
        for (int j = y1; j < y2; j++) {
            if (i < 0 || i >= image_width || j < 0 || j >= image_height) continue;

            int idx = (j * image_width + i) * 3;
            sum.r += image_buffer[idx + 0] / COLOR_ONE;
            sum.g += image_buffer[idx + 1] / COLOR_ONE;
            sum.b += image_buffer[idx + 2] / COLOR_ONE;
            count++;
        }
    }

    if (count > 0) {
        sum.r /= count;
        sum.g /= count;
        sum.b /= count;
    }

    return sum;
}

void pset(int x, int y, uint8_t color_index) {
    if (x < 0 || x >= OUT_WIDTH || y < 0 || y >= OUT_HEIGHT)
        return;

    // Récupère la couleur MO5 (indexée de 0 à 15)
//    uint16_t bgr = thomson_palette[color_index % 16];

//    uint8_t r4 =  bgr        & 0xF;
//    uint8_t g4 = (bgr >> 4)  & 0xF;
//    uint8_t b4 = (bgr >> 8)  & 0xF;

    uint8_t r4 = thomson_palette[color_index % 16][0];
    uint8_t g4 = thomson_palette[color_index % 16][1];
    uint8_t b4 = thomson_palette[color_index % 16][2];
    
    // Convertit en 8 bits linéaire
    uint8_t R = (uint8_t)(thomson_levels.linear[r4] * 255.0f + 0.5f);
    uint8_t G = (uint8_t)(thomson_levels.linear[g4] * 255.0f + 0.5f);
    uint8_t B = (uint8_t)(thomson_levels.linear[b4] * 255.0f + 0.5f);

    int idx = (y * OUT_WIDTH + x) * 3;
    output_buffer[idx + 0] = R;
    output_buffer[idx + 1] = G;
    output_buffer[idx + 2] = B;
}



Color get_linear_palette(int index) {
    if (index < 0 || index >= 16) {
        return (Color){0.0f, 0.0f, 0.0f}; // sécurité
    }
    
//    uint16_t bgr = thomson_palette[index];
    
//    int r4 =  bgr        & 0xF;
//    int g4 = (bgr >> 4)  & 0xF;
//    int b4 = (bgr >> 8)  & 0xF;
    
    uint8_t r4 = thomson_palette[index][0];
    uint8_t g4 = thomson_palette[index][1];
    uint8_t b4 = thomson_palette[index][2];
    
    
    
    Color c = {
        thomson_levels.linear[r4],
        thomson_levels.linear[g4],
        thomson_levels.linear[b4]
    };
    
    return c;
}


float distance_between(int c1, int c2) {
    if (c1 < 0 || c1 >= 16 || c2 < 0 || c2 >= 16)
        return 0.0f;
    
    Color a = get_linear_palette(c1);
    Color b = get_linear_palette(c2);
    
    Color x = {
        a.r - b.r,
        a.g - b.g,
        a.b - b.b
    };
    
    const float c = 1.8f;
    const float wR = 8.0f, wG = 11.0f, wB = 8.0f;
    
    float d =
    powf(fabsf(x.r) * wR, c) +
    powf(fabsf(x.g) * wG, c) +
    powf(fabsf(x.b) * wB, c);
    
    return d;
}


CoupleList best_couple_get(const int h[PALETTE_SIZE]) {
    // Génère une clé unique à partir des 8 premières entrées
    char key[32];
    int k1 = (((h[0]*8 + h[1])*8 + h[2])*8 + h[3]);
    int k2 = (((h[4]*8 + h[5])*8 + h[6])*8 + h[7]);
    snprintf(key, sizeof(key), "%d,%d", k1, k2);

    // Recherche dans le cache
    for (int i = 0; i < couple_cache_count; i++) {
        if (strcmp(couple_cache[i].key, key) == 0) {
            return couple_cache[i].list;
        }
    }

    // Calcul des meilleurs couples
    float dm = 1e6f;
    CoupleList best = { .count = 0 };

    for (int i = 0; i < 15; i++) {
        for (int j = i + 1; j < 16; j++) {
            float d = 0.0f;
            for (int p = 0; p < PALETTE_SIZE; p++) {
                int n = h[p];
                if (n == 0) continue;
                float d1 = distance_between(p, i);
                float d2 = distance_between(p, j);
                d += n * fminf(d1, d2);
                if (d > dm) break;
            }
            if (d < dm) {
                dm = d;
                best.count = 0;
            }
            if (d <= dm && best.count < MAX_COUPLES) {
                best.couples[best.count++] = (Couple){i, j};
            }
        }
    }

    // Gestion mémoire : reset si trop d’entrées
    if (couple_cache_count >= MAX_CACHE) {
        couple_cache_count = 0;
    }

    // Ajout au cache
    strncpy(couple_cache[couple_cache_count].key, key, sizeof(key));
    couple_cache[couple_cache_count].list = best;
    couple_cache_count++;

    printf("coupleCacheCount: %d\n", couple_cache_count);

    return best;
}

void draw_palette() {
    for (int i = 0; i < 16; i++) {
        int x0 = i * 17; // 16 pixels + 1 pixel de marge
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                pset(x0 + x, y, i);
            }
        }
    }
}

int main()
{
    
//    BayerMatrix mat = bayer_make(8);
//    
//    print_bayer_matrix(&mat);
//    printf("\n");
//    print_bayer_norm(&mat);
//    
//    free_bayer_matrix(&mat);
//    
    init_thomson_levels();
//
//    uint8_t pc_level = 179;
//    uint8_t thom_level = thomson_levels.pc2to[pc_level]; // ex: 6
//    printf("tl:%d\n", thom_level);
//    
//    float lin = 0.72f;
//    uint8_t thom_lin = thomson_levels.linear2to[(int)(lin * 255.0f)];
//    printf("tl:%d\n", thom_lin);
//    
//    Color c = get_linear_palette(1); // index Lua = 5 → palette[4]
//    printf("R=%.3f G=%.3f B=%.3f\n", c.r, c.g, c.b);
//    
//    float d = distance_between(0, 1); // magenta foncé vs magenta clair
//    printf("Distance perceptuelle = %.3f\n", d);
//    
//    
//    int histo[16] = {0};
//    histo[3] = 6;
//    histo[0] = 2;
//
//    CoupleList result = best_couple_get(histo);
//    for (int i = 0; i < result.count; i++) {
//        printf("Couple: (%d, %d)\n", result.couples[i].c1, result.couples[i].c2);
//    }
//
//    int histo2[16] = {0};
//    histo2[3] = 4;
//    histo2[4] = 3;
//
//    CoupleList result2 = best_couple_get(histo2);
//    for (int i = 0; i < result2.count; i++) {
//        printf("Couple: (%d, %d)\n", result2.couples[i].c1, result2.couples[i].c2);
//    }

    
    load_image("/Users/rodoc/develop/exoclash/samples/original.png");
    printf("Image : %dx%d\n", image_width, image_height);
    
//    Color color = getLinearPixel(100, 50);
//    printf("R=%.3f G=%.3f B=%.3f\n", color.r, color.g, color.b);

    
    BayerMatrix dither = bayer_make(4);
    print_bayer_norm(&dither);
    
    free(image_buffer);
    
    draw_palette();
    
    stbi_write_png("output.png", OUT_WIDTH, OUT_HEIGHT, 3, output_buffer, OUT_WIDTH * 3);


    
    return 0;
}
