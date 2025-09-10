
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
    int width;
    int height;
    int *data;     // matrice brute
    float *norm;   // matrice normalisée [0..1]
} BayerMatrix;

// 🔁 double une matrice Bayer
BayerMatrix bayer_double(const BayerMatrix *in) {
    int w = in->width, h = in->height;
    int W = w * 2, H = h * 2;

    BayerMatrix out = { W, H, NULL, NULL };
    out.data = (int *)malloc(W * H * sizeof(int));

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int v = in->data[y * w + x];
            out.data[(y*2+0)*W + (x*2+0)] = v*4 - 3;
            out.data[(y*2+0)*W + (x*2+1)] = v*4 - 2;
            out.data[(y*2+1)*W + (x*2+0)] = v*4 - 1;
            out.data[(y*2+1)*W + (x*2+1)] = v*4 - 0;
        }
    }
    return out;
}

// 📏 normalise une matrice Bayer dans [0,1]
void bayer_norm(BayerMatrix *mat) {
    if (!mat || !mat->data) return;

    int W = mat->width, H = mat->height;
    mat->norm = (float *)malloc(W * H * sizeof(float));

    int max = 0;
    for (int i = 0; i < W * H; i++)
        if (mat->data[i] > max) max = mat->data[i];

    for (int i = 0; i < W * H; i++)
        mat->norm[i] = (float)(mat->data[i] - 1) / (float)(max - 1);
}

// 🧱 construit une matrice Bayer normalisée de taille n×n
BayerMatrix bayer_make(int n) {
    BayerMatrix mat = { 1, 1, NULL, NULL };
    mat.data = (int *)malloc(sizeof(int));
    mat.data[0] = 1;

    while (mat.width < n || mat.height < n) {
        BayerMatrix doubled = bayer_double(&mat);
        free(mat.data);
        mat = doubled;
    }

    bayer_norm(&mat);
    return mat;
}

// 🧹 libère une matrice Bayer
void free_bayer_matrix(BayerMatrix *mat) {
    if (!mat) return;
    if (mat->data) free(mat->data);
    if (mat->norm) free(mat->norm);
    mat->data = NULL;
    mat->norm = NULL;
    mat->width = 0;
    mat->height = 0;
}

// 🖨️ affiche la matrice brute
void print_bayer_matrix(const BayerMatrix *mat) {
    if (!mat || !mat->data) {
        printf("(BayerMatrix vide)\n");
        return;
    }

    printf("BayerMatrix [%d x %d] (raw):\n", mat->width, mat->height);
    for (int y = 0; y < mat->height; y++) {
        for (int x = 0; x < mat->width; x++) {
            printf("%4d", mat->data[y * mat->width + x]);
        }
        printf("\n");
    }
}

// 🖨️ affiche la matrice normalisée
void print_bayer_norm(const BayerMatrix *mat) {
    if (!mat || !mat->norm) {
        printf("(BayerMatrix norm non calculée)\n");
        return;
    }

    printf("BayerMatrix [%d x %d] (norm):\n", mat->width, mat->height);
    for (int y = 0; y < mat->height; y++) {
        for (int x = 0; x < mat->width; x++) {
            printf(" %5.2f", mat->norm[y * mat->width + x]);
        }
        printf("\n");
    }
}

