#version 430

out vec4 fragmentColor;

uniform int width;
uniform int height;
uniform int iter_round_size;
uniform int max_ref_iter;
uniform int gradient_scale;
uniform int histogram_size;
uniform dvec2 center;
uniform double mpower;

#define LOG_2 0.69314718055995
#define APPR_LIMIT 10

struct MandStep {
    double dz_x;
    double dz_y;
    double delta_x;
    double delta_y;
    int iter;
    int index;
    int is_finished;
};

layout(binding = 2, std430) buffer MandBuffer {
    MandStep current_iter[];
};

layout(binding = 3, std430) buffer OrbitBuffer {
    dvec2 orbit[];
};

layout(binding = 4, std430) buffer ColorBuffer {
    vec4 colors[];
};

double remap(double x, double a0, double a1, double b0, double b1) {
    return b0 + (x - a0) * (b1 - b0) / (a1 - a0);
}

void color(float x, float y, int iter) {
    float lg = log(x * x + y * y) / 2.0f;
    
    float n = log(lg / LOG_2) / LOG_2;

    float i = sqrt(float(iter) + 10.0 - n) * float(gradient_scale);

    int id1 = int(i) % histogram_size;
    int id2 = (id1+1)%histogram_size;
    float b = fract(i);
    
    fragmentColor = mix(colors[id1],colors[id2],b);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 getHsvColor(float i, float max_iter) {
    float t = i / max_iter;
    float hue = mod(pow(t * 360.0, 1.5), 360.0) / 360.0;
    vec3 hsv = vec3(hue, 1.0, t*3.0);
    return hsv2rgb(hsv);

}

dvec2 div_compl(dvec2 v1, dvec2 v2) {
    double norm = v2.x*v2.x + v2.y*v2.y;

    return dvec2((v1.x*v2.x+v1.y*v2.y)/norm,(v1.y*v2.x-v1.x*v2.y)/norm);
}

dvec2 div_compl(dvec2 v1, double s) {
    return dvec2(v1.x/s,v1.y/s);
}

dvec2 multiply_compl(dvec2 v1, double s) {
    return dvec2(v1.x*s,v1.y*s);
}

dvec2 multiply_compl(dvec2 v1, dvec2 v2) {
    return dvec2(v1.x*v2.x-v1.y*v2.y,v1.x*v2.y+v2.x*v1.y);
}

dvec2 multiply_compl(double x1, double y1, double x2, double y2) {
    return dvec2(x1*x2-y1*y2,x1*y2+x2*y1);
}

dvec2 complex_power(double re, double im, double power) {
    double len = sqrt(re * re + im * im);
    
    double a;
    if (re > 0.0LF) a = atan(float(im / re));
    else if (re < 0.0LF && im >= 0.0LF) a = atan(float(im / re)) + 3.141592653589793LF;
    else if (re < 0.0LF && im < 0.0LF) a = atan(float(im / re)) - 3.141592653589793LF;
    else if (re == 0.0LF && im > 0.0LF) a = 1.5707963267948966LF;
    else if (re == 0.0LF && im < 0.0LF) a = -1.5707963267948966LF;
    else a = 0.0LF;

    len = pow(float(len), float(power));
    float apow = float(a * power);
    return dvec2(len * cos(apow), len * sin(apow));
}

dvec2 approximation_powers(double re, double im, double epsilon_re, double epsilon_im, double power) {
    dvec2 epsilon_start = dvec2(epsilon_re, epsilon_im);
    dvec2 epsilon = epsilon_start;
    double z_mag_sq = re * re + im * im;

    dvec2 result = multiply_compl(complex_power(re, im, power - 1.0LF), epsilon);
    result = multiply_compl(result, power);

    double scalar = power;
    double current_z_power = power - 1.0LF;

    // Loop for higher order terms
    for (int i = 1; i < APPR_LIMIT; i++) {
        current_z_power -= 1.0LF; 
        
        scalar = scalar * (power - double(i)) / double(i + 1);
        
        epsilon = multiply_compl(epsilon, epsilon_start);
        
        dvec2 term = multiply_compl(epsilon, scalar);
        term = multiply_compl(term, complex_power(re, im, current_z_power));
        
        result = result + term;
        
    }

    return result;
}

void perturbation(MandStep step, int id) {
    double dz_x = step.dz_x;
    double dz_y = step.dz_y;
    int c = step.index;
    int cd = step.iter;
    double delta_x = step.delta_x;
    double delta_y = step.delta_y;
    double z_x = orbit[c].x + dz_x;
    double z_y = orbit[c].y + dz_y;
    if (step.is_finished == 0) {
        while (cd < iter_round_size) {
            // With series approximation this will break
            if (mpower != 2.0LF) {
                if (orbit[c].x == 0.0LF && orbit[c].y == 0.0LF) {
                    dvec2 series = complex_power(dz_x, dz_y, mpower);
                    dz_x = series.x + delta_x;
                    dz_y = series.y + delta_y;
                } else {
                    dvec2 series = approximation_powers(orbit[c].x, orbit[c].y, dz_x, dz_y, mpower);
                    dz_x = series.x + delta_x;
                    dz_y = series.y + delta_y;
                }
            } else {
                dvec2 series = multiply_compl(multiply_compl(orbit[c],dvec2(dz_x,dz_y)),2.0LF);
                series = series + multiply_compl(dz_x,dz_y,dz_x,dz_y);
                dz_x = series.x + delta_x;
                dz_y = series.y + delta_y;
            }

            c++;
            
            z_x = orbit[c].x + dz_x;
            z_y = orbit[c].y + dz_y;

            double norm = (z_x * z_x) + (z_y * z_y);
             
            if (norm >= 32.0LF) { 
                break;
            }
            if (norm < (dz_x * dz_x) + (dz_y * dz_y) || c == max_ref_iter) {
                dz_x = z_x;
                dz_y = z_y;
                c = 0;
            }
            cd++;
        }
        step.dz_x = dz_x;
        step.dz_y = dz_y;
        step.iter = cd;
        step.index = c;
    }
    if (cd == iter_round_size) {
        step.is_finished = 0;
        fragmentColor = vec4(0.0,0.0,0.0,1.0);
    } else {
        step.is_finished = 1;
        color(float(z_x),float(z_y),step.iter);
    }
    current_iter[id] = step;
}

void escape_time(MandStep step, int id) {
    int c = step.iter;
    int max_iter = iter_round_size;
    
    double x0 = center.x+step.delta_x;
    double y0 = center.y+step.delta_y;

    double x = c == 0 ? x0 : step.dz_x;
    double y = c == 0 ? y0 : step.dz_y;
    double x2 = x * x;
    double y2 = y * y;
    if (step.is_finished == 0) {
        while (x2 + y2 <= 40.0 && c < iter_round_size) {
            dvec2 res = complex_power(x,y,mpower);
            x = res.x + x0;
            y = res.y + y0;
            x2 = x * x;
            y2 = y * y;
            c++;
        }
        step.dz_x = x;
        step.dz_y = y;
        step.iter = c;
    }
    if (c == iter_round_size) {
        step.is_finished = 0;
        fragmentColor = vec4(0.0,0.0,0.0,1.0);
    } else {
        step.is_finished = 1;
        color(float(x),float(y),c);
    }
    current_iter[id] = step;
}

void main()
{
    int id = int(gl_FragCoord.x)+int(gl_FragCoord.y)*int(width);
    
    MandStep step = current_iter[id];
    // escape_time(step,id);
    if (mpower >= 0) {
        perturbation(step,id);
    } else {
        escape_time(step,id);
    }
}