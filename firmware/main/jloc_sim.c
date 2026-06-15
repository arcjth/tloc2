#include "jloc2.h"
#include <stdio.h>
#include <math.h>

static const char *str_sim_menu = "\n%s=== Simulador/Calculadora TDOA ===%s\n1. Simular fonte do som\n2. Inserir r manual (calculadora)\n3. Sair\n> ";
static const char *str_sim_src  = "%sFonte simulada em (%.4f, %.4f) unidades%s\n";

void sim_generate_r(double src_x_unit, double src_y_unit, double r_out[3]) {
    printf(str_sim_src, COR1, src_x_unit, src_y_unit, COR0);
    vec2 src = {src_x_unit, src_y_unit};
    double d[4] = {0};
    for (int i = 0; i < 4; ++i) {
        double dx = src.i - MIC_POS_UNIT[i].i;
        double dy = src.j - MIC_POS_UNIT[i].j;
        d[i] = sqrt(dx*dx + dy*dy);
    }
    // r_unit = d_i - d_ref (exact in unit space)
    int k = 0;
    for (int i = 0; i < 4; ++i) {
        if (i == 0) continue;   // ref = m0
        r_out[k++] = d[i] - d[0];
    }
    printf(str_loc_r_unit, COR1, r_out[0], r_out[1], r_out[2], COR0);
}

void jloc_simulator(void) {
    linSys3 sys = {0};
    sndLoc2 loc = {0};
    double r_unit[3];
    unsigned int choice = 0;

    while (1) {
        printf(str_sim_menu, COR2, COR0);
        choice = get_value_uint(3);
        
        if (choice == 3) break;
        if (choice == 1) {                      // SIMULATE
            double x_unit = 0.0, y_unit = 0.0;
            printf("%sX unit: %s", COR1, COR0); x_unit = get_value();
            printf("%sY unit: %s", COR1, COR0); y_unit = get_value();
            printf("%lf\t%lf\n", x_unit, y_unit);
            sim_generate_r(x_unit, y_unit, r_unit);
        } else if (choice == 2) {               // MANUAL CALCULATOR
            printf("%sDigite r1, r2, r3 (unidades):%s\n", COR1, COR0);
            for (int i = 0; i < 3; ++i) r_unit[i] = get_value();
        }

        loc2d_build_tdoa_system(MIC_POS_UNIT, 0, r_unit, &sys);
        loc2d_solve_tdoa(&sys, &loc);

        // verificação cramer manual opcional
        if (loc.valid) {
            printf("%sVerificação manual (Cramer):%s\n", COR3, COR0);
            linSys3 sys_c = sys;               // copy
            lin3_cramer_solve(&sys_c);
        }
    }
}