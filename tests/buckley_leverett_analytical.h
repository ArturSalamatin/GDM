#pragma once
#include <vector>
#include <cmath>

namespace buckley_leverett {

inline double f_w(double Sw, double M) {
    double s3 = Sw * Sw * Sw;
    double q3 = (1.0 - Sw) * (1.0 - Sw) * (1.0 - Sw);
    double denom = M * s3 + q3;
    if (denom < 1e-30) return 0.0;
    return M * s3 / denom;
}

inline double df_w(double Sw, double M) {
    double s2 = Sw * Sw;
    double q2 = (1.0 - Sw) * (1.0 - Sw);
    double s3 = s2 * Sw;
    double q3 = q2 * (1.0 - Sw);
    double denom = M * s3 + q3;
    if (denom < 1e-30) return 0.0;
    return 3.0 * M * s2 * q2 / (denom * denom);
}

inline double find_Swf(double M, double tol = 1e-12) {
    double lo = 0.01, hi = 0.99;
    while (hi - lo > tol) {
        double mid = 0.5 * (lo + hi);
        double val = df_w(mid, M) - f_w(mid, M) / mid;
        if (val > 0) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

inline std::vector<double> analytical_profile(
    const std::vector<double>& x_centers,
    double qt, double phi, double A, double t, double M)
{
    double Swf = find_Swf(M);
    double v_front = qt * df_w(Swf, M) / (phi * A);
    double x_front = v_front * t;

    std::vector<double> Sw(x_centers.size(), 0.0);
    for (size_t i = 0; i < x_centers.size(); ++i) {
        double x = x_centers[i];
        if (x >= x_front) {
            Sw[i] = 0.0;
        } else {
            double target = x * phi * A / (qt * t);
            double lo = Swf, hi = 1.0 - 1e-10;
            for (int iter = 0; iter < 100; ++iter) {
                double mid = 0.5 * (lo + hi);
                if (df_w(mid, M) > target) lo = mid;
                else hi = mid;
            }
            Sw[i] = 0.5 * (lo + hi);
        }
    }
    return Sw;
}

} // namespace buckley_leverett
