#ifndef FEATURE_EXTRACTION_H
#define FEATURE_EXTRACTION_H

float calc_mean(float* data, int len);

float calc_std(float* data, int len, float mean);

float calc_slope(float* y, int len);

float calc_rms(float* data, int len);

void extract_feature(float* output_features,
                     float* acc_x, float* acc_y, float* acc_z, float* acc_mag, int acc_len,
                     float* temp_hist, float* light_hist, int hist_len,
                     float current_temp, float current_light);

#endif