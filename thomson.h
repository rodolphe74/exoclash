#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>


#define WIDTH 320
#define HEIGHT 200
#define PALETTE_SIZE 16
#define COLOR_COMP 3

typedef struct {
	uint8_t *data;
	size_t size;
	size_t capacity;
} IntVector;


typedef struct {
	uint8_t columns;
	uint8_t lines;
	IntVector rama;
	IntVector ramb;
} MAP_SEG;

void init_vector(IntVector *vec)
{
	vec->size = 0;
	vec->capacity = 4;
	vec->data = (uint8_t *)malloc(vec->capacity * sizeof(uint8_t));
}

void push_back(IntVector *vec, uint8_t value)
{
	if (vec->size >= vec->capacity) {
		vec->capacity *= 2;
		void *tmp = realloc(vec->data, vec->capacity * sizeof(int));
		if (tmp == NULL) {
			vec->capacity /= 2;
			return;
		}
		vec->data = tmp;
	}
	vec->data[vec->size++] = value;
}

void free_vector(IntVector *vec)
{
	free(vec->data);
	vec->data = NULL;
	vec->size = vec->capacity = 0;
}

void clash_fragment_to_palette_indexed_bloc(const unsigned char *fragment, uint8_t *bloc, int blocSize, unsigned char palette[16 * 4])
{
	for (int i = 0; i < blocSize; i++) {
		//Color c = {fragment[i * COLOR_COMP], fragment[i * COLOR_COMP + 1], fragment[i * COLOR_COMP + 2]};
		//int idx = find_palette_index(c.r, c.g, c.b, palette);
        uint8_t r = fragment[i * COLOR_COMP];
        uint8_t g = fragment[i * COLOR_COMP + 1];
        uint8_t b = fragment[i * COLOR_COMP + 2];

        int idx = 0;
        for (int i = 0; i < 16; i++) {
            uint8_t rp = palette[i * 4];
            uint8_t gp = palette[i * 4 + 1];
            uint8_t bp = palette[i * 4 + 2];

            if (rp == r && gp == g && bp == r) {
                idx = i;
                break;
            }
        }

		bloc[8 - 1 - i] = idx;
	}
}

int get_index_color_thomson_to(int back_index, int fore_index)
{
	// Palette thomson TO xyBVRBVR | x = 0 : fd pastel | y = 0 fo pastel
	// N,R,V,J,B,M,C,BL (fonce)
	// x,x,x,x,x,x,x,OR (pastel)

	// couleur > 7 = pastel
	int subst_back = (back_index > 7 ? 8 : 0);
	int subst_fore = (fore_index > 7 ? 8 : 0);
	unsigned char idx = (back_index > 7 ? 0 : 1) << 7 | (fore_index > 7 ? 0 : 1) << 6 | (fore_index - subst_fore) << 3 |
						(back_index - subst_back);

	return idx;
}

int get_index_color_thomson_mo(int back_index, int fore_index)
{
	// Palette thomson MO5/6 xBVRyBVR | x = 1 : fd pastel | y = 1 fo pastel
	// N,R,V,J,B,M,C,BL (fonce)
	// x,x,x,x,x,x,x,OR (pastel)

	// couleur > 7 = pastel
	unsigned char idx =
		(fore_index > 7 ? 1 : 0) << 7 | (fore_index) << 4 | (back_index > 7 ? 1 : 0) << 3 | (back_index);

	return idx;
}

void thomson_encode_bloc(uint8_t bloc[8], uint8_t thomson_bloc[3])
{
	// Conversion du bloc en valeur thomson to/mo
	// en sortie :
	// thomson_bloc[0] = forme
	// thomson_bloc[1] = couleurs format TO
	// thomson_bloc[2] = couleurs format MO
	// En basic, le format de la couleur est spécifié en fonction de la config TO/MO
	// En SNAP-TO, le format de la couleur est toujours TO

	// recherche des couleurs
	int fd = bloc[0];
	int fo = -1;
	int val = 0 /*, coul = 0*/;

	for (int i = 0; i < 8; i++)
		if (bloc[i] != fd) fo = bloc[i];

	// Calcul forme
	for (int i = 7; i >= 0; i--)
		if (bloc[i] == fo) val += pow(2, i);

	// Couleur MO / TO
	thomson_bloc[1] = get_index_color_thomson_to(fd, fo <= 0 ? 0 : fo);
	thomson_bloc[2] = get_index_color_thomson_mo(fd, fo <= 0 ? 0 : fo);

	thomson_bloc[0] = val;
}

void find_back_and_front(uint8_t bloc[8], uint8_t *back, uint8_t *front)
{
	uint8_t b = 0;
	uint8_t f = 0;
	uint8_t bc = 0, fc = 0;
	uint8_t count[255] = {0};
	for (int i = 0; i < 8; i++) {
		count[bloc[i]]++;
	}
	for (int i = 0; i < 255; i++) {
		if (count[i] > 0) {
			b = i;
			bc = count[i];
		}
	}

	for (int i = 0; i < 255; i++) {
		if (count[i] > 0 && i != b) {
			f = i;
			fc = count[i];
		}
	}
	// printf("b=%d  f=%d\n", b, f);
	if (bc > fc) {
		*back = b;
		*front = f;
	} else {
		*back = f;
		*front = b;
	}
}

void create_rams(const uint8_t *output_image_data, unsigned char palette[16 * 4], IntVector *pixels, IntVector *colors)
{
	MAP_SEG map_40;
	uint8_t b, f;
	init_vector(&map_40.rama);
	init_vector(&map_40.ramb);
	unsigned char *clash_fragment = malloc(8 * COLOR_COMP);
	if (!clash_fragment) return;
	uint8_t current_bloc[8];
	for (int y = 0; y < HEIGHT; y++) {
		for (int x = 0; x < WIDTH; x += 8) {
			int length = x + 8 > WIDTH ? WIDTH - x : 8;
			memset(clash_fragment, 0, 8 * COLOR_COMP);
			for (int i = 0; i < length; i++) {
				int output_pixel_idx = (y * WIDTH + x + i) * COLOR_COMP;
				uint8_t r = output_image_data[output_pixel_idx];
				uint8_t g = output_image_data[output_pixel_idx + 1];
				uint8_t b = output_image_data[output_pixel_idx + 2];
				clash_fragment[i * COLOR_COMP] = r;
				clash_fragment[i * COLOR_COMP + 1] = g;
				clash_fragment[i * COLOR_COMP + 2] = b;
			}
			clash_fragment_to_palette_indexed_bloc(clash_fragment, current_bloc, 8, palette);
			uint8_t ret[3];
			thomson_encode_bloc(current_bloc, ret);
			push_back(&map_40.rama, ret[0]);
			push_back(&map_40.ramb, ret[1]);

			// MO5 pixels and colors
			find_back_and_front(current_bloc, &b, &f);
			unsigned char result = 0;
			for (int i = 0; i < 8; i++) {
				if (current_bloc[7 - i] == f) {
					result |= 1 << (7 - i);
				}
			}
			// en sortie les données pixels et forme (utils pour la sauvegarde MO5)
			push_back(colors, 16 * f + b);
			push_back(pixels, result);
		}
	}
	map_40.lines = HEIGHT;
	map_40.columns = WIDTH / 8 + (WIDTH % 8 == 0 ? 0 : 1);

	free_vector(&map_40.rama);
	free_vector(&map_40.ramb);
}
