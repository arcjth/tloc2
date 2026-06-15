#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include "jlinear.h"
#include "jthio.h"

static const char *str_lin_title = "\n%s=== Sistema Linear 3x3 ===%s\n";
static const char *str_lin_solved_gauss = "%sResolvido com Eliminação Gaussiana%s\n";
static const char *str_lin_solved_cramer = "%sResolvido com Regra de Cramer (manual)%s\n";
static const char *str_lin_det_zero = "\n%sAVISO: Matriz singular (det ≈ 0)!%s\n";

static inline vec2 vec2_addsub(vec2 vec_a, vec2 vec_b) {
    return (vec2){vec_a.i + vec_b.i, vec_a.j + vec_b.j};
}
static inline vec2 vec2_scale(vec2 vec, double k) {
    return (vec2){vec.i * k, vec.j * k};
}
static inline vec2 polar2rect(pol2 vec) {
    return (vec2){vec.mag * cos(vec.angle), vec.mag * sin(vec.angle)};
}
static inline pol2 rect2polar(vec2 vec) {
    return (pol2){sqrt((vec.i * vec.i)+(vec.j * vec.j)), atan2(vec.j, vec.i)};
}
static inline pol2 pol2_mult(pol2 a, pol2 b) {
    return (pol2){a.mag * b.mag, a.angle + b.angle};
}
static inline pol2 pol2_div(pol2 a, pol2 b) {
    return (pol2){a.mag / b.mag, a.angle - b.angle};
}

// Impressão genérica da matriz
void lin3_print(const char *nome, linSys3 *sys) {
    printf(str_lin_title, COR2, COR0);
    printf("%sMatriz A:%s\n", COR1, COR0);
    for (int i = 0; i < 3; ++i) {
        printf("   %.4f   %.4f   %.4f\n", sys->A[i][0], sys->A[i][1], sys->A[i][2]);
    }
    printf("%sVetor b:%s\n", COR1, COR0);
    printf("   %.4f\n   %.4f\n   %.4f\n", sys->b[0], sys->b[1], sys->b[2]);
}

// determinante 3x3
double lin3_det(const double A[3][3]) {
    return A[0][0]*(A[1][1]*A[2][2] - A[1][2]*A[2][1]) -
           A[0][1]*(A[1][0]*A[2][2] - A[1][2]*A[2][0]) +
           A[0][2]*(A[1][0]*A[2][1] - A[1][1]*A[2][0]);
}

// eliminação Gaussiana
void lin3_gaussian_solve(linSys3 *sys) {
    double mat[3][4]; // matriz aumentada [A|b]
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) mat[i][j] = sys->A[i][j];
        mat[i][3] = sys->b[i];
    }

    // eliminação progressiva com pivoteamento parcial
    for (int p = 0; p < 3; ++p) {
        int maxRow = p;
        double maxVal = fabs(mat[p][p]);
        for (int i = p + 1; i < 3; ++i) {
            if (fabs(mat[i][p]) > maxVal) {
                maxVal = fabs(mat[i][p]);
                maxRow = i;
            }
        }
        // troca de linhas
        for (int j = 0; j < 4; ++j) {
            double tmp = mat[p][j];
            mat[p][j] = mat[maxRow][j];
            mat[maxRow][j] = tmp;
        }

        if (fabs(mat[p][p]) < 1e-24) {
            printf(str_lin_det_zero, COR3, COR0);
            sys->solved = false;
            return;
        }

        // remoção
        for (int i = p + 1; i < 3; ++i) {
            double factor = mat[i][p] / mat[p][p];
            for (int j = p; j < 4; ++j) {
                mat[i][j] -= factor * mat[p][j];
            }
        }
    }

    // Substituição regressiva
    double x[3];
    for (int i = 2; i >= 0; --i) {
        double sum = 0.0;
        for (int j = i + 1; j < 3; ++j) sum += mat[i][j] * x[j];
        x[i] = (mat[i][3] - sum) / mat[i][i];
    }

    for (int i = 0; i < 3; ++i) sys->x[i] = x[i];
    sys->solved = true;
    printf(str_lin_solved_gauss, COR2, COR0);
}

// Regra de Cramer (método puramente manual, ótimo para verificação à mão)
void lin3_cramer_solve(linSys3 *sys) {
    sys->detA = lin3_det(sys->A);
    if (fabs(sys->detA) < 1e-24) {
        printf(str_lin_det_zero, COR3, COR0);
        sys->solved = false;
        return;
    }

    // Matriz Ax (substitui coluna 0 por b)
    double Ax[3][3] = {{sys->b[0], sys->A[0][1], sys->A[0][2]},
                       {sys->b[1], sys->A[1][1], sys->A[1][2]},
                       {sys->b[2], sys->A[2][1], sys->A[2][2]}};
    // Ay (coluna 1)
    double Ay[3][3] = {{sys->A[0][0], sys->b[0], sys->A[0][2]},
                       {sys->A[1][0], sys->b[1], sys->A[1][2]},
                       {sys->A[2][0], sys->b[2], sys->A[2][2]}};
    // Ad (coluna 2)
    double Ad[3][3] = {{sys->A[0][0], sys->A[0][1], sys->b[0]},
                       {sys->A[1][0], sys->A[1][1], sys->b[1]},
                       {sys->A[2][0], sys->A[2][1], sys->b[2]}};

    sys->x[0] = lin3_det(Ax) / sys->detA;
    sys->x[1] = lin3_det(Ay) / sys->detA;
    sys->x[2] = lin3_det(Ad) / sys->detA;

    sys->solved = true;
    printf(str_lin_solved_cramer, COR3, COR0);
}
