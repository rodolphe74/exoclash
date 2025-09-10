#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#define THOM_LEVELS 16

typedef struct {
    uint8_t pc[THOM_LEVELS];       // niveaux en PC-space (0–255)
    float   linear[THOM_LEVELS];   // niveaux linéarisés (0.0–1.0)
    uint8_t pc2to[256];            // map PC → index Thomson [0–15]
    uint8_t linear2to[256];        // map linéaire → index Thomson [0–15]
} ThomsonLevels;

typedef struct {
    float r, g, b;
} Color;

ThomsonLevels thomson_levels;

void init_thomson_levels(void) {
    // Niveaux PC-space (calculés avec (i/15)^(1/3) * 255 + 0.5)
    const uint8_t pc_values[THOM_LEVELS] = {
        0, 103, 130, 149, 164, 177, 188, 198,
        207, 215, 223, 230, 237, 243, 249, 255
    };

    for (int i = 0; i < THOM_LEVELS; i++) {
        thomson_levels.pc[i] = pc_values[i];
        thomson_levels.linear[i] = pc_values[i] / 255.0f;
    }

    // Table PC → Thomson index [0–15]
    for (int i = 0; i < 256; i++) {
        int best = 0;
        int min_dist = abs(i - thomson_levels.pc[0]);
        for (int j = 1; j < THOM_LEVELS; j++) {
            int d = abs(i - thomson_levels.pc[j]);
            if (d < min_dist) {
                min_dist = d;
                best = j;
            }
        }
        thomson_levels.pc2to[i] = best; // index 0–15
    }

    // Table linéaire → Thomson index [0–15]
    for (int i = 0; i < 256; i++) {
        float v = i / 255.0f;
        int best = 0;
        float min_dist = fabsf(v - thomson_levels.linear[0]);
        for (int j = 1; j < THOM_LEVELS; j++) {
            float d = fabsf(v - thomson_levels.linear[j]);
            if (d < min_dist) {
                min_dist = d;
                best = j;
            }
        }
        thomson_levels.linear2to[i] = best; // index 0–15
    }
}
