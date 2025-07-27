#include "glad_egl.c"
#include "glad.c"

#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include <vector>
#include <math.h>

static inline int64_t getHPCounter()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t i64 = now.tv_sec*INT64_C(1000000000) + now.tv_nsec;
    return i64;
}

static inline int64_t getHPFrequency()
{
    return INT64_C(1000000000);
}

enum
{
    EglOffscreenDeviceNumber = 0,
};

EGLContext g_context;
EGLDisplay g_display;
EGLSurface g_surface;
EGLConfig  g_config;

int32_t createEglContext()
{
    if (!gladLoadEGL())
    {
        fprintf(stderr, "Failed to initialize GLAD EGL\n");
        exit(1);
    }

    EGLint deviceCount;
    eglQueryDevicesEXT(0, NULL, &deviceCount);
    fprintf(stderr, "device count: %d\n", deviceCount);

    if (deviceCount == 0)
    {
        fprintf(stderr, "No EGL devices found!");
        exit(1);
    }

    EGLint numDevices;
    EGLDeviceEXT* devices = new EGLDeviceEXT[deviceCount];
    eglQueryDevicesEXT(deviceCount, devices, &numDevices);

    g_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, devices[EglOffscreenDeviceNumber], NULL);
    fprintf(stderr, "g_display: %p\n", g_display);

    EGLint major = 0;
    EGLint minor = 0;
    EGLBoolean success = eglInitialize(g_display, &major, &minor);
    fprintf(stderr, "major %d minor: %d\n", major, minor);

    fprintf(stderr, "EGL info:\n");
    const char* clientApis = eglQueryString(g_display, EGL_CLIENT_APIS);
    fprintf(stderr, "   APIs: %s\n", clientApis);

    const char* vendor = eglQueryString(g_display, EGL_VENDOR);
    fprintf(stderr, " Vendor: %s\n", vendor);

    const char* version = eglQueryString(g_display, EGL_VERSION);
    fprintf(stderr, "Version: %s\n", version);

    EGLint attrs[] =
    {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        //EGL_CONFORMANT, EGL_OPENGL_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        //EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };

    EGLint numConfig = 0;
    success = eglChooseConfig(g_display, attrs, &g_config, 1, &numConfig);

    static const EGLint pbufferAttribs[] = {
        EGL_WIDTH,  (EGLint)1,
        EGL_HEIGHT, (EGLint)1,
        EGL_NONE,
    };

    g_surface    = eglCreatePbufferSurface(g_display, g_config, pbufferAttribs);

    eglBindAPI(EGL_OPENGL_API);

    //int32_t flags = EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
    int32_t flags = 0;
    static const int s_contextAttrs[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 4,
        EGL_CONTEXT_MINOR_VERSION_KHR, 6,
        EGL_CONTEXT_FLAGS_KHR, flags,
        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
        EGL_NONE,
    };

    g_context = eglCreateContext(g_display, g_config, EGL_NO_CONTEXT, s_contextAttrs);
    success = eglMakeCurrent(g_display, g_surface, g_surface, g_context);
    if (!success)
    {
        fprintf(stderr, "eglMakeCurrent failure!\n");
        exit(1);
    }

    fprintf(stderr, "EGL context: %p success: %d\n", g_context, success);

    if (!gladLoadGLLoader((GLADloadproc) eglGetProcAddress))
    {
        fprintf(stderr, "Failed to initialize GLAD\n");
        exit(1);
    }

    return 0;
}

void doSparseTest()
{
    createEglContext();

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLsizei const Size(32768);
    std::size_t const Levels = 1 + log2f(float(Size));
    std::size_t const MaxLevels = 4;

    GLuint TextureName;
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &TextureName);
    glTextureParameteri(TextureName, GL_TEXTURE_SWIZZLE_R, GL_RED);
    glTextureParameteri(TextureName, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
    glTextureParameteri(TextureName, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
    glTextureParameteri(TextureName, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
    glTextureParameteri(TextureName, GL_TEXTURE_BASE_LEVEL, 0);
    glTextureParameteri(TextureName, GL_TEXTURE_MAX_LEVEL, MaxLevels - 1);
    glTextureParameteri(TextureName, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTextureParameteri(TextureName, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(TextureName, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
    glTextureStorage3D(TextureName, static_cast<GLsizei>(Levels), GL_RGBA8, GLsizei(Size), GLsizei(Size), 1);

    struct ivec3 { int32_t x,y,z; };
    struct u8vec4 { uint8_t x,y,z,w; };
    ivec3 PageSize;
    glGetInternalformativ(GL_TEXTURE_2D_ARRAY, GL_RGBA8, GL_VIRTUAL_PAGE_SIZE_X_ARB, 1, &PageSize.x);
    glGetInternalformativ(GL_TEXTURE_2D_ARRAY, GL_RGBA8, GL_VIRTUAL_PAGE_SIZE_Y_ARB, 1, &PageSize.y);
    glGetInternalformativ(GL_TEXTURE_2D_ARRAY, GL_RGBA8, GL_VIRTUAL_PAGE_SIZE_Z_ARB, 1, &PageSize.z);

    std::vector<u8vec4> Page;
    Page.resize(static_cast<std::size_t>(PageSize.x * PageSize.y * PageSize.z));

    GLint Page3DSizeX(0);
    GLint Page3DSizeY(0);
    GLint Page3DSizeZ(0);
    glGetInternalformativ(GL_TEXTURE_3D, GL_RGBA32F, GL_VIRTUAL_PAGE_SIZE_X_ARB, 1, &Page3DSizeX);
    glGetInternalformativ(GL_TEXTURE_3D, GL_RGBA32F, GL_VIRTUAL_PAGE_SIZE_Y_ARB, 1, &Page3DSizeY);
    glGetInternalformativ(GL_TEXTURE_3D, GL_RGBA32F, GL_VIRTUAL_PAGE_SIZE_Z_ARB, 1, &Page3DSizeZ);

    const int64_t timerFreq = getHPFrequency();
    const double toMs = 1000.0/double(timerFreq);

    const int64_t begTot = getHPCounter();

    size_t commitCount = 0;

    for (std::size_t Level = 0; Level < MaxLevels; ++Level)
    {
        GLsizei LevelSize = (Size >> Level);
        GLsizei TileCountY = LevelSize / PageSize.y;
        GLsizei TileCountX = LevelSize / PageSize.x;

        for(GLsizei j = 0; j < TileCountY; ++j)
        for(GLsizei i = 0; i < TileCountX; ++i)
        {
            u8vec4 pix;
            pix.x = static_cast<unsigned char>(float(i) / float(LevelSize / PageSize.x) * 255);
            pix.y = static_cast<unsigned char>(float(j) / float(LevelSize / PageSize.y) * 255);
            pix.z = static_cast<unsigned char>(float(Level) / float(MaxLevels) * 255);
            pix.w = 255;

            std::fill(Page.begin(), Page.end(), pix);

            const int64_t beg0 = getHPCounter();
            glTexturePageCommitmentEXT(TextureName, static_cast<GLint>(Level),
                static_cast<GLsizei>(PageSize.x) * i, static_cast<GLsizei>(PageSize.y) * j, 0,
                static_cast<GLsizei>(PageSize.x), static_cast<GLsizei>(PageSize.y), 1,
                GL_TRUE);
            commitCount++;
            const int64_t end0 = getHPCounter();
            fprintf(stderr, "TIME COMMIT %fms | %u %u | %zu\n", (end0-beg0)*toMs, PageSize.x, PageSize.y, Level);

            //const int64_t beg1 = getHPCounter();
            glTextureSubImage3D(TextureName, static_cast<GLint>(Level),
                static_cast<GLsizei>(PageSize.x) * i, static_cast<GLsizei>(PageSize.y) * j, 0,
                static_cast<GLsizei>(PageSize.x), static_cast<GLsizei>(PageSize.y), 1,
                GL_RGBA, GL_UNSIGNED_BYTE,
                &Page[0].x);
            //const int64_t end1 = getHPCounter();
            //fprintf(stderr, "TIME UPDATE %fms | %u %u | %zu\n", (end1-beg1)*toMs, PageSize.x, PageSize.y, Level);
        }
    }

    const int64_t commitEnd = getHPCounter();

    size_t evictCount = 0;
    for (std::size_t Level = 0; Level < MaxLevels; ++Level)
    {
        GLsizei LevelSize = (Size >> Level);
        GLsizei TileCountY = LevelSize / PageSize.y;
        GLsizei TileCountX = LevelSize / PageSize.x;

        for(GLsizei j = 0; j < TileCountY; ++j)
        for(GLsizei i = 0; i < TileCountX; ++i)
        {
            const int64_t beg0 = getHPCounter();
            glTexturePageCommitmentEXT(TextureName, static_cast<GLint>(Level),
                static_cast<GLsizei>(PageSize.x) * i, static_cast<GLsizei>(PageSize.y) * j, 0,
                static_cast<GLsizei>(PageSize.x), static_cast<GLsizei>(PageSize.y), 1,
                GL_FALSE);
            evictCount++;
            const int64_t end0 = getHPCounter();
            fprintf(stderr, "TIME EVICT %fms | %u %u | %zu\n", (end0-beg0)*toMs, PageSize.x, PageSize.y, Level);
        }
    }
    const int64_t endTot = getHPCounter();

    fprintf(stderr, "TOTAL COMMITS: %zd\n", commitCount);
    fprintf(stderr, "TOTAL EVICTS: %zd\n", evictCount);
    fprintf(stderr, "TOTAL COMMIT TIME: %fms\n", (commitEnd-begTot)*toMs);
    fprintf(stderr, "TOTAL EVICT TIME: %fms\n", (endTot-commitEnd)*toMs);
    fprintf(stderr, "TOTAL TIME (commit + evict): %fms\n", (endTot-begTot)*toMs);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

int32_t main(int32_t argc, char **argv)
{
    doSparseTest();
    return 0;
}

/* vim: set sw=4 ts=4 expandtab: */
