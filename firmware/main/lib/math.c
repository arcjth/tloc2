#include <math.h>
#include "math.h"

void lin3_gaussian_solve(linSys3 *sys) {
    f64 mat[3][4];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) mat[i][j] = sys->A[i][j];
        mat[i][3] = sys->b[i];
    }

    for (int p = 0; p < 3; p++) {
        int maxRow = p;
        f64 maxVal = fabs(mat[p][p]);
        for (int i = p + 1; i < 3; i++) {
            if (fabs(mat[i][p]) > maxVal) {
                maxVal = fabs(mat[i][p]);
                maxRow = i;
            }
        }
        if (maxRow != p) {
            for (int j = 0; j < 4; j++) {
                f64 tmp = mat[p][j];
                mat[p][j] = mat[maxRow][j];
                mat[maxRow][j] = tmp;
            }
        }
        if (fabs(mat[p][p]) < 1e-24) {
            sys->solved = false;
            return;
        }
        for (int i = p + 1; i < 3; i++) {
            f64 factor = mat[i][p] / mat[p][p];
            for (int j = p; j < 4; j++) mat[i][j] -= factor * mat[p][j];
        }
    }

    f64 x[3];
    for (int i = 2; i >= 0; i--) {
        f64 sum = 0.0;
        for (int j = i + 1; j < 3; j++) sum += mat[i][j] * x[j];
        x[i] = (mat[i][3] - sum) / mat[i][i];
    }
    for (int i = 0; i < 3; i++) sys->x[i] = x[i];
    sys->solved = true;
}
