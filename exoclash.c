
// gcc bayer_mo5_exoquant.c -O2 -o bayer_mo5_exoquant -lm
// Usage: ./bayer_mo5_exoquant input.png output.png

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#define W_THOM       320
#define H_THOM       200
#define PALETTE_SIZE 16
#define COLOR_ONE    255.0f

typedef struct { float r, g, b; } Color;
typedef struct { int c1, c2; } Couple;

// niveaux linéaires issus de thomson.levels.pc /255
static const float thom_levels_linear[16] = {
    0.0f/255.0f, 103.0f/255.0f,130.0f/255.0f,149.0f/255.0f,
    164.0f/255.0f,177.0f/255.0f,188.0f/255.0f,198.0f/255.0f,
    207.0f/255.0f,215.0f/255.0f,223.0f/255.0f,230.0f/255.0f,
    237.0f/255.0f,243.0f/255.0f,249.0f/255.0f,255.0f/255.0f
};

static Color palette[PALETTE_SIZE];
static float bayer8[8][8];

// initialisation de palette MO5 (3 bits RGB + 1 bit intensité)
static void init_palette(void) {
    for(int i=0;i<PALETTE_SIZE;i++){
        int I=(i>>3)&1, R=(i>>0)&1, G=(i>>1)&1, B=(i>>2)&1;
        int lvl= I?15:8;
        int r4= R?lvl:0, g4= G?lvl:0, b4= B?lvl:0;
        palette[i].r = thom_levels_linear[r4];
        palette[i].g = thom_levels_linear[g4];
        palette[i].b = thom_levels_linear[b4];
    }
}

// Bayer 8×8 issu de double(double({1,2},{3,4})) normalisé [0..1)
static void init_bayer(void) {
    int b2[2][2] = {{1,2},{3,4}}, b4[4][4], b8i[8][8];
    for(int y=0;y<2;y++)for(int x=0;x<2;x++){
        int v=b2[y][x];
        b4[y*2+0][x*2+0]=v*4-3;
        b4[y*2+0][x*2+1]=v*4-2;
        b4[y*2+1][x*2+0]=v*4-1;
        b4[y*2+1][x*2+1]=v*4-0;
    }
    for(int y=0;y<4;y++)for(int x=0;x<4;x++){
        int v=b4[y][x];
        b8i[y*2+0][x*2+0]=v*4-3;
        b8i[y*2+0][x*2+1]=v*4-2;
        b8i[y*2+1][x*2+0]=v*4-1;
        b8i[y*2+1][x*2+1]=v*4-0;
    }
    int maxv=0;
    for(int y=0;y<8;y++)for(int x=0;x<8;x++)
        if(b8i[y][x]>maxv) maxv=b8i[y][x];
    for(int y=0;y<8;y++)for(int x=0;x<8;x++)
        bayer8[y][x] = (b8i[y][x]-1)/(float)(maxv-1);
}

// vecteurs de diffusion d'erreur verticale
static Color *err_cur, *err_next;

// opérations sur Color
static inline Color c_add(Color a, Color b){ return (Color){a.r+b.r, a.g+b.g, a.b+b.b}; }
static inline Color c_sub(Color a, Color b){ return (Color){a.r-b.r, a.g-b.g, a.b-b.b}; }
static inline Color c_mul(Color a, float k){ return (Color){a.r*k, a.g*k, a.b*k}; }
static inline float clamp01(float v){ return v<0?0:(v>1?1:v); }

// distance perceptuelle pondérée (8R,11G,8B)^1.8
static inline float dist(Color a, Color b){
    float dr=fabsf(a.r-b.r)*8.0f;
    float dg=fabsf(a.g-b.g)*11.0f;
    float db=fabsf(a.b-b.b)*8.0f;
    return powf(dr,1.8f)+powf(dg,1.8f)+powf(db,1.8f);
}

// dimensions d'échantillonnage centré/échelle
typedef struct { float sx, sy, ox, oy; } MapCtx;
static MapCtx make_map(int w,int h){
    MapCtx m; float asp=(float)w/h;
    if(asp<1.6f){
        m.sx=(float)h/200; m.sy=(float)h/200;
        float tw=1.6f*h; m.ox=(w-tw)/2; m.oy=0;
    } else {
        m.sx=(float)w/320; m.sy=(float)w/320;
        float th=(float)w/1.6f; m.ox=0; m.oy=(h-th)/2;
    }
    return m;
}

// moyenne des pixels recouvrant [x,x+1]×[y,y+1] en coords Thomson
static Color sample_cell(const unsigned char *img,int w,int h,MapCtx m,int x,int y){
    float x1=x*m.sx+m.ox, y1=y*m.sy+m.oy;
    float x2=(x+1)*m.sx+m.ox, y2=(y+1)*m.sy+m.oy;
    int ix1=fmaxf(0,floorf(x1)), iy1=fmaxf(0,floorf(y1));
    int ix2=fminf(w,ceilf(x2)),  iy2=fminf(h,ceilf(y2));
    if(ix2<=ix1) ix2=ix1+1;
    if(iy2<=iy1) iy2=iy1+1;
    Color acc={0,0,0}; int cnt=0;
    for(int j=iy1;j<iy2;j++){
        const unsigned char *row = img + (size_t)j*w*3;
        for(int i=ix1;i<ix2;i++){
            const unsigned char *p = row + (size_t)i*3;
            acc.r += p[0]/COLOR_ONE;
            acc.g += p[1]/COLOR_ONE;
            acc.b += p[2]/COLOR_ONE;
            cnt++;
        }
    }
    if(cnt>0){ acc.r/=cnt; acc.g/=cnt; acc.b/=cnt; }
    return acc;
}

// trouve le meilleur couple (i<j) minimisant sum_k min(dist(C[k],p[i]),dist(C[k],p[j]))
static Couple find_best_couple(Color C[8]){
    float best_cost=1e30f;
    Couple best={0,1};
    for(int i=0;i<PALETTE_SIZE-1;i++){
        for(int j=i+1;j<PALETTE_SIZE;j++){
            float cost=0;
            for(int k=0;k<8;k++){
                float d1=dist(C[k], palette[i]);
                float d2=dist(C[k], palette[j]);
                cost += fminf(d1,d2);
            }
            if(cost<best_cost){
                best_cost=cost;
                best.c1=i; best.c2=j;
            }
        }
    }
    return best;
}

// calcule amplitude Bayer = demi-pas minimal de la palette
static float compute_amp(void){
    float v0  = thom_levels_linear[0];
    float v8  = thom_levels_linear[8];
    float v15 = thom_levels_linear[15];
    float s1=fabsf(v8-v0), s2=fabsf(v15-v8);
    return 0.5f * fminf(s1,s2);
}

int main(int argc,char **argv){
    if(argc<3){
        fprintf(stderr,"Usage: %s in.(png/jpg) out.png\n",argv[0]);
        return 1;
    }

    int iw,ih,nc;
    unsigned char *in = stbi_load(argv[1],&iw,&ih,&nc,3);
    if(!in){ fprintf(stderr,"Erreur chargement\n"); return 1; }

    init_palette();
    init_bayer();
    float amp = compute_amp();

    // buffers
    unsigned char *out = malloc(W_THOM*H_THOM*3);
    err_cur  = calloc(W_THOM,sizeof(Color));
    err_next = calloc(W_THOM,sizeof(Color));

    MapCtx map = make_map(iw, ih);

    for(int y=0;y<H_THOM;y++){
        // pour chaque bloc de 8
        for(int x0=0;x0<W_THOM;x0+=8){
            Color C[8];
            int px;
            // 1) échantillon + erreur + Bayer
            for(int k=0;k<8;k++){
                px = x0 + k;
                Color c = sample_cell(in,iw,ih,map,px,y);
                c = c_add(c, err_cur[px]);
                // bias Bayer [-1..+1]*amp
                float b = (bayer8[y&7][px&7]*2 - 1)*amp;
                c.r = clamp01(c.r + b);
                c.g = clamp01(c.g + b);
                c.b = clamp01(c.b + b);
                C[k] = c;
            }
            // 2) meilleur couple
            Couple cp = find_best_couple(C);

            // 3) quantize + diffusion
            for(int k=0;k<8;k++){
                px = x0 + k;
                // choisir i ou j
                float d1 = dist(C[k], palette[cp.c1]);
                float d2 = dist(C[k], palette[cp.c2]);
                int pick = (d1<d2?cp.c1:cp.c2);
                // écrire pixel
                size_t oi = ((size_t)y*W_THOM + px)*3;
                Color pc = palette[pick];
                out[oi+0] = (uint8_t)roundf(pc.r*255);
                out[oi+1] = (uint8_t)roundf(pc.g*255);
                out[oi+2] = (uint8_t)roundf(pc.b*255);
                // erreur = C[k]-pc
                Color e = c_sub(C[k], pc);
                err_next[px] = c_add(err_next[px], c_mul(e, 0.6f));
            }
        }
        // swap erreurs
        memcpy(err_cur, err_next, W_THOM*sizeof(Color));
        memset(err_next, 0, W_THOM*sizeof(Color));
        if((y&7)==0) fprintf(stderr,"Progress: %d%%\r",(y*100)/H_THOM);
    }
    fprintf(stderr,"Progress: 100%%\n");

    stbi_write_png(argv[2], W_THOM, H_THOM, 3, out, W_THOM*3);

    free(in);
    free(out);
    free(err_cur);
    free(err_next);
    return 0;
}
