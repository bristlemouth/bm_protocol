#include "do_calc.h"
#include <math.h>

static double saturatedWaterVaporPressure(double t, double s);
static double coStar(double t, double s);
static double pO2(double p);

double doConcMg(double t, double p, double s) {
  return (doConcUmol(t, p, s) * (2.0 * 15.9994) / 1000.0);
}

double doConcUmol(double t, double p, double s) {
  double Ptotal = p / 1013.25; // convert mbar to atm
  double pWSat = saturatedWaterVaporPressure(t, s);
  double pO2measured = pO2(Ptotal - pWSat);
  double pO2reference = pO2(1.0 - pWSat);
  return coStar(t, s) * pO2measured / pO2reference;
}

double salinityFactor(double t, double s) {
  if (s != 0.0) {
    return doConcMg(t, 101325.0, s) / doConcMg(t, 101325.0, 0.0);
  } else {
    return 1.0;
  }
}

static double saturatedWaterVaporPressure(double t, double s) {

  double TK = t + 273.15;

  return exp(24.4543 - 67.4509 * (100.0 / TK) - 4.8489 * log(TK / 100.0) - 0.000544 * s);
}

static double coStar(double t, double s) {
  double A0 = 2.00907;
  double A1 = 3.22014;
  double A2 = 4.05010;
  double A3 = 4.94457;
  double A4 = -0.256847;
  double A5 = 3.88767;
  double B0 = -6.24523e-3;
  double B1 = -7.37614e-3;
  double B2 = -1.03410e-2;
  double B3 = -8.17083e-3;
  double C0 = -4.88682e-7;

  /*---compute scaled temperature according to Eq. 8---*/
  double Ts = log((298.15 - t) / (273.15 + t));

  /*---compute Co* according to Eq. 8  (cm^3 O2 at STP / dm^3 water)---*/
  double result = exp(A0 + A1 * Ts + A2 * pow(Ts, 2) + A3 * pow(Ts, 3) +
                           A4 * pow(Ts, 4) + A5 * pow(Ts, 5) +
                           s * (B0 + B1 * Ts + B2 * pow(Ts, 2) + B3 * pow(Ts, 3)) +
                           C0 * pow(s, 2));

  return result * (1000.0 / 22.3916); // umol/L
}

static double pO2(double p) { return 0.209446 * p; }