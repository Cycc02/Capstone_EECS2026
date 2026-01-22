#ifndef FEATURE_EXTRACTION_H
#define FEATURE_EXTRACTION_H

#include "feature_extraction.h"
#include <math.h>

float calc_mean(float *data, int len){
    float sum = 0.0;
    for (int i = 0; i < len ; i++) sum += data[i];
    return sum/len;
}

float calc_std(float *data, int len, float mean){
    float variance = 0.0;
    for(int i = 0; i < len ; i++) variance += pow(data[i] - mean, 2);
    return sqrt(variance/len);
}

float calc_slope(float *y, int len){
    if(len < 2) return 0.0;
    float sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
    for(int i = 0; i < len; i++){
        sum_x += i;
        sum_y += y[i];
        sum_xy += i * y[i];
        sum_xx += i*i;
    }
    float numerator = (len*sum_xy) - (sum_x*sum_y);
    float denominator = (len*sum_xx) - (sum_x*sum_x);
    if(denominator == 0) return 0;
    return numerator/denominator;
}

float calc_rms(float *data, int len) {
    float sum_sq = 0.0;
    for(int i=0; i<len; i++) sum_sq += pow(data[i], 2);
    return sqrt(sum_sq / len);
}

void extract_feature(float* output_features,
                     float* acc_x, float* acc_y, float* acc_z, float* acc_mag, int acc_len,
                     float* temp_hist, float* light_hist, int hist_len,
                     float current_temp, float current_light){

                        int idx = 0;

                        //TEMPERATURE
                        float t_min = temp_hist[0];
                        float t_max = temp_hist[0];

                        for(int i = 1; i < hist_len ; i++){
                            if(temp_hist[i] < t_min) t_min = temp_hist[i];
                            if(temp_hist[i] > t_max) t_max = temp_hist[i];
                        }

                        float t_mean = calc_mean(temp_hist, hist_len);
                        float t_std  = calc_std(temp_hist, hist_len, t_mean);
                        float t_slope = calc_slope(temp_hist, hist_len);
                        float t_roc = (hist_len >= 2)? (temp_hist[hist_len - 1] - temp_hist[0]) : 0;
                        float t_zscore = (t_std > 0)?
                            (temp_hist[hist_len - 1] - t_mean) / t_std : 0;

                        output_features[idx++] = t_mean;
                        output_features[idx++] = t_std;
                        output_features[idx++] = t_min;
                        output_features[idx++] = t_max;
                        output_features[idx++] = t_slope;
                        output_features[idx++] = t_roc;
                        output_features[idx++] = t_zscore;

                        //LIGHT
                        float l_min = light_hist[0];
                        float l_max = light_hist[0];

                        for(int i = 1; i < hist_len; i++){
                            if(light_hist[i] < l_min) l_min = light_hist[i];
                            if(light_hist[i] > l_max) l_max = light_hist[i];
                        }

                        float l_mean = calc_mean(light_hist, hist_len);
                        float l_std   = calc_std(light_hist, hist_len, l_mean);
                        float l_slope = calc_slope(light_hist, hist_len);
                        float l_roc   = (hist_len >= 2) ? (light_hist[hist_len-1] - light_hist[0]) : 0;
                        float l_zscore = (l_std > 0)?
                          (light_hist[hist_len - 1] - l_mean) / l_std : 0;

                        output_features[idx++] = l_mean; 
                        output_features[idx++] = l_std; 
                        output_features[idx++] = l_min;
                        output_features[idx++] = l_max; 
                        output_features[idx++] = l_slope; 
                        output_features[idx++] = l_roc; 
                        output_features[idx++] = l_zscore;

                        float* axes[] = {acc_x, acc_y, acc_z};
                        for(int k = 0; k < 3; k++){
                            float amin = axes[k][0];
                            float amax = axes[k][0];

                            for(int i = 0; i < acc_len; i++){
                                if(axes[k][i] < amin) amin = axes[k][i];
                                if(axes[k][i] > amax) amax = axes[k][i];
                            }

                            float mean = calc_mean(axes[k], acc_len);
                            float std = calc_std(axes[k], acc_len, mean);

                            output_features[idx++] = mean;
                            output_features[idx++] = std;
                            output_features[idx++] = amin;
                            output_features[idx++] = amax;
                            output_features[idx++] = amax - amin; // Range
                            output_features[idx++] = calc_rms(axes[k], acc_len);
                            output_features[idx++] = calc_slope(axes[k], acc_len);
                        }

                        //ACC MAGNITUDE
                        float m_mean = calc_mean(acc_mag, acc_len);
                        output_features[idx++] = m_mean;
                        output_features[idx++] = calc_std(acc_mag, acc_len, m_mean);
                        float m_max = acc_mag[0];
                        for(int i = 0; i < acc_len; i++){
                            if(acc_mag[i] > m_max) m_max = acc_mag[i];
                        }
                        output_features[idx++] = m_max;
                    } 

#endif



