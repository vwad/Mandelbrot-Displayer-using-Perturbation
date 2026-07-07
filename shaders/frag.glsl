#version 430

out vec4 fragmentColor;

uniform int width;
uniform int height;
uniform int iter_round_size;
uniform dvec2 center;
uniform int gradient_scale;
uniform int histogram_size;

#define LOG_2 0.69314718055995
#define ORBIT_LIMIT 100000

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

    int c_i = int(sqrt(float(iter) + 10.0 - n) * float(gradient_scale)) % histogram_size;
    
    fragmentColor = colors[c_i];
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
            double norm = (z_x * z_x) + (z_y * z_y);
            
            if (norm >= 4.0LF) { 
                break;
            }
            if (norm < (dz_x * dz_x) + (dz_y * dz_y)) {
                dz_x = z_x;
                dz_y = z_y;
                c = 0;
            }

            double old_dz_x = dz_x;
            double old_dz_y = dz_y;
            dz_x = old_dz_x * orbit[c].x - old_dz_y * orbit[c].y;
            dz_y = old_dz_y * orbit[c].x + old_dz_x * orbit[c].y;
            dz_x = dz_x * 2.0LF;
            dz_y = dz_y * 2.0LF;

            dz_x = dz_x + (old_dz_x*old_dz_x-old_dz_y*old_dz_y);
            dz_y = dz_y + 2.0LF * old_dz_x * old_dz_y;

            dz_x = dz_x + delta_x;
            dz_y = dz_y + delta_y;

            c++;

            z_x = orbit[c].x + dz_x;
            z_y = orbit[c].y + dz_y;
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
void main()
{
    int id = int(gl_FragCoord.x)+int(gl_FragCoord.y)*int(width);
    
    MandStep step = current_iter[id];
    perturbation(step,id);
    // int c = step.iter;
    // int max_iter = iter_round_size;
    
    // double x0 = remap(gl_FragCoord.x,0,width,tl.x,br.x);
    // double y0 = remap(gl_FragCoord.y,0,height,tl.y,br.y);

    // double x = c == 0 ? x0 : step.x;
    // double y = c == 0 ? y0 : step.y;
    // double x2 = x * x;
    // double y2 = y * y;

    // while (x2 + y2 <= 4.0 && c < max_iter) {
    //     y = (x + x) * y + y0;
    //     x = x2 - y2 + x0;
    //     x2 = x * x;
    //     y2 = y * y;
    //     c++;
    // }
    // if (c == max_iter) {
    //     step.is_finished = 0;
    //     step.x = x;
    //     step.y = y;
    //     step.iter = c;
    //     fragmentColor = vec4(0.0,0.0,0.0,1.0);
    // } else {
    //     step.is_finished = 1;
    //     step.x = x;
    //     step.y = y;
    //     step.iter = c;
    //     color(step,step.iter);
    // }
    // current_iter[id] = step;
}