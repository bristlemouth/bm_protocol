#pragma once

#ifdef __cplusplus
extern "C" {
#endif

double doConcMg(double t, double p, double s);
double doConcUmol(double t, double p, double s);
double salinityFactor(double t, double s);

#ifdef __cplusplus
}
#endif
