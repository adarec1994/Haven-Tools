#include "shader.h"
#include <iostream>
#include <vector>
#include <cstring>

#ifdef _WIN32
// Define function pointers
PFNGLCREATESHADERPROC glCreateShader = nullptr;
PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
PFNGLATTACHSHADERPROC glAttachShader = nullptr;
PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
PFNGLDELETESHADERPROC glDeleteShader = nullptr;
PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
PFNGLUNIFORM1IPROC glUniform1i = nullptr;
PFNGLUNIFORM1FPROC glUniform1f = nullptr;
PFNGLUNIFORM3FPROC glUniform3f = nullptr;
PFNGLUNIFORM4FPROC glUniform4f = nullptr;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = nullptr;
PFNGLACTIVETEXTUREPROC glActiveTexture = nullptr;
PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = nullptr;
PFNGLGENBUFFERSPROC glGenBuffers = nullptr;
PFNGLBINDBUFFERPROC glBindBuffer = nullptr;
PFNGLBUFFERDATAPROC glBufferData = nullptr;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = nullptr;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = nullptr;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = nullptr;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = nullptr;
PFNGLVERTEXATTRIB3FPROC glVertexAttrib3f = nullptr;
#endif

static bool s_shadersInitialized = false;
static bool s_shadersAvailable = false;
static ShaderProgram s_modelShader;

// Vertex shader for model rendering - uses built-in attributes for compatibility
static const char* MODEL_VERTEX_SHADER = R"(
#version 120

varying vec3 vWorldPos;
varying vec3 vNormal;
varying vec2 vTexCoord;

uniform mat4 uModelViewProj;
uniform mat4 uModelView;

void main() {
    gl_Position = uModelViewProj * gl_Vertex;
    vWorldPos = (uModelView * gl_Vertex).xyz;
    vNormal = gl_NormalMatrix * gl_Normal;
    vTexCoord = gl_MultiTexCoord0.xy;
}
)";

// Fragment shader with normal mapping, specular, and tint support
static const char* MODEL_FRAGMENT_SHADER = R"(
#version 120

varying vec3 vWorldPos;
varying vec3 vNormal;
varying vec2 vTexCoord;

uniform sampler2D uDiffuseTex;
uniform sampler2D uNormalTex;
uniform sampler2D uSpecularTex;
uniform sampler2D uTintTex;

uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform vec4 uTintColor;
uniform float uSpecularPower;
uniform float uAmbientStrength;

uniform int uUseDiffuse;
uniform int uUseNormal;
uniform int uUseSpecular;
uniform int uUseTint;

void main() {
    // Base color
    vec4 diffuseColor;
    if (uUseDiffuse != 0) {
        diffuseColor = texture2D(uDiffuseTex, vTexCoord);
        // Discard fully transparent pixels
        if (diffuseColor.a < 0.1) discard;
    } else {
        diffuseColor = vec4(0.7, 0.7, 0.7, 1.0);
    }

    // Apply tint color directly for hair/skin
    diffuseColor.rgb *= uTintColor.rgb;

    // Apply tint map if available
    if (uUseTint != 0) {
        vec4 tintMask = texture2D(uTintTex, vTexCoord);
        // Tint map can modulate the color further
        diffuseColor.rgb *= mix(vec3(1.0), tintMask.rgb, tintMask.a);
    }

    vec3 normal = normalize(vNormal);

    // Normal mapping (simplified - no tangent space for now)
    if (uUseNormal != 0) {
        vec3 normalMap = texture2D(uNormalTex, vTexCoord).rgb * 2.0 - 1.0;
        // Simple blend with vertex normal
        normal = normalize(normal + normalMap * 0.5);
    }

    // Lighting
    vec3 lightDir = normalize(uLightDir);
    float diff = max(dot(normal, lightDir), 0.0);

    // Ambient
    vec3 ambient = uAmbientStrength * diffuseColor.rgb;

    // Diffuse
    vec3 diffuse = diff * diffuseColor.rgb;

    // Specular
    vec3 specular = vec3(0.0);
    if (uUseSpecular != 0 && diff > 0.0) {
        vec3 viewDir = normalize(uViewPos - vWorldPos);
        vec3 halfDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfDir), 0.0), uSpecularPower);
        vec4 specMap = texture2D(uSpecularTex, vTexCoord);
        specular = spec * specMap.rgb * 0.5;
    }
    
    vec3 result = ambient + diffuse + specular;
    gl_FragColor = vec4(result, diffuseColor.a);
}
)";

bool initShaderExtensions() {
#ifdef _WIN32
    glCreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)wglGetProcAddress("glGetProgramiv");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)wglGetProcAddress("glGetProgramInfoLog");
    glUseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
    glDeleteShader = (PFNGLDELETESHADERPROC)wglGetProcAddress("glDeleteShader");
    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)wglGetProcAddress("glDeleteProgram");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
    glUniform1i = (PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i");
    glUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
    glUniform3f = (PFNGLUNIFORM3FPROC)wglGetProcAddress("glUniform3f");
    glUniform4f = (PFNGLUNIFORM4FPROC)wglGetProcAddress("glUniform4f");
    glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)wglGetProcAddress("glUniformMatrix4fv");
    glActiveTexture = (PFNGLACTIVETEXTUREPROC)wglGetProcAddress("glActiveTexture");
    glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)wglGetProcAddress("glGetAttribLocation");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)wglGetProcAddress("glVertexAttribPointer");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)wglGetProcAddress("glEnableVertexAttribArray");
    glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)wglGetProcAddress("glDisableVertexAttribArray");
    glGenBuffers = (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)wglGetProcAddress("glDeleteBuffers");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)wglGetProcAddress("glGenVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)wglGetProcAddress("glBindVertexArray");
    glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)wglGetProcAddress("glDeleteVertexArrays");
    glVertexAttrib3f = (PFNGLVERTEXATTRIB3FPROC)wglGetProcAddress("glVertexAttrib3f");
    
    // Check required functions
    if (!glCreateShader || !glShaderSource || !glCompileShader || !glCreateProgram ||
        !glAttachShader || !glLinkProgram || !glUseProgram || !glGetUniformLocation ||
        !glUniform1i || !glUniform1f || !glUniform3f || !glUniform4f || !glUniformMatrix4fv ||
        !glActiveTexture) {
        std::cerr << "[SHADER] Failed to load required OpenGL extensions" << std::endl;
        return false;
    }
    
    std::cout << "[SHADER] OpenGL extensions loaded successfully" << std::endl;
    return true;
#else
    return true; // Linux has GL extensions available
#endif
}

uint32_t compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(logLength + 1);
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        std::cerr << "[SHADER] Compilation failed: " << log.data() << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

ShaderProgram createShaderProgram(const char* vertexSrc, const char* fragmentSrc) {
    ShaderProgram program;
    
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
    if (!vs) return program;
    
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!fs) {
        glDeleteShader(vs);
        return program;
    }
    
    program.id = glCreateProgram();
    glAttachShader(program.id, vs);
    glAttachShader(program.id, fs);
    glLinkProgram(program.id);
    
    GLint success;
    glGetProgramiv(program.id, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetProgramiv(program.id, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(logLength + 1);
        glGetProgramInfoLog(program.id, logLength, nullptr, log.data());
        std::cerr << "[SHADER] Link failed: " << log.data() << std::endl;
        glDeleteProgram(program.id);
        glDeleteShader(vs);
        glDeleteShader(fs);
        program.id = 0;
        return program;
    }
    
    // Clean up shaders (they're linked into the program now)
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    // Get uniform locations
    program.uModelViewProj = glGetUniformLocation(program.id, "uModelViewProj");
    program.uModelView = glGetUniformLocation(program.id, "uModelView");
    program.uNormalMatrix = glGetUniformLocation(program.id, "uNormalMatrix");
    program.uLightDir = glGetUniformLocation(program.id, "uLightDir");
    program.uViewPos = glGetUniformLocation(program.id, "uViewPos");
    
    program.uDiffuseTex = glGetUniformLocation(program.id, "uDiffuseTex");
    program.uNormalTex = glGetUniformLocation(program.id, "uNormalTex");
    program.uSpecularTex = glGetUniformLocation(program.id, "uSpecularTex");
    program.uTintTex = glGetUniformLocation(program.id, "uTintTex");
    
    program.uTintColor = glGetUniformLocation(program.id, "uTintColor");
    program.uSpecularPower = glGetUniformLocation(program.id, "uSpecularPower");
    program.uAmbientStrength = glGetUniformLocation(program.id, "uAmbientStrength");
    
    program.uUseDiffuse = glGetUniformLocation(program.id, "uUseDiffuse");
    program.uUseNormal = glGetUniformLocation(program.id, "uUseNormal");
    program.uUseSpecular = glGetUniformLocation(program.id, "uUseSpecular");
    program.uUseTint = glGetUniformLocation(program.id, "uUseTint");
    
    program.valid = true;
    std::cout << "[SHADER] Program created successfully (id=" << program.id << ")" << std::endl;
    
    return program;
}

void deleteShaderProgram(ShaderProgram& program) {
    if (program.id != 0) {
        glDeleteProgram(program.id);
        program.id = 0;
        program.valid = false;
    }
}

bool initShaderSystem() {
    if (s_shadersInitialized) return s_shadersAvailable;
    
    s_shadersInitialized = true;
    
    if (!initShaderExtensions()) {
        s_shadersAvailable = false;
        return false;
    }
    
    s_modelShader = createShaderProgram(MODEL_VERTEX_SHADER, MODEL_FRAGMENT_SHADER);
    if (!s_modelShader.valid) {
        std::cerr << "[SHADER] Failed to create model shader" << std::endl;
        s_shadersAvailable = false;
        return false;
    }
    
    s_shadersAvailable = true;
    std::cout << "[SHADER] Shader system initialized" << std::endl;
    return true;
}

void cleanupShaderSystem() {
    deleteShaderProgram(s_modelShader);
    s_shadersInitialized = false;
    s_shadersAvailable = false;
}

ShaderProgram& getModelShader() {
    return s_modelShader;
}

bool shadersAvailable() {
    return s_shadersAvailable;
}