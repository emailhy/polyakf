// by Jan Eric Kyprianidis <www.kyprianidis.com>
#extension GL_EXT_gpu_shader4 : enable
#pragma optionNV unroll all

uniform sampler2D src;
uniform sampler2D tfm;
uniform float radius;
uniform float q;
uniform float alpha;

const float PI = 3.14159265358979323846;
//const int N = 8;

//const float sigma_r = 0.5;

//const float sigma_r2 = sigma_r * sigma_r;

void main (void) {
    vec2 src_size = vec2(textureSize2D(src, 0));
    vec2 uv = gl_FragCoord.xy / src_size;

    vec4 m[4];
    vec3 s[4];
    for (int k = 0; k < 4; ++k) {
        m[k] = vec4(0.0);
        s[k] = vec3(0.0);
    }

    float piN = 2.0 * PI / float(4);
    mat2 X = mat2(cos(piN), sin(piN), -sin(piN), cos(piN));

    vec4 t = texture2D(tfm, uv);
    
    float a = radius * clamp((alpha + t.w) / alpha, 0.1, 2.0); 
    float b = radius * clamp(alpha / (alpha + t.w), 0.1, 2.0);

    float cos_phi = cos(t.z);
    float sin_phi = sin(t.z);

    //mat2 R = mat2(cos_phi, -sin_phi, sin_phi, cos_phi);
    //mat2 S = mat2(1.0/a, 0.0, 0.0, 1.0/b);
    //mat2 SR = S * R;
    mat2 SR = mat2(cos_phi/a, -sin_phi/b, sin_phi/a, cos_phi/b); 

    int max_x = int(sqrt(a*a * cos_phi*cos_phi +
                          b*b * sin_phi*sin_phi));
    int max_y = int(sqrt(a*a * sin_phi*sin_phi +
                          b*b * cos_phi*cos_phi));

    for (int j = 0; j <= max_y; ++j)  {
        for (int i = -max_x; i <= max_x; ++i) {
            if ((j !=0) || (i > 0)) {
                vec2 v = SR * vec2(float(i),float(j));
                float dot_v = dot(v,v);
                if (dot_v <= 1.0) {
                    vec4 c0_fix = texture2D(src, uv + vec2(i,j) / src_size);
                    vec3 c0 = c0_fix.rgb;
                    vec4 c1_fix = texture2D(src, uv - vec2(i,j) / src_size);
                    vec3 c1 = c1_fix.rgb;

                    vec3 cc0 = c0 * c0;
                    vec3 cc1 = c1 * c1;

                    float n = 0.0;
                    float wx[4];
                    /*
                    for(int k = 0; k < 4; ++k) {
                        //float z = (v.y + 0.5) - (1.0 * v.x * v.x);
                        //wx[k] = step(0.0, z)*z*z;
                        float z = max(0, (v.y + 0.5) - (1.0 * v.x * v.x));
                        wx[k] = z * z;
                        n += wx[k];
                        v *= X;
                    }
                    */
                    {
                        float z;
                        float xx = 0.33 - 0.84 * v.x * v.x;
                        float yy = 0.33 - 0.84 * v.y * v.y;

                        z = max(0.0, v.y + xx);
                        n += wx[0] = z * z;

                        z = max(0.0, -v.x + yy);
                        n += wx[1] = z * z;

                        z = max(0.0, -v.y + xx);
                        n += wx[2] = z * z;

                        z = max(0.0, v.x + yy);
                        n += wx[3] = z * z;
                    }
                
                    float g = exp(-3.125 * dot_v) / n;

                    for (int k = 0; k < 4; ++k) {
                        float w = wx[k] * g;
                        m[k] += vec4(c0 * w, w);
                        s[k] += cc0 * w;
                        m[(k+2)&3] += vec4(c1 * w, w);
                        s[(k+2)&3] += cc1 * w;
                    }
                }
            }
        }
    }

    vec4 o = vec4(0.0);
    for (int k = 0; k < 4; ++k) {
        m[k].rgb /= m[k].w;
        s[k] = abs(s[k] / m[k].w - m[k].rgb * m[k].rgb);

        float sigma2 = sqrt(s[k].r) + sqrt(s[k].g) + sqrt(s[k].b);
        float w = 1.0 / (1.0 + pow(255.0 * sigma2, q));
        o += vec4(m[k].rgb * w, w);
    }

    gl_FragColor = vec4(o.rgb / o.w, 1.0);
}
