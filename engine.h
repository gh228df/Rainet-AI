#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <zstd.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <GLFW/glfw3.h>

typedef struct
{
    uint32_t mask1;
    uint32_t mask2;
    uint32_t mask_comb;
    uint16_t shift1;
    uint16_t shift2;
} dTblEntry;

__attribute__((aligned(64))) static const dTblEntry decode_table[16] = {
    {0x00000000, 0x00000000, 0xFFFFFFFF, 0, 0},  // - - - - > - - - - 0
    {0x000000FF, 0x00000000, 0xFFFFFF00, 0, 0},  // r - - - > r - - - 1
    {0x0000FF00, 0x00000000, 0xFFFF00FF, 8, 0},  // g - - - > - g - - 2
    {0x0000FFFF, 0x00000000, 0xFFFF0000, 0, 0},  // r g - - > r g - - 3
    {0x00FF0000, 0x00000000, 0xFF00FFFF, 16, 0}, // b - - - > - - b - 4
    {0x000000FF, 0x00FF0000, 0xFF00FF00, 0, 8},  // r b - - > r - b - 5
    {0x00FFFF00, 0x00000000, 0xFF0000FF, 8, 0},  // g b - - > - g b - 6
    {0x00FFFFFF, 0x00000000, 0xFF000000, 0, 0},  // r g b - > r g b - 7
    {0xFF000000, 0x00000000, 0x00FFFFFF, 24, 0}, // a - - - > - - - a 8
    {0x000000FF, 0xFF000000, 0x00FFFF00, 0, 16}, // r a - - > r - - a 9
    {0x0000FF00, 0xFF000000, 0x00FF00FF, 8, 16}, // g a - - > - g - a 10
    {0x0000FFFF, 0xFF000000, 0x00FF0000, 0, 8},  // r g a - > r g - a 11
    {0xFFFF0000, 0x00000000, 0x0000FFFF, 16, 0}, // b a - - > - - b a 12
    {0x000000FF, 0xFFFF0000, 0x0000FF00, 0, 8},  // r b a - > r - b a 13
    {0xFFFFFF00, 0x00000000, 0x000000FF, 8, 0},  // g b a - > - g b a 14
    {0xFFFFFFFF, 0x00000000, 0x00000000, 0, 0}   // r g b a > r g b a 15
};

static inline void decode(uint32_t *out_pixel_data, uint8_t *encoded_image, int width)
{
    uint32_t cur_pointer_color = 0;
    uint32_t last_pointer_color = 0;

    while (true)
    {
        int instr = *encoded_image;
        if ((instr & 7) == 1)
        {
            if (__builtin_expect(instr & 8, 0))
            {
                return;
            }

            last_pointer_color = cur_pointer_color;
            int mask = (*(encoded_image++)) >> 4;
            cur_pointer_color =
                (cur_pointer_color & decode_table[mask].mask_comb) |
                ((((*(uint32_t *)encoded_image) << decode_table[mask].shift1) & decode_table[mask].mask1) |
                 (((*(uint32_t *)encoded_image) << decode_table[mask].shift2) & decode_table[mask].mask2));
            encoded_image += __builtin_popcount(mask);
            *(out_pixel_data++) = cur_pointer_color;
        }
        else if (instr & 4)
        {
            last_pointer_color = cur_pointer_color;
            cur_pointer_color = *(out_pixel_data - width + ((*encoded_image) & 7) - 4);
            uint32_t temp = (*(encoded_image++) >> 3) + 1;
            for (; temp; --temp)
                *(out_pixel_data++) = cur_pointer_color;
        }
        else if ((instr & 7) == 0)
        {
            uint32_t temp = (*(encoded_image++) >> 3) + 1;
            for (; temp; --temp)
                *(out_pixel_data++) = cur_pointer_color;
        }
        else if ((instr & 7) == 3)
        {
            uint32_t temp = cur_pointer_color;
            uint32_t sub_7 = (cur_pointer_color | 0x80808080U) - (last_pointer_color & ~0x80808080U);
            last_pointer_color ^= sub_7;
            cur_pointer_color = (((sub_7 & ~0x80808080U) + (cur_pointer_color & ~0x80808080U)) ^ 0x80808080U) ^ (last_pointer_color & 0x80808080U);
            last_pointer_color = temp;

            temp = (*(encoded_image++) >> 3) + 1;
            for (; temp; --temp)
                *(out_pixel_data++) = cur_pointer_color;
        }
        else
        {
            uint32_t repeat_var = (*(uint32_t *)encoded_image) >> 2;
            uint32_t move_coef = ((instr >> 2) & 2); // either 2 or 0
            uint32_t mask = (move_coef != 0) ? 1073741823 : 16383;

            encoded_image += (2 + move_coef);

            uint32_t temp = (repeat_var & mask) - move_coef + 4;

            //__builtin_assume(temp > 32);

            while (temp > 32)
            {
                *(out_pixel_data + 0) = cur_pointer_color;
                *(out_pixel_data + 1) = cur_pointer_color;
                *(out_pixel_data + 2) = cur_pointer_color;
                *(out_pixel_data + 3) = cur_pointer_color;
                *(out_pixel_data + 4) = cur_pointer_color;
                *(out_pixel_data + 5) = cur_pointer_color;
                *(out_pixel_data + 6) = cur_pointer_color;
                *(out_pixel_data + 7) = cur_pointer_color;

                out_pixel_data += 8;
                temp -= 32;
            }

            *(out_pixel_data + 0) = cur_pointer_color;
            *(out_pixel_data + 1) = cur_pointer_color;
            *(out_pixel_data + 2) = cur_pointer_color;
            *(out_pixel_data + 3) = cur_pointer_color;
            *(out_pixel_data + 4) = cur_pointer_color;
            *(out_pixel_data + 5) = cur_pointer_color;
            *(out_pixel_data + 6) = cur_pointer_color;
            *(out_pixel_data + 7) = cur_pointer_color;

            out_pixel_data = (uint32_t *)((uintptr_t)out_pixel_data + temp);
        }
    }
}

#ifdef __linux__
#include <GL/glx.h>
#endif

#ifdef __linux__
#include <sys/mman.h>
#else
#include <windows.h>

static PFNGLTEXIMAGE3DPROC glTexImage3D;
static PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D;
static PFNGLACTIVETEXTUREPROC glActiveTexture;

#endif

#define perform_texture_alpha_operation_single(current_time, time_begin_first, duration_first, target_first, is_fading_in_first) ((is_fading_in_first) ? (((current_time) - (time_begin_first) >= duration_first) ? (target_first) : ((((current_time) - (time_begin_first)) * (target_first)) / (duration_first))) : (((current_time) - (time_begin_first) > (duration_first)) ? 0 : ((((time_begin_first) + (duration_first) - (current_time)) * (target_first)) / (duration_first))))

static PFNGLCREATESHADERPROC glCreateShader;
static PFNGLSHADERSOURCEPROC glShaderSource;
static PFNGLCOMPILESHADERPROC glCompileShader;
static PFNGLCREATEPROGRAMPROC glCreateProgram;
static PFNGLLINKPROGRAMPROC glLinkProgram;
static PFNGLATTACHSHADERPROC glAttachShader;
static PFNGLUSEPROGRAMPROC glUseProgram;
static PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
static PFNGLGENBUFFERSPROC glGenBuffers;
static PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
static PFNGLBINDBUFFERPROC glBindBuffer;
static PFNGLBUFFERDATAPROC glBufferData;
static PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
static PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer;
static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
static PFNGLVERTEXATTRIBDIVISORPROC glVertexAttribDivisor;
static PFNGLDELETESHADERPROC glDeleteShader;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
static PFNGLUNIFORM2FPROC glUniform2f;
static PFNGLUNIFORM1IPROC glUniform1i;
static PFNGLUNIFORM1FPROC glUniform1f;
static PFNGLUNIFORM4FPROC glUniform4f;
static PFNGLBUFFERSUBDATAPROC glBufferSubData;
static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;

static unsigned int VAO, VBO, EBO;
static unsigned int shader_program;

static GLuint fbo = 0;
static GLuint fbo_texture = 0;
static GLuint screen_VAO = 0;
static GLuint screen_VBO = 0;
static GLuint screen_shader_program = 0;

static const char *vertex_shader_source =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord;\n"
    "layout (location = 2) in uint aColorPacked;\n"
    "layout (location = 3) in int aTexIndex;\n"
    "layout (location = 4) in uint aFlags;\n"
    "\n"
    "out vec2 TexCoord;\n"
    "out vec4 Color;\n"
    "flat out int TexIndex;\n"
    "\n"
    "void main() {\n"
    "    gl_Position = vec4(vec2(aPos.x * 2.0 - 1.0, 1.0 - aPos.y * 2.0), aPos.z, 1.0);\n"
    "\n"
    "    TexCoord = aTexCoord;\n"
    "    TexIndex = aTexIndex;\n"
    "\n"
    "    Color = vec4(float(aColorPacked & 0xFFu), float((aColorPacked >> 8u) & 0xFFu), float((aColorPacked >> 16u) & 0xFFu), float(aColorPacked >> 24u)) / 255.0;\n"
    "}";

static const char *fragment_shader_source =
    "#version 330 core\n"
    "uniform sampler2DArray u_textureArray;\n"
    "\n"
    "in vec2 TexCoord;\n"
    "in vec4 Color;\n"
    "flat in int TexIndex;\n"
    "\n"
    "out vec4 FragColor;\n"
    "\n"
    "void main() {\n"
    "    FragColor = texture(u_textureArray, vec3(TexCoord, TexIndex)) * Color;\n"
    "}";

const char *screen_vertex_shader =
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec2 aTexCoords;\n"
    "out vec2 TexCoords;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);\n"
    "    TexCoords = aTexCoords;\n"
    "}\n";

const char *screen_fragment_shader =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in vec2 TexCoords;\n"
    "uniform sampler2D screenTexture;\n"
    "void main()\n"
    "{\n"
    "    FragColor = texture(screenTexture, TexCoords);\n"
    "}\n";

#define ROTATE_0 0
#define ROTATE_90 1
#define ROTATE_180 2
#define ROTATE_270 3
#define FLIP_H 4
#define FLIP_V 8

#define BATCH_BUFFER_SIZE 2048

typedef struct
{
    float pos[3];
    float texCoord[2];
    uint32_t color;
    int32_t texIndex;
    uint32_t aFlags;
} vertex_t;

static vertex_t vertices[4 * BATCH_BUFFER_SIZE]; // 4 vertices per quad
static uint32_t awaiting_in_batch;
static uint32_t array_texture_handle;

typedef struct
{
    uint16_t atlas_x;
    uint16_t atlas_y;
    uint8_t width;
    uint8_t height;
    uint8_t atlas_id;
    uint8_t pad[1];
} font_glyph_t;

typedef struct
{
    uint16_t atlas_x;
    uint16_t atlas_y;
    uint16_t width;
    uint16_t height;
    uint8_t atlas_id;
    uint8_t pad[7];
} texture_t;

typedef struct
{
    font_glyph_t character_map[256];
    float font_size;
    float advance_value;
} font_t;

typedef struct button_t
{
    int64_t interation_time;

    void (*on_click)(struct button_t *self); // when clicked
    void (*render)(struct button_t *self);   // render function

    union
    {
        struct
        {
            font_t *font_ptr;
            char *text_ptr;
            float font_size;
            uint8_t r, g, b, a;
        } button_no_bg_text;

        struct
        {
            texture_t *texture_ptr;
            font_t *font_ptr;
            char *text_ptr;
            float font_size;
            uint8_t r, g, b, a;
        } button_solid_text;

        struct
        {
            texture_t *corner;
            texture_t *side;
            texture_t *center;
            font_t *font_ptr;
            char *text_ptr;
            float font_size;
            uint8_t r, g, b, a;
        } button_3_slice_text;

        struct
        {
            texture_t *top_left;
            texture_t *top;
            texture_t *top_right;
            texture_t *left;
            texture_t *center;
            texture_t *right;
            texture_t *bottom_left;
            texture_t *bottom;
            texture_t *bottom_right;
            font_t *font_ptr;
            char *text_ptr;
            float font_size;
            uint8_t r, g, b, a;
        } button_9_slice_text;

        struct
        {
            texture_t *texture_ptr;
            uint8_t r, g, b, a;
        } button_image;

        struct
        {
            texture_t *corner;
            texture_t *side;
            texture_t *center;
            texture_t *texture_ptr;
            uint8_t r, g, b, a;
        } button_3_slice_image;

        struct
        {
            texture_t *top_left;
            texture_t *top;
            texture_t *top_right;
            texture_t *left;
            texture_t *center;
            texture_t *right;
            texture_t *bottom_left;
            texture_t *bottom;
            texture_t *bottom_right;
            texture_t *texture_ptr;
            uint8_t r, g, b, a;
        } button_9_slice_image;
    };

    int padding_top, padding_bottom, padding_left, padding_right;

    int x, y, width, height;
    int fade_time;

    uint8_t r, g, b, a;
    bool visible;
    bool is_interacting;
} button_t;

static button_t *button_buf = NULL;
static size_t button_buf_size = 0;
static size_t button_buf_capacity = 0;

#ifndef TEXTURE_ARRAY_DIM
#error "TEXTURE_ARRAY_DIM has to be defined, don't include this file directly"
#endif

#ifndef TEXTURE_ARRAY_SIZE
#error "TEXTURE_ARRAY_SIZE has to be defined, don't include this file directly"
#endif

extern GLFWwindow *window;
extern const char *texture_array_compressed_data;

int view_w, view_h, view_x, view_y;

static void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    float base_aspect = (float)BASE_W / BASE_H;
    float window_aspect = (float)width / height;

    if (window_aspect > base_aspect)
    {
        // Window is wider - letterbox sides
        view_h = height;
        view_w = (int)(height * base_aspect);
        view_x = (width - view_w) / 2;
        view_y = 0;
    }
    else
    {
        // Window is taller - letterbox top/bottom
        view_w = width;
        view_h = (int)(width / base_aspect);
        view_x = 0;
        view_y = (height - view_h) / 2;
    }
}

static int windowed_xpos, windowed_ypos, windowed_width, windowed_height;
static bool is_fullscreen = false;

// Key callback function
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_F11 && action == GLFW_PRESS)
    {
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *mode = glfwGetVideoMode(monitor);

        if (is_fullscreen)
        {
            glfwSetWindowMonitor(window, NULL, windowed_xpos, windowed_ypos, windowed_width, windowed_height, GLFW_DONT_CARE);
        }
        else
        {
            glfwGetWindowPos(window, &windowed_xpos, &windowed_ypos);
            glfwGetWindowSize(window, &windowed_width, &windowed_height);

            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }

        is_fullscreen = !is_fullscreen;
    }
}

bool end_button_scan_on_hit = true;

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
        return;

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    // Convert cursor position from window coordinates to virtual coordinates
    // First, translate to viewport-relative coordinates
    int cursor_x = (int)xpos - view_x;
    int cursor_y = (int)ypos - view_y;

    // Check if click is outside the viewport
    if (cursor_x < 0 || cursor_x >= view_w || cursor_y < 0 || cursor_y >= view_h)
        return;

    // Scale from viewport coordinates to virtual BASE_W x BASE_H coordinates
    int virtual_x = (cursor_x * BASE_W) / view_w;
    int virtual_y = (cursor_y * BASE_H) / view_h;

    // Iterate through all buttons
    for (size_t i = 0; i < button_buf_size; i++)
    {
        button_t *btn = &button_buf[i];

        // Skip if button is NULL or not visible
        if (btn == NULL || !btn->visible)
            continue;

        // Calculate actual clickable bounds including padding
        int click_x = btn->x;
        int click_y = btn->y;
        int click_width = btn->width + btn->padding_left + btn->padding_right;
        int click_height = btn->height + btn->padding_top + btn->padding_bottom;

        // Check if click is within button bounds (including padding)
        if (virtual_x >= click_x && virtual_x <= click_x + click_width &&
            virtual_y >= click_y && virtual_y <= click_y + click_height)
        {
            // Call on_click if it exists
            if (btn->on_click != NULL)
            {
                btn->on_click(btn);
                if (end_button_scan_on_hit)
                    return;
            }
        }
    }
}

static int64_t current_time = 0;

static void cursor_position_callback(GLFWwindow *window, double xpos, double ypos)
{
    // Convert cursor position from window coordinates to virtual coordinates
    // First, translate to viewport-relative coordinates
    int cursor_x = (int)xpos - view_x;
    int cursor_y = (int)ypos - view_y;

    // Check if click is outside the viewport
    // if (cursor_x < 0 || cursor_x >= view_w || cursor_y < 0 || cursor_y >= view_h)
    //     return;

    // Scale from viewport coordinates to virtual BASE_W x BASE_H coordinates
    int virtual_x = (cursor_x * BASE_W) / view_w;
    int virtual_y = (cursor_y * BASE_H) / view_h;

    // Iterate through all buttons
    for (size_t i = 0; i < button_buf_size; i++)
    {
        button_t *btn = &button_buf[i];

        // Skip if button is NULL or not visible
        if (btn == NULL)
            continue;

        // Calculate actual clickable bounds including padding
        int click_x = btn->x;
        int click_y = btn->y;
        int click_width = btn->width + btn->padding_left + btn->padding_right;
        int click_height = btn->height + btn->padding_top + btn->padding_bottom;

        // Check if click is within button bounds (including padding)
        if (virtual_x >= click_x && virtual_x <= click_x + click_width &&
            virtual_y >= click_y && virtual_y <= click_y + click_height)
        {
            if (btn->is_interacting == false)
            {
                btn->is_interacting = true;
                btn->interation_time = current_time - ((current_time - btn->interation_time < btn->fade_time) ? (btn->fade_time - (current_time - btn->interation_time)) : 0);
            }
        }
        else if (btn->is_interacting)
        {
            btn->is_interacting = false;
            btn->interation_time = current_time - ((current_time - btn->interation_time < btn->fade_time) ? (btn->fade_time - (current_time - btn->interation_time)) : 0);
        }
    }
}

#define ROUND_UP_TO_MEMORY_PAGE(x) (((x) + 4095) & (~4095))

void init(const char *win_name)
{
#ifdef __linux__
    button_buf = (button_t *)mmap(NULL, (1ULL << 32), PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (button_buf == MAP_FAILED)
    {
        fprintf(stderr, "OOM\n");
        exit(1);
    }
#else
    button_buf = (button_t *)VirtualAlloc(NULL, (1ULL << 32), MEM_RESERVE, PAGE_NOACCESS);

    if (button_buf == NULL)
    {
        fprintf(stderr, "OOM\n");
        exit(1);
    }
#endif

#ifdef __linux__
    if (mprotect(button_buf, ROUND_UP_TO_MEMORY_PAGE(4096 * sizeof(button_t)), PROT_READ | PROT_WRITE) != 0)
#else
    if (VirtualAlloc(button_buf, ROUND_UP_TO_MEMORY_PAGE(4096 * sizeof(button_t)), MEM_COMMIT, PAGE_READWRITE) == NULL)
#endif
    {
        fprintf(stderr, "OOM\n");
        exit(1);
    }

    button_buf_capacity = 4096;

    if (glfwInit() == GLFW_FALSE)
    {
        fprintf(stderr, "glfw init failed\n");
        exit(1);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    window = glfwCreateWindow(BASE_W, BASE_H, win_name, NULL, NULL);

    if (window == NULL)
    {
        fprintf(stderr, "glfw window init failed\n");
        exit(1);
    }

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    glfwGetFramebufferSize(window, &view_w, &view_h);

    // Set viewport to the actual framebuffer size
    glViewport(0, 0, view_w, view_h);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glfwMakeContextCurrent(window);

#ifdef __linux__
    glCreateShader = (PFNGLCREATESHADERPROC)glXGetProcAddress((const GLubyte *)"glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)glXGetProcAddress((const GLubyte *)"glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)glXGetProcAddress((const GLubyte *)"glCompileShader");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)glXGetProcAddress((const GLubyte *)"glCreateProgram");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)glXGetProcAddress((const GLubyte *)"glLinkProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)glXGetProcAddress((const GLubyte *)"glAttachShader");
    glUseProgram = (PFNGLUSEPROGRAMPROC)glXGetProcAddress((const GLubyte *)"glUseProgram");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)glXGetProcAddress((const GLubyte *)"glGenVertexArrays");
    glGenBuffers = (PFNGLGENBUFFERSPROC)glXGetProcAddress((const GLubyte *)"glGenBuffers");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)glXGetProcAddress((const GLubyte *)"glBindVertexArray");
    glBindBuffer = (PFNGLBINDBUFFERPROC)glXGetProcAddress((const GLubyte *)"glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)glXGetProcAddress((const GLubyte *)"glBufferData");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)glXGetProcAddress((const GLubyte *)"glVertexAttribPointer");
    glVertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC)glXGetProcAddress((const GLubyte *)"glVertexAttribIPointer");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)glXGetProcAddress((const GLubyte *)"glEnableVertexAttribArray");
    glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC)glXGetProcAddress((const GLubyte *)"glVertexAttribDivisor");
    glDeleteShader = (PFNGLDELETESHADERPROC)glXGetProcAddress((const GLubyte *)"glDeleteShader");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)glXGetProcAddress((const GLubyte *)"glGetUniformLocation");
    glUniform2f = (PFNGLUNIFORM2FPROC)glXGetProcAddress((const GLubyte *)"glUniform2f");
    glUniform1i = (PFNGLUNIFORM1IPROC)glXGetProcAddress((const GLubyte *)"glUniform1i");
    glUniform1f = (PFNGLUNIFORM1FPROC)glXGetProcAddress((const GLubyte *)"glUniform1f");
    glUniform4f = (PFNGLUNIFORM4FPROC)glXGetProcAddress((const GLubyte *)"glUniform4f");
    glBufferSubData = (PFNGLBUFFERSUBDATAPROC)glXGetProcAddress((const GLubyte *)"glBufferSubData");
    glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)glXGetProcAddress((const GLubyte *)"glGenFramebuffers");
    glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)glXGetProcAddress((const GLubyte *)"glBindFramebuffer");
    glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glXGetProcAddress((const GLubyte *)"glFramebufferTexture2D");

    if (glCreateShader == NULL || glShaderSource == NULL || glCompileShader == NULL || glCreateProgram == NULL || glLinkProgram == NULL || glAttachShader == NULL || glUseProgram == NULL || glGenVertexArrays == NULL || glGenBuffers == NULL || glBindVertexArray == NULL || glBindBuffer == NULL || glBufferData == NULL || glVertexAttribPointer == NULL || glVertexAttribIPointer == NULL || glEnableVertexAttribArray == NULL || glDeleteShader == NULL || glGetUniformLocation == NULL || glUniform2f == NULL || glUniform1i == NULL || glUniform1f == NULL || glUniform4f == NULL || glBufferSubData == NULL || glGenFramebuffers == NULL || glBindFramebuffer == NULL || glFramebufferTexture2D == NULL)
    {
        fprintf(stderr, "opengl 3.3 functions init\n");
        exit(1);
    }
#else
    glCreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
    glUseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)wglGetProcAddress("glGenVertexArrays");
    glGenBuffers = (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)wglGetProcAddress("glBindVertexArray");
    glBindBuffer = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)wglGetProcAddress("glVertexAttribPointer");
    glVertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC)wglGetProcAddress("glVertexAttribIPointer");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)wglGetProcAddress("glEnableVertexAttribArray");
    glDeleteShader = (PFNGLDELETESHADERPROC)wglGetProcAddress("glDeleteShader");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
    glUniform2f = (PFNGLUNIFORM2FPROC)wglGetProcAddress("glUniform2f");
    glUniform1i = (PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i");
    glUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
    glUniform4f = (PFNGLUNIFORM4FPROC)wglGetProcAddress("glUniform4f");
    glBufferSubData = (PFNGLBUFFERSUBDATAPROC)wglGetProcAddress("glBufferSubData");
    glTexImage3D = (PFNGLTEXIMAGE3DPROC)wglGetProcAddress("glTexImage3D");
    glTexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC)wglGetProcAddress("glTexSubImage3D");
    glActiveTexture = (PFNGLACTIVETEXTUREPROC)wglGetProcAddress("glActiveTexture");
    glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)wglGetProcAddress("glGenFramebuffers");
    glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)wglGetProcAddress("glBindFramebuffer");
    glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)wglGetProcAddress("glFramebufferTexture2D");

    if (glCreateShader == NULL || glShaderSource == NULL || glCompileShader == NULL || glCreateProgram == NULL || glLinkProgram == NULL || glAttachShader == NULL || glUseProgram == NULL || glGenVertexArrays == NULL || glGenBuffers == NULL || glBindVertexArray == NULL || glBindBuffer == NULL || glBufferData == NULL || glVertexAttribPointer == NULL || glVertexAttribIPointer == NULL || glEnableVertexAttribArray == NULL || glDeleteShader == NULL || glGetUniformLocation == NULL || glUniform2f == NULL || glUniform1i == NULL || glUniform1f == NULL || glUniform4f == NULL || glBufferSubData == NULL || glTexImage3D == NULL || glTexSubImage3D == NULL || glActiveTexture == NULL || glGenFramebuffers == NULL || glBindFramebuffer == NULL || glFramebufferTexture2D == NULL)
    {
        fprintf(stderr, "opengl 3.3 functions init\n");
        exit(1);
    }
#endif

    // Compile vertex shader
    unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);

    // Compile fragment shader
    unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);

    // Create and link program
    shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    // Clean up shaders (they're linked into the program now)
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Setup vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), NULL, GL_DYNAMIC_DRAW);

    // Pre-generate indices (static data)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    int *text_indices = (int *)malloc(BATCH_BUFFER_SIZE * 6 * sizeof(int));
    if (text_indices == NULL)
    {
        fprintf(stderr, "OOM\n");
        exit(1);
    }

    for (int i = 0; i < BATCH_BUFFER_SIZE; ++i)
    {
        int base_vertex = i * 4;
        int base_index = i * 6;

        text_indices[base_index + 0] = base_vertex + 0; // top-left
        text_indices[base_index + 1] = base_vertex + 1; // bottom-left
        text_indices[base_index + 2] = base_vertex + 2; // bottom-right
        text_indices[base_index + 3] = base_vertex + 0; // top-left
        text_indices[base_index + 4] = base_vertex + 2; // bottom-right
        text_indices[base_index + 5] = base_vertex + 3; // top-right
    }
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, BATCH_BUFFER_SIZE * 6 * sizeof(int), text_indices, GL_STATIC_DRAW);
    free(text_indices);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (void *)offsetof(vertex_t, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (void *)offsetof(vertex_t, texCoord));
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(vertex_t), (void *)offsetof(vertex_t, color));
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(3, 1, GL_INT, sizeof(vertex_t), (void *)offsetof(vertex_t, texIndex));
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, sizeof(vertex_t), (void *)offsetof(vertex_t, aFlags));
    glEnableVertexAttribArray(4);

    glGenTextures(1, &array_texture_handle);
    glBindTexture(GL_TEXTURE_2D_ARRAY, array_texture_handle);

    // Allocate storage for all layers at once
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, TEXTURE_ARRAY_DIM, TEXTURE_ARRAY_DIM, TEXTURE_ARRAY_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glUseProgram(shader_program);
    glUniform1i(glGetUniformLocation(shader_program, "u_textureArray"), 0); // Texture unit 0

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, array_texture_handle);

    // Create framebuffer
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create texture for color attachment
    glGenTextures(1, &fbo_texture);
    glBindTexture(GL_TEXTURE_2D, fbo_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, BASE_W, BASE_H, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // Use NEAREST for pixel-perfect
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_texture, 0);

    // Create fullscreen quad for rendering the texture to screen
    float quad_vertices[] = {
        // positions   // texCoords
        -1.0f, 1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
        1.0f, -1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f};

    glGenVertexArrays(1, &screen_VAO);
    glGenBuffers(1, &screen_VBO);
    glBindVertexArray(screen_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, screen_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &screen_vertex_shader, NULL);
    glCompileShader(vertex_shader);

    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &screen_fragment_shader, NULL);
    glCompileShader(fragment_shader);

    screen_shader_program = glCreateProgram();
    glAttachShader(screen_shader_program, vertex_shader);
    glAttachShader(screen_shader_program, fragment_shader);
    glLinkProgram(screen_shader_program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    uint8_t *compressed_ptr = (uint8_t *)texture_array_compressed_data;
    uint32_t *layer_data = (uint32_t *)malloc(TEXTURE_ARRAY_DIM * TEXTURE_ARRAY_DIM * 4 + 256);

    for (int i = 0; i < TEXTURE_ARRAY_SIZE; ++i)
    {
        size_t compressed_size = *(size_t *)compressed_ptr;
        compressed_ptr += 8;

        unsigned long long raw_size = ZSTD_getFrameContentSize(compressed_ptr, compressed_size);
        char *decompressed = (char *)malloc(raw_size);
        if (decompressed == NULL)
        {
            fprintf(stderr, "OOM\n");
            exit(1);
        }

        size_t decompressed_size = ZSTD_decompress(decompressed, raw_size, compressed_ptr, compressed_size);

        decode(layer_data, (uint8_t *)decompressed, TEXTURE_ARRAY_DIM);

        free(decompressed);

        compressed_ptr += compressed_size;

        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, TEXTURE_ARRAY_DIM, TEXTURE_ARRAY_DIM, 1, GL_RGBA, GL_UNSIGNED_BYTE, layer_data);
    }

    free(layer_data);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

inline static void render_batch()
{
    if (awaiting_in_batch == 0)
        return;

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    glUseProgram(shader_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, array_texture_handle);

    glBufferSubData(GL_ARRAY_BUFFER, 0, awaiting_in_batch * 4 * sizeof(vertex_t), vertices);
    glDrawElements(GL_TRIANGLES, awaiting_in_batch * 6, GL_UNSIGNED_INT, 0);
    awaiting_in_batch = 0;
}

static void inline render_atlas_section_normalized(uint32_t atlas_id, float u1, float u2, float v1, float v2, float x_out, float y_out, float w_out, float h_out, uint32_t color_packed, uint32_t flags)
{
    if (awaiting_in_batch == BATCH_BUFFER_SIZE)
    {
        render_batch();
    }

    // Apply flipping
    if (flags & FLIP_H)
    {
        float temp = u1;
        u1 = u2;
        u2 = temp;
    }
    if (flags & FLIP_V)
    {
        float temp = v1;
        v1 = v2;
        v2 = temp;
    }

    // Base texture coordinates for each vertex (before rotation)
    // [0] = top-left, [1] = bottom-left, [2] = bottom-right, [3] = top-right
    float u[4] = {u1, u1, u2, u2};
    float v[4] = {v1, v2, v2, v1};

    // Apply rotation by rotating the vertex assignment
    uint32_t rotation = flags & 3u;
    if (rotation != ROTATE_0)
    {
        float u_temp[4], v_temp[4];
        for (int i = 0; i < 4; i++)
        {
            u_temp[i] = u[i];
            v_temp[i] = v[i];
        }

        // Rotate texture coordinate assignment
        // rotation = 1 (90° CW):  each vertex gets coords from previous vertex
        // rotation = 2 (180°):   each vertex gets coords from opposite vertex
        // rotation = 3 (270° CW): each vertex gets coords from next vertex
        for (int i = 0; i < 4; i++)
        {
            int src_index = (i + rotation) % 4;
            u[i] = u_temp[src_index];
            v[i] = v_temp[src_index];
        }
    }

    int offset = (awaiting_in_batch++) * 4;

    // Top-left vertex
    vertices[offset + 0].pos[0] = x_out / (float)BASE_W;
    vertices[offset + 0].pos[1] = y_out / (float)BASE_H;
    vertices[offset + 0].pos[2] = 0;
    vertices[offset + 0].texCoord[0] = u[0];
    vertices[offset + 0].texCoord[1] = v[0];
    vertices[offset + 0].color = color_packed;
    vertices[offset + 0].texIndex = atlas_id;

    // Bottom-left vertex
    vertices[offset + 1].pos[0] = x_out / (float)BASE_W;
    vertices[offset + 1].pos[1] = (y_out + h_out) / (float)BASE_H;
    vertices[offset + 1].pos[2] = 0;
    vertices[offset + 1].texCoord[0] = u[1];
    vertices[offset + 1].texCoord[1] = v[1];
    vertices[offset + 1].color = color_packed;
    vertices[offset + 1].texIndex = atlas_id;

    // Bottom-right vertex
    vertices[offset + 2].pos[0] = (x_out + w_out) / (float)BASE_W;
    vertices[offset + 2].pos[1] = (y_out + h_out) / (float)BASE_H;
    vertices[offset + 2].pos[2] = 0;
    vertices[offset + 2].texCoord[0] = u[2];
    vertices[offset + 2].texCoord[1] = v[2];
    vertices[offset + 2].color = color_packed;
    vertices[offset + 2].texIndex = atlas_id;

    // Top-right vertex
    vertices[offset + 3].pos[0] = (x_out + w_out) / (float)BASE_W;
    vertices[offset + 3].pos[1] = y_out / (float)BASE_H;
    vertices[offset + 3].pos[2] = 0;
    vertices[offset + 3].texCoord[0] = u[3];
    vertices[offset + 3].texCoord[1] = v[3];
    vertices[offset + 3].color = color_packed;
    vertices[offset + 3].texIndex = atlas_id;
}

static void render_atlas_section(uint32_t atlas_id, float x_atlas, float y_atlas, float w_atlas, float h_atlas, float x_out, float y_out, float w_out, float h_out, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint32_t flags)
{
    render_atlas_section_normalized(atlas_id, x_atlas / (float)TEXTURE_ARRAY_DIM, (x_atlas + w_atlas) / (float)TEXTURE_ARRAY_DIM, y_atlas / (float)TEXTURE_ARRAY_DIM, (y_atlas + h_atlas) / (float)TEXTURE_ARRAY_DIM, x_out, y_out, w_out, h_out, ((uint32_t)r << 0) | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24), flags);
}

static void render_tex_custom_scale(const texture_t *tex, float x_out, float y_out, float w_out, float h_out, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint32_t flags)
{
    render_atlas_section_normalized((uint32_t)tex->atlas_id, (float)tex->atlas_x / (float)TEXTURE_ARRAY_DIM, (float)(tex->atlas_x + tex->width) / (float)TEXTURE_ARRAY_DIM, (float)tex->atlas_y / (float)TEXTURE_ARRAY_DIM, (float)(tex->atlas_y + tex->height) / (float)TEXTURE_ARRAY_DIM, x_out, y_out, w_out, h_out, ((uint32_t)r << 0) | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24), flags);
}

static void render_tex_scale(const texture_t *tex, float x_out, float y_out, float scale, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint32_t flags)
{
    render_atlas_section_normalized((uint32_t)tex->atlas_id, (float)tex->atlas_x / (float)TEXTURE_ARRAY_DIM, (float)(tex->atlas_x + tex->width) / (float)TEXTURE_ARRAY_DIM, (float)tex->atlas_y / (float)TEXTURE_ARRAY_DIM, (float)(tex->atlas_y + tex->height) / (float)TEXTURE_ARRAY_DIM, x_out, y_out, (float)(tex->width) * scale, (float)(tex->height) * scale, ((uint32_t)r << 0) | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24), flags);
}

static void render_tex(const texture_t *tex, float x_out, float y_out, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint32_t flags)
{
    render_atlas_section_normalized((uint32_t)tex->atlas_id, (float)tex->atlas_x / (float)TEXTURE_ARRAY_DIM, (float)(tex->atlas_x + tex->width) / (float)TEXTURE_ARRAY_DIM, (float)tex->atlas_y / (float)TEXTURE_ARRAY_DIM, (float)(tex->atlas_y + tex->height) / (float)TEXTURE_ARRAY_DIM, x_out, y_out, (float)tex->width, (float)tex->height, ((uint32_t)r << 0) | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24), flags);
}

static void render_text(const font_t *font, float size, char *text, float x_out, float y_out, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    float scale_val = size / font->font_size;

    while (*text)
    {
        if (*text == ' ')
        {
            x_out += font->advance_value * scale_val;
            ++text;
            continue;
        }

        font_glyph_t glyph = font->character_map[(uint8_t)*text];
        float width = (float)glyph.width * scale_val;
        float height = (float)glyph.height * scale_val;

        render_atlas_section((uint32_t)glyph.atlas_id, (float)glyph.atlas_x, (float)glyph.atlas_y, (float)glyph.width, (float)glyph.height, (float)x_out, (float)y_out, width, height, r, g, b, a, 0);
        x_out += width;
        ++text;
    }
}

static void render_text_custom_max_height(const font_t *font, float size, float max_height, char *text, float x_out, float y_out, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    float scale_val = size / font->font_size;

    while (*text)
    {
        if (*text == ' ')
        {
            x_out += font->advance_value * scale_val;
            ++text;
            continue;
        }

        font_glyph_t glyph = font->character_map[(uint8_t)*text];
        float width = (float)glyph.width * scale_val;
        float height = (float)glyph.height * scale_val;

        float y_adj = y_out + (float)(max_height - glyph.height) * scale_val;

        render_atlas_section((uint32_t)glyph.atlas_id, (float)glyph.atlas_x, (float)glyph.atlas_y, (float)glyph.width, (float)glyph.height, (float)x_out, (float)y_adj, width, height, r, g, b, a, 0);
        x_out += width;
        ++text;
    }
}

static void size_text(const font_t *font, float size, char *text, float *text_width, float *text_height)
{
    float scale_val = size / font->font_size;

    *text_width = 0;
    *text_height = 0;

    while (*text)
    {
        if (*text == ' ')
        {
            *text_width += font->advance_value * scale_val;
            ++text;
            continue;
        }

        font_glyph_t glyph = font->character_map[(uint8_t)*text];
        float width = (float)glyph.width * scale_val;
        float height = (float)glyph.height * scale_val;

        *text_width += width;
        *text_height = (*text_height < height) ? height : *text_height;

        ++text;
    }
}

static void size_text_int(const font_t *font, float size, const char *text, int *text_width, int *text_height)
{
    float scale_val = size / font->font_size;

    float text_w = 0;
    float text_h = 0;

    while (*text)
    {
        if (*text == ' ')
        {
            text_w += font->advance_value * scale_val;
            ++text;
            continue;
        }

        font_glyph_t glyph = font->character_map[(uint8_t)*text];
        float width = (float)glyph.width * scale_val;
        float height = (float)glyph.height * scale_val;

        text_w += width;

        text_h = (text_h < height) ? height : text_h;

        ++text;
    }

    *text_width = (int)text_w;
    *text_height = (int)text_h;
}

void render_button_text_no_bg(struct button_t *self)
{
    if (self->visible == false || self->a == 0)
        return;

    int vis_x = self->x + self->padding_left;
    int vis_y = self->y + self->padding_top;

    if (self->is_interacting || current_time - self->interation_time < self->fade_time)
    {
        uint8_t alpha = perform_texture_alpha_operation_single(current_time, self->interation_time, (int64_t)self->fade_time, 255, self->is_interacting);

        render_text_custom_max_height((const font_t *)self->button_no_bg_text.font_ptr, self->button_no_bg_text.font_size, (float)self->height, self->button_no_bg_text.text_ptr, (float)(vis_x - 1), (float)vis_y, self->button_no_bg_text.r, self->button_no_bg_text.g, self->button_no_bg_text.b, ((uint32_t)self->button_no_bg_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_no_bg_text.font_ptr, self->button_no_bg_text.font_size, (float)self->height, self->button_no_bg_text.text_ptr, (float)(vis_x + 1), (float)vis_y, self->button_no_bg_text.r, self->button_no_bg_text.g, self->button_no_bg_text.b, ((uint32_t)self->button_no_bg_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_no_bg_text.font_ptr, self->button_no_bg_text.font_size, (float)self->height, self->button_no_bg_text.text_ptr, (float)vis_x, (float)(vis_y - 1), self->button_no_bg_text.r, self->button_no_bg_text.g, self->button_no_bg_text.b, ((uint32_t)self->button_no_bg_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_no_bg_text.font_ptr, self->button_no_bg_text.font_size, (float)self->height, self->button_no_bg_text.text_ptr, (float)vis_x, (float)(vis_y + 1), self->button_no_bg_text.r, self->button_no_bg_text.g, self->button_no_bg_text.b, ((uint32_t)self->button_no_bg_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));

        render_text_custom_max_height((const font_t *)self->button_no_bg_text.font_ptr, self->button_no_bg_text.font_size, (float)self->height, self->button_no_bg_text.text_ptr, (float)(vis_x - 2), (float)vis_y, self->button_no_bg_text.r, self->button_no_bg_text.g, self->button_no_bg_text.b, ((uint32_t)self->button_no_bg_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_no_bg_text.font_ptr, self->button_no_bg_text.font_size, (float)self->height, self->button_no_bg_text.text_ptr, (float)(vis_x + 2), (float)vis_y, self->button_no_bg_text.r, self->button_no_bg_text.g, self->button_no_bg_text.b, ((uint32_t)self->button_no_bg_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_no_bg_text.font_ptr, self->button_no_bg_text.font_size, (float)self->height, self->button_no_bg_text.text_ptr, (float)vis_x, (float)(vis_y - 2), self->button_no_bg_text.r, self->button_no_bg_text.g, self->button_no_bg_text.b, ((uint32_t)self->button_no_bg_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_no_bg_text.font_ptr, self->button_no_bg_text.font_size, (float)self->height, self->button_no_bg_text.text_ptr, (float)vis_x, (float)(vis_y + 2), self->button_no_bg_text.r, self->button_no_bg_text.g, self->button_no_bg_text.b, ((uint32_t)self->button_no_bg_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
    }

    render_text_custom_max_height((const font_t *)self->button_no_bg_text.font_ptr, self->button_no_bg_text.font_size, (float)self->height, self->button_no_bg_text.text_ptr, (float)vis_x, (float)vis_y, self->button_no_bg_text.r, self->button_no_bg_text.g, self->button_no_bg_text.b, ((uint32_t)self->button_no_bg_text.a * (uint32_t)self->a) / 255);
}

void render_button_text_solid(struct button_t *self)
{
    if (self->visible == false || self->a == 0)
        return;

    render_tex_custom_scale(self->button_solid_text.texture_ptr, (float)self->x, (float)self->y, (float)(self->padding_right + self->padding_left + self->width), (float)(self->padding_bottom + self->padding_top + self->height), self->r, self->g, self->b, self->a, 0); // background

    int vis_x = self->x + self->padding_left;
    int vis_y = self->y + self->padding_top;

    if (self->is_interacting || current_time - self->interation_time < self->fade_time)
    {
        uint8_t alpha = perform_texture_alpha_operation_single(current_time, self->interation_time, (int64_t)self->fade_time, 255, self->is_interacting);

        render_text_custom_max_height((const font_t *)self->button_solid_text.font_ptr, self->button_solid_text.font_size, (float)self->height, self->button_solid_text.text_ptr, (float)(vis_x - 1), (float)vis_y, self->button_solid_text.r, self->button_solid_text.g, self->button_solid_text.b, ((uint32_t)self->button_solid_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_solid_text.font_ptr, self->button_solid_text.font_size, (float)self->height, self->button_solid_text.text_ptr, (float)(vis_x + 1), (float)vis_y, self->button_solid_text.r, self->button_solid_text.g, self->button_solid_text.b, ((uint32_t)self->button_solid_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_solid_text.font_ptr, self->button_solid_text.font_size, (float)self->height, self->button_solid_text.text_ptr, (float)vis_x, (float)(vis_y - 1), self->button_solid_text.r, self->button_solid_text.g, self->button_solid_text.b, ((uint32_t)self->button_solid_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_solid_text.font_ptr, self->button_solid_text.font_size, (float)self->height, self->button_solid_text.text_ptr, (float)vis_x, (float)(vis_y + 1), self->button_solid_text.r, self->button_solid_text.g, self->button_solid_text.b, ((uint32_t)self->button_solid_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));

        render_text_custom_max_height((const font_t *)self->button_solid_text.font_ptr, self->button_solid_text.font_size, (float)self->height, self->button_solid_text.text_ptr, (float)(vis_x - 2), (float)vis_y, self->button_solid_text.r, self->button_solid_text.g, self->button_solid_text.b, ((uint32_t)self->button_solid_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_solid_text.font_ptr, self->button_solid_text.font_size, (float)self->height, self->button_solid_text.text_ptr, (float)(vis_x + 2), (float)vis_y, self->button_solid_text.r, self->button_solid_text.g, self->button_solid_text.b, ((uint32_t)self->button_solid_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_solid_text.font_ptr, self->button_solid_text.font_size, (float)self->height, self->button_solid_text.text_ptr, (float)vis_x, (float)(vis_y - 2), self->button_solid_text.r, self->button_solid_text.g, self->button_solid_text.b, ((uint32_t)self->button_solid_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_solid_text.font_ptr, self->button_solid_text.font_size, (float)self->height, self->button_solid_text.text_ptr, (float)vis_x, (float)(vis_y + 2), self->button_solid_text.r, self->button_solid_text.g, self->button_solid_text.b, ((uint32_t)self->button_solid_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
    }

    render_text_custom_max_height((const font_t *)self->button_solid_text.font_ptr, self->button_solid_text.font_size, (float)self->height, self->button_solid_text.text_ptr, (float)vis_x, (float)vis_y, self->button_solid_text.r, self->button_solid_text.g, self->button_solid_text.b, ((uint32_t)self->button_solid_text.a * (uint32_t)self->a) / 255);
}

void render_button_text_3_slice(struct button_t *self)
{
    if (self->visible == false || self->a == 0)
        return;

    texture_t *corner = self->button_3_slice_text.corner;

    int corner_width = (int)corner->width;
    int corner_height = (int)corner->height;
    int side_length = self->padding_top + self->padding_bottom + self->height;
    int top_side_length = self->padding_left + self->padding_right + self->width;

    render_tex(corner, (float)self->x, (float)self->y, self->r, self->g, self->b, self->a, ROTATE_0);                                                                      // top left
    render_tex(corner, (float)(self->x + top_side_length - corner_width), (float)self->y, self->r, self->g, self->b, self->a, ROTATE_90);                                  // top right
    render_tex(corner, (float)self->x, (float)(self->y + side_length - corner_height), self->r, self->g, self->b, self->a, ROTATE_180);                                    // bottom left
    render_tex(corner, (float)(self->x + top_side_length - corner_width), (float)(self->y + side_length - corner_height), self->r, self->g, self->b, self->a, ROTATE_270); // bottom right

    texture_t *side = self->button_3_slice_text.side;

    int side_height = (int)side->height;

    render_tex_custom_scale(side, (float)(self->x + corner_width), (float)self->y, (float)(top_side_length - corner_width * 2), (float)side_height, self->r, self->g, self->b, self->a, ROTATE_0);                                  // top
    render_tex_custom_scale(side, (float)self->x, (float)(self->y + corner_height), (float)side_height, (float)(side_length - corner_height * 2), self->r, self->g, self->b, self->a, ROTATE_270);                                  // left
    render_tex_custom_scale(side, (float)(self->x + top_side_length - side_height), (float)(self->y + corner_height), (float)side_height, (float)(side_length - corner_height * 2), self->r, self->g, self->b, self->a, ROTATE_90); // right
    render_tex_custom_scale(side, (float)(self->x + corner_width), (float)(self->y + side_length - side_height), (float)(top_side_length - corner_width * 2), (float)side_height, self->r, self->g, self->b, self->a, ROTATE_180);  // bottom

    texture_t *center = self->button_3_slice_text.center;
    render_tex_custom_scale(center, (float)(self->x + corner_width), (float)(self->y + corner_height), (float)(top_side_length - corner_width * 2), (float)(side_length - corner_height * 2), self->r, self->g, self->b, self->a, ROTATE_0); // center

    int vis_x = self->x + self->padding_left;
    int vis_y = self->y + self->padding_top;

    if (self->is_interacting || current_time - self->interation_time < self->fade_time)
    {
        uint8_t alpha = perform_texture_alpha_operation_single(current_time, self->interation_time, (int64_t)self->fade_time, 255, self->is_interacting);

        render_text_custom_max_height((const font_t *)self->button_3_slice_text.font_ptr, self->button_3_slice_text.font_size, (float)self->height, self->button_3_slice_text.text_ptr, (float)(vis_x - 1), (float)vis_y, self->button_3_slice_text.r, self->button_3_slice_text.g, self->button_3_slice_text.b, ((uint32_t)self->button_3_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_3_slice_text.font_ptr, self->button_3_slice_text.font_size, (float)self->height, self->button_3_slice_text.text_ptr, (float)(vis_x + 1), (float)vis_y, self->button_3_slice_text.r, self->button_3_slice_text.g, self->button_3_slice_text.b, ((uint32_t)self->button_3_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_3_slice_text.font_ptr, self->button_3_slice_text.font_size, (float)self->height, self->button_3_slice_text.text_ptr, (float)vis_x, (float)(vis_y - 1), self->button_3_slice_text.r, self->button_3_slice_text.g, self->button_3_slice_text.b, ((uint32_t)self->button_3_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_3_slice_text.font_ptr, self->button_3_slice_text.font_size, (float)self->height, self->button_3_slice_text.text_ptr, (float)vis_x, (float)(vis_y + 1), self->button_3_slice_text.r, self->button_3_slice_text.g, self->button_3_slice_text.b, ((uint32_t)self->button_3_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));

        render_text_custom_max_height((const font_t *)self->button_3_slice_text.font_ptr, self->button_3_slice_text.font_size, (float)self->height, self->button_3_slice_text.text_ptr, (float)(vis_x - 2), (float)vis_y, self->button_3_slice_text.r, self->button_3_slice_text.g, self->button_3_slice_text.b, ((uint32_t)self->button_3_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_3_slice_text.font_ptr, self->button_3_slice_text.font_size, (float)self->height, self->button_3_slice_text.text_ptr, (float)(vis_x + 2), (float)vis_y, self->button_3_slice_text.r, self->button_3_slice_text.g, self->button_3_slice_text.b, ((uint32_t)self->button_3_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_3_slice_text.font_ptr, self->button_3_slice_text.font_size, (float)self->height, self->button_3_slice_text.text_ptr, (float)vis_x, (float)(vis_y - 2), self->button_3_slice_text.r, self->button_3_slice_text.g, self->button_3_slice_text.b, ((uint32_t)self->button_3_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_3_slice_text.font_ptr, self->button_3_slice_text.font_size, (float)self->height, self->button_3_slice_text.text_ptr, (float)vis_x, (float)(vis_y + 2), self->button_3_slice_text.r, self->button_3_slice_text.g, self->button_3_slice_text.b, ((uint32_t)self->button_3_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
    }

    render_text_custom_max_height((const font_t *)self->button_3_slice_text.font_ptr, self->button_3_slice_text.font_size, (float)self->height, self->button_3_slice_text.text_ptr, (float)vis_x, (float)vis_y, self->button_3_slice_text.r, self->button_3_slice_text.g, self->button_3_slice_text.b, ((uint32_t)self->button_3_slice_text.a * (uint32_t)self->a) / 255);
}

void render_button_text_9_slice(struct button_t *self)
{
    if (self->visible == false || self->a == 0)
        return;

    int side_length = self->padding_top + self->padding_bottom + self->height;
    int top_side_length = self->padding_left + self->padding_right + self->width;

    texture_t *tl = self->button_9_slice_text.top_left;
    texture_t *t = self->button_9_slice_text.top;
    texture_t *tr = self->button_9_slice_text.top_right;
    texture_t *l = self->button_9_slice_text.left;
    texture_t *c = self->button_9_slice_text.center;
    texture_t *r = self->button_9_slice_text.right;
    texture_t *bl = self->button_9_slice_text.bottom_left;
    texture_t *b = self->button_9_slice_text.bottom;
    texture_t *br = self->button_9_slice_text.bottom_right;

    int total_w = self->padding_left + self->padding_right + self->width;
    int total_h = self->padding_top + self->padding_bottom + self->height;

    int w_left = (int)tl->width;
    int w_right = (int)tr->width;
    int h_top = (int)tl->height;
    int h_bottom = (int)bl->height;

    float mid_w = (float)(total_w - w_left - w_right);
    float mid_h = (float)(total_h - h_top - h_bottom);

    float x_start = (float)self->x;
    float x_mid = (float)(self->x + w_left);
    float x_end = (float)(self->x + total_w - w_right);

    float y_start = (float)self->y;
    float y_mid = (float)(self->y + h_top);
    float y_end = (float)(self->y + total_h - h_bottom);

    render_tex(tl, x_start, y_start, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex_custom_scale(t, x_mid, y_start, mid_w, (float)h_top, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex(tr, x_end, y_start, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex_custom_scale(l, x_start, y_mid, (float)w_left, mid_h, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex_custom_scale(c, x_mid, y_mid, mid_w, mid_h, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex_custom_scale(r, x_end, y_mid, (float)w_right, mid_h, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex(bl, x_start, y_end, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex_custom_scale(b, x_mid, y_end, mid_w, (float)h_bottom, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex(br, x_end, y_end, self->r, self->g, self->b, self->a, ROTATE_0);

    int vis_x = self->x + self->padding_left;
    int vis_y = self->y + self->padding_top;

    if (self->is_interacting || current_time - self->interation_time < self->fade_time)
    {
        uint8_t alpha = perform_texture_alpha_operation_single(current_time, self->interation_time, (int64_t)self->fade_time, 255, self->is_interacting);

        render_text_custom_max_height((const font_t *)self->button_9_slice_text.font_ptr, self->button_9_slice_text.font_size, (float)self->height, self->button_9_slice_text.text_ptr, (float)(vis_x - 1), (float)vis_y, self->button_9_slice_text.r, self->button_9_slice_text.g, self->button_9_slice_text.b, ((uint32_t)self->button_9_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_9_slice_text.font_ptr, self->button_9_slice_text.font_size, (float)self->height, self->button_9_slice_text.text_ptr, (float)(vis_x + 1), (float)vis_y, self->button_9_slice_text.r, self->button_9_slice_text.g, self->button_9_slice_text.b, ((uint32_t)self->button_9_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_9_slice_text.font_ptr, self->button_9_slice_text.font_size, (float)self->height, self->button_9_slice_text.text_ptr, (float)vis_x, (float)(vis_y - 1), self->button_9_slice_text.r, self->button_9_slice_text.g, self->button_9_slice_text.b, ((uint32_t)self->button_9_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_9_slice_text.font_ptr, self->button_9_slice_text.font_size, (float)self->height, self->button_9_slice_text.text_ptr, (float)vis_x, (float)(vis_y + 1), self->button_9_slice_text.r, self->button_9_slice_text.g, self->button_9_slice_text.b, ((uint32_t)self->button_9_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));

        render_text_custom_max_height((const font_t *)self->button_9_slice_text.font_ptr, self->button_9_slice_text.font_size, (float)self->height, self->button_9_slice_text.text_ptr, (float)(vis_x - 2), (float)vis_y, self->button_9_slice_text.r, self->button_9_slice_text.g, self->button_9_slice_text.b, ((uint32_t)self->button_9_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_9_slice_text.font_ptr, self->button_9_slice_text.font_size, (float)self->height, self->button_9_slice_text.text_ptr, (float)(vis_x + 2), (float)vis_y, self->button_9_slice_text.r, self->button_9_slice_text.g, self->button_9_slice_text.b, ((uint32_t)self->button_9_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_9_slice_text.font_ptr, self->button_9_slice_text.font_size, (float)self->height, self->button_9_slice_text.text_ptr, (float)vis_x, (float)(vis_y - 2), self->button_9_slice_text.r, self->button_9_slice_text.g, self->button_9_slice_text.b, ((uint32_t)self->button_9_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
        render_text_custom_max_height((const font_t *)self->button_9_slice_text.font_ptr, self->button_9_slice_text.font_size, (float)self->height, self->button_9_slice_text.text_ptr, (float)vis_x, (float)(vis_y + 2), self->button_9_slice_text.r, self->button_9_slice_text.g, self->button_9_slice_text.b, ((uint32_t)self->button_9_slice_text.a * (uint32_t)self->a * (uint32_t)alpha) / (255 * 255 * 8));
    }

    render_text_custom_max_height((const font_t *)self->button_9_slice_text.font_ptr, self->button_9_slice_text.font_size, (float)self->height, self->button_9_slice_text.text_ptr, (float)vis_x, (float)vis_y, self->button_9_slice_text.r, self->button_9_slice_text.g, self->button_9_slice_text.b, ((uint32_t)self->button_9_slice_text.a * (uint32_t)self->a) / 255);
}

void render_button_image(struct button_t *self)
{
    if (self->visible == false || self->a == 0)
        return;

    render_tex_custom_scale(self->button_image.texture_ptr, (float)(self->x + self->padding_left), (float)(self->y + self->padding_top), (float)(self->width), (float)(self->height), self->button_image.r, self->button_image.g, self->button_image.b, ((uint32_t)self->button_image.a * (uint32_t)self->a) / 255, 0); // background
}

void render_button_image_3_slice(struct button_t *self)
{
    if (self->visible == false || self->a == 0)
        return;

    texture_t *corner = self->button_3_slice_image.corner;

    int corner_width = (int)corner->width;
    int corner_height = (int)corner->height;
    int side_length = self->padding_top + self->padding_bottom + self->height;
    int top_side_length = self->padding_left + self->padding_right + self->width;

    render_tex(corner, (float)self->x, (float)self->y, self->r, self->g, self->b, self->a, ROTATE_0);                                                                      // top left
    render_tex(corner, (float)(self->x + top_side_length - corner_width), (float)self->y, self->r, self->g, self->b, self->a, ROTATE_90);                                  // top right
    render_tex(corner, (float)self->x, (float)(self->y + side_length - corner_height), self->r, self->g, self->b, self->a, ROTATE_180);                                    // bottom left
    render_tex(corner, (float)(self->x + top_side_length - corner_width), (float)(self->y + side_length - corner_height), self->r, self->g, self->b, self->a, ROTATE_270); // bottom right

    texture_t *side = self->button_3_slice_image.side;

    int side_height = (int)side->height;

    render_tex_custom_scale(side, (float)(self->x + corner_width), (float)self->y, (float)(top_side_length - corner_width * 2), (float)side_height, self->r, self->g, self->b, self->a, ROTATE_0);                                  // top
    render_tex_custom_scale(side, (float)self->x, (float)(self->y + corner_height), (float)side_height, (float)(side_length - corner_height * 2), self->r, self->g, self->b, self->a, ROTATE_270);                                  // left
    render_tex_custom_scale(side, (float)(self->x + top_side_length - side_height), (float)(self->y + corner_height), (float)side_height, (float)(side_length - corner_height * 2), self->r, self->g, self->b, self->a, ROTATE_90); // right
    render_tex_custom_scale(side, (float)(self->x + corner_width), (float)(self->y + side_length - side_height), (float)(top_side_length - corner_width * 2), (float)side_height, self->r, self->g, self->b, self->a, ROTATE_180);  // bottom

    texture_t *center = self->button_3_slice_image.center;
    render_tex_custom_scale(center, (float)(self->x + corner_width), (float)(self->y + corner_height), (float)(top_side_length - corner_width * 2), (float)(side_length - corner_height * 2), self->r, self->g, self->b, self->a, ROTATE_0); // center

    render_tex_custom_scale(self->button_3_slice_image.texture_ptr, (float)(self->x + self->padding_left), (float)(self->y + self->padding_top), (float)(self->width), (float)(self->height), self->button_3_slice_image.r, self->button_3_slice_image.g, self->button_3_slice_image.b, ((uint32_t)self->button_3_slice_image.a * (uint32_t)self->a) / 255, 0); // background
}

void render_button_image_9_slice(struct button_t *self)
{
    if (self->visible == false || self->a == 0)
        return;

    int side_length = self->padding_top + self->padding_bottom + self->height;
    int top_side_length = self->padding_left + self->padding_right + self->width;

    texture_t *tl = self->button_9_slice_image.top_left;
    texture_t *t = self->button_9_slice_image.top;
    texture_t *tr = self->button_9_slice_image.top_right;
    texture_t *l = self->button_9_slice_image.left;
    texture_t *c = self->button_9_slice_image.center;
    texture_t *r = self->button_9_slice_image.right;
    texture_t *bl = self->button_9_slice_image.bottom_left;
    texture_t *b = self->button_9_slice_image.bottom;
    texture_t *br = self->button_9_slice_image.bottom_right;

    int total_w = self->padding_left + self->padding_right + self->width;
    int total_h = self->padding_top + self->padding_bottom + self->height;

    int w_left = (int)tl->width;
    int w_right = (int)tr->width;
    int h_top = (int)tl->height;
    int h_bottom = (int)bl->height;

    float mid_w = (float)(total_w - w_left - w_right);
    float mid_h = (float)(total_h - h_top - h_bottom);

    float x_start = (float)self->x;
    float x_mid = (float)(self->x + w_left);
    float x_end = (float)(self->x + total_w - w_right);

    float y_start = (float)self->y;
    float y_mid = (float)(self->y + h_top);
    float y_end = (float)(self->y + total_h - h_bottom);

    render_tex(tl, x_start, y_start, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex_custom_scale(t, x_mid, y_start, mid_w, (float)h_top, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex(tr, x_end, y_start, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex_custom_scale(l, x_start, y_mid, (float)w_left, mid_h, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex_custom_scale(c, x_mid, y_mid, mid_w, mid_h, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex_custom_scale(r, x_end, y_mid, (float)w_right, mid_h, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex(bl, x_start, y_end, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex_custom_scale(b, x_mid, y_end, mid_w, (float)h_bottom, self->r, self->g, self->b, self->a, ROTATE_0);
    render_tex(br, x_end, y_end, self->r, self->g, self->b, self->a, ROTATE_0);

    render_tex_custom_scale(self->button_9_slice_image.texture_ptr, (float)(self->x + self->padding_left), (float)(self->y + self->padding_top), (float)(self->width), (float)(self->height), self->button_9_slice_image.r, self->button_9_slice_image.g, self->button_9_slice_image.b, ((uint32_t)self->button_9_slice_image.a * (uint32_t)self->a) / 255, 0); // background
}

button_t *button_alloc()
{
    if (button_buf_size == button_buf_capacity)
    {
        uint64_t protected_space = ROUND_UP_TO_MEMORY_PAGE(button_buf_capacity * sizeof(button_t));
        uint64_t last_mem_page = ROUND_UP_TO_MEMORY_PAGE((button_buf_capacity << 1) * sizeof(button_t));
        uint64_t want_space = last_mem_page - protected_space;
#ifdef __linux__
        if (mprotect((uint8_t *)button_buf + protected_space, want_space, PROT_READ | PROT_WRITE) != 0)
#else
        if (VirtualAlloc((uint8_t *)button_buf + protected_space, want_space, MEM_COMMIT, PAGE_READWRITE) == NULL)
#endif
        {
            fprintf(stderr, "OOM\n");
            exit(1);
        }
        button_buf_capacity <<= 1;
    }
    memset(&button_buf[button_buf_size], 0, sizeof(button_t));
    return &button_buf[button_buf_size++];
}

button_t *button_text_no_bg_init(const font_t *font, float font_size, char *text_ptr, int x, int y, int padding_top, int padding_left, int padding_right, int padding_bottom, uint8_t text_r, uint8_t text_g, uint8_t text_b, uint8_t text_a, uint8_t button_r, uint8_t button_g, uint8_t button_b, uint8_t button_a)
{
    button_t *button = button_alloc();

    button->button_no_bg_text.r = text_r;
    button->button_no_bg_text.g = text_g;
    button->button_no_bg_text.b = text_b;
    button->button_no_bg_text.a = text_a;
    button->button_no_bg_text.font_ptr = (font_t *)font;
    button->button_no_bg_text.font_size = font_size;
    button->button_no_bg_text.text_ptr = text_ptr;

    button->visible = true;
    button->interation_time = -1;
    button->fade_time = 150 * 1000;

    button->padding_bottom = padding_bottom;
    button->padding_left = padding_left;
    button->padding_right = padding_right;
    button->padding_top = padding_top;

    button->x = x;
    button->y = y;

    button->r = button_r;
    button->g = button_g;
    button->b = button_b;
    button->a = button_a;

    size_text_int(font, font_size, text_ptr, &button->width, &button->height);

    return button;
}

button_t *button_text_solid_init(const font_t *font, float font_size, char *text_ptr, texture_t *bg, int x, int y, int padding_top, int padding_left, int padding_right, int padding_bottom, uint8_t text_r, uint8_t text_g, uint8_t text_b, uint8_t text_a, uint8_t button_r, uint8_t button_g, uint8_t button_b, uint8_t button_a)
{
    button_t *button = button_alloc();

    button->button_solid_text.r = text_r;
    button->button_solid_text.g = text_g;
    button->button_solid_text.b = text_b;
    button->button_solid_text.a = text_a;
    button->button_solid_text.font_ptr = (font_t *)font;
    button->button_solid_text.font_size = font_size;
    button->button_solid_text.text_ptr = text_ptr;
    button->button_solid_text.texture_ptr = bg;

    button->visible = true;
    button->interation_time = -1;
    button->fade_time = 150 * 1000;

    button->padding_bottom = padding_bottom;
    button->padding_left = padding_left;
    button->padding_right = padding_right;
    button->padding_top = padding_top;

    button->x = x;
    button->y = y;

    button->r = button_r;
    button->g = button_g;
    button->b = button_b;
    button->a = button_a;

    size_text_int(font, font_size, text_ptr, &button->width, &button->height);

    return button;
}

button_t *button_text_3_slice_init(const font_t *font, float font_size, const char *text_ptr, texture_t *corner, texture_t *side, texture_t *center, int x, int y, int padding_top, int padding_left, int padding_right, int padding_bottom, uint8_t text_r, uint8_t text_g, uint8_t text_b, uint8_t text_a, uint8_t button_r, uint8_t button_g, uint8_t button_b, uint8_t button_a)
{
    button_t *button = button_alloc();

    button->button_3_slice_text.r = text_r;
    button->button_3_slice_text.g = text_g;
    button->button_3_slice_text.b = text_b;
    button->button_3_slice_text.a = text_a;
    button->button_3_slice_text.font_ptr = (font_t *)font;
    button->button_3_slice_text.font_size = font_size;
    button->button_3_slice_text.text_ptr = (char *)text_ptr;
    button->button_3_slice_text.side = side;
    button->button_3_slice_text.corner = corner;
    button->button_3_slice_text.center = center;

    button->visible = true;
    button->interation_time = -1;
    button->fade_time = 150 * 1000;

    button->padding_bottom = padding_bottom;
    button->padding_left = padding_left;
    button->padding_right = padding_right;
    button->padding_top = padding_top;

    button->x = x;
    button->y = y;

    button->r = button_r;
    button->g = button_g;
    button->b = button_b;
    button->a = button_a;

    size_text_int(font, font_size, text_ptr, &button->width, &button->height);

    return button;
}

button_t *button_text_9_slice_init(const font_t *font, float font_size, char *text_ptr, texture_t *top_left, texture_t *top, texture_t *top_right, texture_t *left, texture_t *center, texture_t *right, texture_t *bottom_left, texture_t *bottom, texture_t *bottom_right, int x, int y, int padding_top, int padding_left, int padding_right, int padding_bottom, uint8_t text_r, uint8_t text_g, uint8_t text_b, uint8_t text_a, uint8_t button_r, uint8_t button_g, uint8_t button_b, uint8_t button_a)
{
    button_t *button = button_alloc();

    button->button_9_slice_text.r = text_r;
    button->button_9_slice_text.g = text_g;
    button->button_9_slice_text.b = text_b;
    button->button_9_slice_text.a = text_a;
    button->button_9_slice_text.font_ptr = (font_t *)font;
    button->button_9_slice_text.font_size = font_size;
    button->button_9_slice_text.text_ptr = text_ptr;

    button->button_9_slice_text.top_left = top_left;
    button->button_9_slice_text.top = top;
    button->button_9_slice_text.top_right = top_right;
    button->button_9_slice_text.left = left;
    button->button_9_slice_text.center = center;
    button->button_9_slice_text.right = right;
    button->button_9_slice_text.bottom_left = bottom_left;
    button->button_9_slice_text.bottom = bottom;
    button->button_9_slice_text.bottom_right = bottom_right;

    button->visible = true;
    button->interation_time = -1;
    button->fade_time = 150 * 1000;

    button->padding_bottom = padding_bottom;
    button->padding_left = padding_left;
    button->padding_right = padding_right;
    button->padding_top = padding_top;

    button->x = x;
    button->y = y;

    button->r = button_r;
    button->g = button_g;
    button->b = button_b;
    button->a = button_a;

    size_text_int(font, font_size, text_ptr, &button->width, &button->height);

    return button;
}

button_t *button_image_init(texture_t *image, int x, int y, int padding_top, int padding_left, int padding_right, int padding_bottom, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t button_r, uint8_t button_g, uint8_t button_b, uint8_t button_a)
{
    button_t *button = button_alloc();

    button->button_image.r = r;
    button->button_image.g = g;
    button->button_image.b = b;
    button->button_image.a = a;
    button->button_image.texture_ptr = image;

    button->visible = true;
    button->interation_time = -1;
    button->fade_time = 150 * 1000;

    button->padding_bottom = padding_bottom;
    button->padding_left = padding_left;
    button->padding_right = padding_right;
    button->padding_top = padding_top;

    button->x = x;
    button->y = y;

    button->r = button_r;
    button->g = button_g;
    button->b = button_b;
    button->a = button_a;

    button->width = (int)image->width;
    button->height = (int)image->height;

    return button;
}

button_t *button_image_3_slice_init(texture_t *image, texture_t *corner, texture_t *side, texture_t *center, int x, int y, int padding_top, int padding_left, int padding_right, int padding_bottom, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t button_r, uint8_t button_g, uint8_t button_b, uint8_t button_a)
{
    button_t *button = button_alloc();

    button->button_3_slice_image.r = r;
    button->button_3_slice_image.g = g;
    button->button_3_slice_image.b = b;
    button->button_3_slice_image.a = a;
    button->button_3_slice_image.texture_ptr = image;
    button->button_3_slice_image.side = side;
    button->button_3_slice_image.corner = corner;
    button->button_3_slice_image.center = center;

    button->visible = true;
    button->interation_time = -1;
    button->fade_time = 150 * 1000;

    button->padding_bottom = padding_bottom;
    button->padding_left = padding_left;
    button->padding_right = padding_right;
    button->padding_top = padding_top;

    button->x = x;
    button->y = y;

    button->r = button_r;
    button->g = button_g;
    button->b = button_b;
    button->a = button_a;

    button->width = (int)image->width;
    button->height = (int)image->height;

    return button;
}

button_t *button_image_9_slice_init(texture_t *image, texture_t *top_left, texture_t *top, texture_t *top_right, texture_t *left, texture_t *center, texture_t *right, texture_t *bottom_left, texture_t *bottom, texture_t *bottom_right, int x, int y, int padding_top, int padding_left, int padding_right, int padding_bottom, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t button_r, uint8_t button_g, uint8_t button_b, uint8_t button_a)
{
    button_t *button = button_alloc();

    button->button_9_slice_image.r = r;
    button->button_9_slice_image.g = g;
    button->button_9_slice_image.b = b;
    button->button_9_slice_image.a = a;
    button->button_9_slice_image.texture_ptr = image;

    button->button_9_slice_image.top_left = top_left;
    button->button_9_slice_image.top = top;
    button->button_9_slice_image.top_right = top_right;
    button->button_9_slice_image.left = left;
    button->button_9_slice_image.center = center;
    button->button_9_slice_image.right = right;
    button->button_9_slice_image.bottom_left = bottom_left;
    button->button_9_slice_image.bottom = bottom;
    button->button_9_slice_image.bottom_right = bottom_right;

    button->visible = true;
    button->interation_time = -1;
    button->fade_time = 150 * 1000;

    button->padding_bottom = padding_bottom;
    button->padding_left = padding_left;
    button->padding_right = padding_right;
    button->padding_top = padding_top;

    button->x = x;
    button->y = y;

    button->r = button_r;
    button->g = button_g;
    button->b = button_b;
    button->a = button_a;

    button->width = (int)image->width;
    button->height = (int)image->height;

    return button;
}

uint8_t render_state = 0;

void start_render()
{
    if (render_state != 0)
    {
        fprintf(stderr, "calling start_render without calling end_render");
        exit(1);
    }
    render_state = 1;

    static struct timespec time_sct;
    clock_gettime(CLOCK_MONOTONIC, &time_sct);
    current_time = (time_sct.tv_sec * 1000000) + (time_sct.tv_nsec / 1000);

    // Bind framebuffer and set viewport to fixed size
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, BASE_W, BASE_H);
}

void end_render()
{
    if (render_state != 1)
    {
        fprintf(stderr, "calling end_render without calling start_render");
        exit(1);
    }
    render_state = 0;

    // Render any remaining batched geometry to the framebuffer
    render_batch();

    // Now render the framebuffer texture to the screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(view_x, view_y, view_w, view_h);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(screen_shader_program);
    glBindVertexArray(screen_VAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo_texture);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glfwSwapBuffers(window);
    glfwPollEvents();
}
