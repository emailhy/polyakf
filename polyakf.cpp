//
// by Jan Eric Kyprianidis <www.kyprianidis.com>
// Copyright (C) 2010-2011 Computer Graphics Systems Group at the
// Hasso-Plattner-Institut, Potsdam, Germany <www.hpi3d.de>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
#include "GLee.h"
#include "tga.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
using namespace std;


const char *g_glsl[] = {
    "akf_v2n4.glsl",
    "akf_v3n4.glsl",
    "akf_v2n8.glsl",
    "akf_v3n8.glsl",
    "gauss.glsl",
    "sst.glsl",
    "tfm.glsl",
    NULL
};

enum {
    TEX_KRNLX4_N4,
    TEX_KRNLX4_N8,
    TEX_SRC,
    TEX_DST,
    TEX_TFM,
    TEX_TMP0,
    TEX_TMP1,
    TEX_MAX
};

map<const char*, GLuint> g_pid;
int g_width;
int g_height;
unsigned char *g_krnlx4_n4;
unsigned char *g_krnlx4_n8;
unsigned char *g_src;
unsigned char *g_dst;
GLuint g_tex[TEX_MAX];
GLuint g_fbo;


LRESULT CALLBACK wnd_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hWnd, message, wParam, lParam);
}


void create_context() {
    WNDCLASSEXA wcex;
    wcex.cbSize = sizeof(WNDCLASSEXA);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = wnd_proc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = 0;//hInstance;
    wcex.hIcon          = NULL;
    wcex.hCursor        = NULL;
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = "fastkuwClass";
    wcex.hIconSm        = NULL;
    RegisterClassExA(&wcex);

    HWND hwnd = CreateWindowA("fastkuwClass", "fastkuw", WS_OVERLAPPEDWINDOW, 0,0, 100, 100, NULL, NULL, NULL, NULL);
    HDC dc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = { 
        sizeof(PIXELFORMATDESCRIPTOR),   // size of this pfd 
        1,                     // version number 
        PFD_DRAW_TO_WINDOW |   // support window 
        PFD_SUPPORT_OPENGL,    // support OpenGL 
        PFD_TYPE_RGBA,         // RGBA type 
        24,                    // 24-bit color depth 
        0, 0, 0, 0, 0, 0,      // color bits ignored 
        0,                     // no alpha buffer 
        0,                     // shift bit ignored 
        0,                     // no accumulation buffer 
        0, 0, 0, 0,            // accum bits ignored 
        32,                    // 32-bit z-buffer 
        0,                     // no stencil buffer 
        0,                     // no auxiliary buffer 
        PFD_MAIN_PLANE,        // main layer 
        0,                     // reserved 
        0, 0, 0                // layer masks ignored 
    }; 

    int  pixel_format = ChoosePixelFormat(dc, &pfd); 
    SetPixelFormat(dc, pixel_format, &pfd); 

    HGLRC glrc = wglCreateContext(dc);
    wglMakeCurrent(dc, glrc);

    cout << "GL_VENDOR:                   " << glGetString(GL_VENDOR) << endl
         << "GL_RENDERER:                 " << glGetString(GL_RENDERER) << endl
         << "GL_VERSION:                  " << glGetString(GL_VERSION) << endl
         << "GL_SHADING_LANGUAGE_VERSION: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl 
         << endl;
}


void check() {
    if (!GLEE_VERSION_2_0 ||
        !GLEE_EXT_gpu_shader4 ||
        !GLEE_EXT_framebuffer_object ||
        !GLEE_ARB_texture_float ||
        !GLEE_ARB_texture_rectangle ||
        !GLEE_EXT_bgra) {
        cerr << "***ERROR***" << endl
             << "OpenGL 2.0 Graphics Card with EXT_gpu_shader4, EXT_framebuffer_object, " << endl 
             << "ARB_texture_rectangle, ARB_texture_float and EXT_bgra required!" << endl;
        exit(1);
    }
}


void build() {
    for (int k = 0; g_glsl[k]; ++k) {
        cerr << "Compiling: " << g_glsl[k] << endl; 
        ifstream is(g_glsl[k], ios::in | ios::binary | ios::ate);
        if (!is.is_open()) {
            cerr << "Can't open: " << g_glsl[k] << endl;
            exit(1);
        }
        size_t length = is.tellg();
        is.seekg(0, std::ios::beg);
        char* buffer = new char [length];
        is.read (buffer, length);
        is.close();

        GLuint shader_id = glCreateShader(GL_FRAGMENT_SHADER);
        GLint src_len = length;
        const char *src_ptr = buffer;
        glShaderSource(shader_id, 1, &src_ptr, &src_len);
        glCompileShader(shader_id);
        delete[] buffer;

        GLint len;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &len);
        if (len > 1) {
            char *log = new char[len];
            glGetShaderInfoLog(shader_id, len, NULL, log);
            cerr << log << endl;
            delete log;
        }

        GLint status;
        glGetShaderiv(shader_id, GL_COMPILE_STATUS, &status);
        if (!status) {
            cerr << "Compiling failed!" << endl;
            exit(1);
        }

        GLuint prog_id = glCreateProgram();
        g_pid[g_glsl[k]] = prog_id;
        glAttachShader(prog_id, shader_id);
        glLinkProgram(prog_id);

        glGetProgramiv(prog_id, GL_INFO_LOG_LENGTH, &len);
        if (len > 1) {
            char *log = new char[len];
            glGetProgramInfoLog(prog_id, len, NULL, log);
            cerr << log << endl;
            delete log;
        }

        glGetProgramiv(prog_id, GL_LINK_STATUS, &status);
        if (!status) {
            cerr << "Linking failed!" << endl;
            exit(1);
        }
    }
    cerr << endl;
}


void init() {
    if (!tga_load("test_512x512.tga", &g_src, &g_width, &g_height)) {
        cerr << "Loading test image failed!" << endl;
        exit(1);
    }
    int kw, kh;
    if (!tga_load("krnlx4_32n4.tga", &g_krnlx4_n4, &kw, &kh) ||
        !tga_load("krnlx4_32n8.tga", &g_krnlx4_n8, &kw, &kh)) {
        cerr << "Loading kernels failed!" << endl;
        exit(1);
    }

    glEnable(GL_TEXTURE_2D);
    glGenTextures(TEX_MAX, g_tex);
    for (int i = 0; i < TEX_MAX; ++i) {
        glBindTexture(GL_TEXTURE_2D, g_tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        switch(i) {
            case TEX_KRNLX4_N4:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_krnlx4_n4);
                break;
            case TEX_KRNLX4_N8:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_krnlx4_n8);
                break;
            case TEX_SRC:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, g_width, g_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_src);
                break;
            default:
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, g_width, g_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
                break;
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glGenFramebuffersEXT(1, &g_fbo);
}


void bind_sampler(GLuint id, const char *name, GLint unit, GLuint texture) {
    GLuint location = glGetUniformLocation(id, name);
    assert(glGetError() == GL_NO_ERROR);
    glUniform1i(location, unit);
    assert(glGetError() == GL_NO_ERROR);
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
}


double process(int algorithm, float sigma, float alpha, float radius, float q) {
    glPushAttrib(GL_ENABLE_BIT | GL_VIEWPORT_BIT );
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    glViewport(0, 0, g_width, g_height);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glFinish();

    LARGE_INTEGER p_freq, p_start, p_stop;
    QueryPerformanceFrequency(&p_freq);
    QueryPerformanceCounter(&p_start);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, g_fbo);
    {
        GLuint pid = g_pid["sst.glsl"];
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, g_tex[TEX_TMP0], 0);
        glUseProgram(pid);
        bind_sampler(pid, "src", 0, g_tex[TEX_SRC]);
        glRectf(-1,-1,1,1);

        pid = g_pid["gauss.glsl"];
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, g_tex[TEX_TMP1], 0);
        glUseProgram(pid);
        bind_sampler(pid, "src", 0, g_tex[TEX_TMP0]);
        glUniform1f(glGetUniformLocation(pid, "sigma"), sigma);
        glRectf(-1,-1,1,1);

        pid = g_pid["tfm.glsl"];
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, g_tex[TEX_TFM], 0);
        glUseProgram(pid);
        bind_sampler(pid, "src", 0, g_tex[TEX_TMP1]);
        glRectf(-1,-1,1,1);

        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, g_tex[TEX_DST], 0);
        if (algorithm == 0) {
            GLuint pid = g_pid["akf_v2n4.glsl"];
            glUseProgram(pid);
            bind_sampler(pid, "src", 0, g_tex[TEX_SRC]);
            bind_sampler(pid, "K0123", 1, g_tex[TEX_KRNLX4_N4]);
            bind_sampler(pid, "tfm", 2, g_tex[TEX_TFM]);
            glUniform1f(glGetUniformLocation(pid, "alpha"), alpha);
            glUniform1f(glGetUniformLocation(pid, "radius"), radius);
            glUniform1f(glGetUniformLocation(pid, "q"), q);
        } else if (algorithm == 1) {
            GLuint pid = g_pid["akf_v3n4.glsl"];
            glUseProgram(pid);
            bind_sampler(pid, "src", 0, g_tex[TEX_SRC]);
            bind_sampler(pid, "tfm", 2, g_tex[TEX_TFM]);
            glUniform1f(glGetUniformLocation(pid, "alpha"), alpha);
            glUniform1f(glGetUniformLocation(pid, "radius"), radius);
            glUniform1f(glGetUniformLocation(pid, "q"), q);
        } else if (algorithm == 2) {
            GLuint pid = g_pid["akf_v2n8.glsl"];
            glUseProgram(pid);
            bind_sampler(pid, "src", 0, g_tex[TEX_SRC]);
            bind_sampler(pid, "K0123", 1, g_tex[TEX_KRNLX4_N8]);
            bind_sampler(pid, "tfm", 2, g_tex[TEX_TFM]);
            glUniform1f(glGetUniformLocation(pid, "alpha"), alpha);
            glUniform1f(glGetUniformLocation(pid, "radius"), radius);
            glUniform1f(glGetUniformLocation(pid, "q"), q);
        } else {
            GLuint pid = g_pid["akf_v3n8.glsl"];
            glUseProgram(pid);
            bind_sampler(pid, "src", 0, g_tex[TEX_SRC]);
            bind_sampler(pid, "tfm", 2, g_tex[TEX_TFM]);
            glUniform1f(glGetUniformLocation(pid, "alpha"), alpha);
            glUniform1f(glGetUniformLocation(pid, "radius"), radius);
            glUniform1f(glGetUniformLocation(pid, "q"), q);
        }

        glRectf(-1,-1,1,1);
    }

    glUseProgram(0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    glFinish();
    double t = 0;
    QueryPerformanceCounter(&p_stop);
    t = (double)(p_stop.QuadPart - p_start.QuadPart) / p_freq.QuadPart;

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();

    return 1000 * t;
}


void save_dst(const char *path) {
    unsigned char *pixels = new unsigned char[4 * g_width * g_height];
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, g_fbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, g_tex[TEX_DST], 0);
    glReadPixels(0, 0, g_width, g_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    if (!tga_save(path, pixels, NULL, g_width, g_width, 32)) {
        cerr << "Saving " << path << " failed!" << endl;
        exit(1);
    }
    delete[] pixels;
}


void test() {
    for (int k = 0; k < 4; ++k) {
        process(k, 2, 1, 6, 8);
        char path[MAX_PATH];
        strcpy(path, "dst_0.tga");
        path[4] += k;
        save_dst(path);
    }
}


void benchmark() {
    static const int N = 100;

    double tm[4];
    for (int k = 0; k < 4; ++k) {
        vector<double> tv;
        tv.reserve(2*N);
        for (int i = 0; i < 2*N; ++i) {
            double t = process(k, 2, 1, 6, 8);
            tv.push_back(t);
        }
        sort(tv.begin(), tv.end());
    
        double sum = 0;
        for (int i = 0; i < N; ++i) {
            sum += tv[N/2 + i];
        }
        sum /= N;
        sum = floor(sum * 100.0 + 0.5) / 100.0;

        tm[k] = sum;
        cout << sum << " ms  ";
        if (k % 2 == 1) {
            double diff = floor((tm[k-1]-tm[k])/tm[k-1]*1000 + 0.5 ) / 10.0;
            cout << "(" << diff << ")    ";
        }
    }
    cout << endl;
}


int main(int argc, char** argv) {
    create_context();
    check();
    build();
    init();
    test();
    for (int i = 0; i < 10; ++i) benchmark();
}
