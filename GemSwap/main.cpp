
#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>
#include <future>
#include <unistd.h>

#if defined(__APPLE__)
#include <GLUT/GLUT.h>
#include <OpenGL/gl3.h>
#include <OpenGL/glu.h>
#else
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#include <windows.h>
#endif
#include <GL/glew.h>        // must be downloaded
#include <GL/freeglut.h>    // must be downloaded unless you have an Apple
#endif

const unsigned int windowWidth = 512, windowHeight = 512;

bool keyboardState[256];

double T = 0.0;
double DT = 0.0;

int majorVersion = 3, minorVersion = 0;

void getErrorInfo(unsigned int handle)
{
    int logLen;
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLen);
    if (logLen > 0)
    {
        char * log = new char[logLen];
        int written;
        glGetShaderInfoLog(handle, logLen, &written, log);
        printf("Shader log:\n%s", log);
        delete[] log;
    }
}

// check if shader could be compiled
void checkShader(unsigned int shader, char * message)
{
    int OK;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &OK);
    if (!OK)
    {
        printf("%s!\n", message);
        getErrorInfo(shader);
    }
}

// check if shader could be linked
void checkLinking(unsigned int program)
{
    int OK;
    glGetProgramiv(program, GL_LINK_STATUS, &OK);
    if (!OK)
    {
        printf("Failed to link shader program!\n");
        getErrorInfo(program);
    }
}




// row-major matrix 4x4
struct mat4
{
    float m[4][4];
public:
    mat4() {}
    mat4(float m00, float m01, float m02, float m03,
         float m10, float m11, float m12, float m13,
         float m20, float m21, float m22, float m23,
         float m30, float m31, float m32, float m33)
    {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
        m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
    }
    
    mat4 operator*(const mat4& right)
    {
        mat4 result;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                result.m[i][j] = 0;
                for (int k = 0; k < 4; k++) result.m[i][j] += m[i][k] * right.m[k][j];
            }
        }
        return result;
    }
    operator float*() { return &m[0][0]; }
};


// 3D point in homogeneous coordinates
struct vec4
{
    float v[4];
    
    vec4(float x = 0, float y = 0, float z = 0, float w = 1)
    {
        v[0] = x; v[1] = y; v[2] = z; v[3] = w;
    }
    
    vec4 operator*(const mat4& mat)
    {
        vec4 result;
        for (int j = 0; j < 4; j++)
        {
            result.v[j] = 0;
            for (int i = 0; i < 4; i++) result.v[j] += v[i] * mat.m[i][j];
        }
        return result;
    }
    
    vec4 operator+(const vec4& vec)
    {
        vec4 result(v[0] + vec.v[0], v[1] + vec.v[1], v[2] + vec.v[2], v[3] + vec.v[3]);
        return result;
    }
    
    vec4 operator*(const float& s)
    {
        vec4 result(v[0] * s, v[1] * s, v[2] * s, v[3] * s);
        return result;
    }

};

// 2D point in Cartesian coordinates
struct vec2
{
    float x, y;
    
    vec2(float x = 0.0, float y = 0.0) : x(x), y(y) {}
    
    vec2 operator+(const vec2& v)
    {
        return vec2(x + v.x, y + v.y);
    }
    
    vec2 operator*(float s)
    {
        return vec2(x * s, y * s);
    }
};

extern "C" unsigned char* stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);

class Texture {
    unsigned int textureId;
public:
    
    Texture(const std::string& inputFileName){
        unsigned char* data;
        int width; int height; int nComponents = 4;
        
        data = stbi_load(inputFileName.c_str(), &width, &height, &nComponents, 0);
        
        if(data == NULL) { return; }
        
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        
        delete data;
    }
    
    void Bind()
    {
        glBindTexture(GL_TEXTURE_2D, textureId);
    }
};

class Camera
{
    vec2 center;
    vec2 halfSize;
    float orientation;
    
public:
    Camera()
    {
        center = vec2(0.0, 0.0);
        halfSize =  vec2(1.0, 1.0);
        orientation = 0.0;
    }
    
    mat4 GetViewTransformationMatrix()
    {
        mat4 T = mat4(
                      1.0, 0.0, 0.0, 0.0,
                      0.0, 1.0, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      -center.x, -center.y, 0.0, 1.0); // change the signs
        
        mat4 S = mat4(
                      1.0 / halfSize.x, 0.0, 0.0, 0.0,
                      0.0, 1.0 / halfSize.y, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      0.0, 0.0, 0.0, 1.0); // put reciprocals
        
        float alpha = orientation * M_PI / 180.0;
        
        mat4 R = mat4(
            cos(alpha), sin(alpha), 0.0, 0.0,
            -sin(alpha), cos(alpha), 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0); // change alpha to -alpha so sin becomes positive and cos stays the same
        
        return T * R * S;
    }
    
    mat4 GetInverseViewTransformationMatrix()
    {
        mat4 T = mat4(
                      1.0, 0.0, 0.0, 0.0,
                      0.0, 1.0, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      center.x, center.y, 0.0, 1.0); // change the signs
        
        mat4 S = mat4(
                      halfSize.x, 0.0, 0.0, 0.0,
                      0.0, halfSize.y, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      0.0, 0.0, 0.0, 1.0); // put reciprocals
        
        float alpha = orientation * M_PI / 180.0;
        
        mat4 R = mat4(
                      cos(alpha), -sin(alpha), 0.0, 0.0,
                      sin(alpha), cos(alpha), 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      0.0, 0.0, 0.0, 1.0); // change alpha to -alpha so sin becomes positive and cos stays the same
        
        return T * R * S;
    }
    
    void SetAspectRatio(int width, int height)
    {
        halfSize = vec2((float)width / height,1.0);
    }
    
    void Move(float dt)
    {
        if(keyboardState['l']) center = center + vec2(-1.0, 0.0) * dt;
        if(keyboardState['j']) center = center + vec2(1.0, 0.0) * dt;
        if(keyboardState['i']) center = center + vec2(0.0, -1.0) * dt;
        if(keyboardState['k']) center = center + vec2(0.0, 1.0) * dt;
        if(keyboardState['a']) orientation = orientation + 20 * dt;
        if(keyboardState['d']) orientation = orientation - 20 * dt;
    }
    
    void Quake() {
        if (keyboardState['q']) {
        float radius = 0.1;
        float angle = (rand() % 360) * M_PI/180;
        vec2 change = vec2(sin(angle) * radius, cos(angle) * radius);
        center = center + change;
        while (radius > 0) {
            radius = radius - 0.01;
            angle = (angle + (150 + rand() % 60)) * M_PI/180;
            change = vec2(-sin(angle) * radius, -cos(angle) * radius);
            center = center + change;
            change = vec2(sin(angle) * radius, cos(angle) * radius);
            center = center + change;
        }
        } else {
            Reset();
        }
    }
    
    void Reset() {
        center = vec2(0.0, 0.0);
    }
};

Camera camera;

class SuperShader
{
protected:
    //shader ID
    unsigned int shaderProgram;
    
public:
    SuperShader()
    {
        shaderProgram = 0;
    }
    
    ~SuperShader()
    {
        glDeleteProgram(shaderProgram);
    }
    
    void CompileProgram(const char *vertexSource, const char *fragmentSource)
    {
        // create vertex shader from string
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        if (!vertexShader) { printf("Error in vertex shader creation\n"); exit(1); }
        
        glShaderSource(vertexShader, 1, &vertexSource, NULL);
        glCompileShader(vertexShader);
        checkShader(vertexShader, "Vertex shader error");
        
        // create fragment shader from string
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        if (!fragmentShader) { printf("Error in fragment shader creation\n"); exit(1); }
        
        glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
        glCompileShader(fragmentShader);
        checkShader(fragmentShader, "Fragment shader error");
        
        // attach shaders to a single program
        shaderProgram = glCreateProgram();
        if (!shaderProgram) { printf("Error in shader program creation\n"); exit(1); }
        
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        
          printf("compile break\n");
    }
    
    void LinkProgram()
    {
        // program packaging
        glLinkProgram(shaderProgram); // link program
        checkLinking(shaderProgram);
        printf("link break\n");
    }
    
    // shader program starts running
    void Run()
    {
        glUseProgram(shaderProgram);
    }
    
    virtual void UploadColor(vec4 color) {}
    
    void UploadM (mat4 M) {
        int location = glGetUniformLocation(shaderProgram, "M");
        if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, M);
        else printf("uniform M cannot be set\n");
    }
    
    virtual void UploadSamplerID() {}
    
};

class Shader : public SuperShader
{
    
    
public:
    
    // compiles and links vertex and fragment shaders
    Shader()
    {
        // vertex shader in GLSL
        const char *vertexSource = "\n\
        #version 150 \n\
        precision highp float; \n\
        in vec2 vertexPosition;    \n\
        out vec3 color; \n\
        uniform mat4 M; \n\
        uniform vec3 vertexColor; \n\
        void main() \n\
        { \n\
        color = vertexColor; \n\
        gl_Position = vec4(vertexPosition.x, \n\
        vertexPosition.y, 0, 1) * M; \n\
        } \n\
        ";
        
        // fragment shader in GLSL
        const char *fragmentSource = "\n\
        #version 150 \n\
        precision highp float; \n\
        \n\
        in vec3 color;            // variable input: interpolated from the vertex colors \n\
        out vec4 fragmentColor;        // output that goes to the raster memory as told by glBindFragDataLocation \n\
        \n\
        void main() \n\
        { \n\
        fragmentColor = vec4(color, 1); // extend RGB to RGBA \n\
        } \n\
        ";
        
       CompileProgram(vertexSource, fragmentSource);
        
        glBindAttribLocation(shaderProgram, 0, "vertexPosition"); // vertexPosition gets values from Attrib Array 0
        glBindFragDataLocation(shaderProgram, 0, "fragmentColor"); // fragmentColor goes to the frame buffer memory
        
        LinkProgram();
        
    }
    
    // uploads color to GPU
    void UploadColor(vec4 color)
    {
        int location = glGetUniformLocation(shaderProgram, "vertexColor");
        if (location >= 0) glUniform3fv(location, 1, &color.v[0]);
        else printf("uniform vertexColor cannot be set\n");
    }
    
};

class TexturedShader : public SuperShader
{
public:
    TexturedShader()
    {
        
        const char *vertexSource = "\n\
        #version 150 \n\
        precision highp float; \n\
        in vec2 vertexPosition; \n\
        in vec2 vertexTexCoord; \n\
        uniform mat4 M; \n\
        out vec2 texCoord; \n\
        void main() \n\
        {\n\
            texCoord = vertexTexCoord; \n\
            gl_Position = vec4(vertexPosition.x, vertexPosition.y, 0, 1) * M; \n\
        } \n\
    ";
        
        const char *fragmentSource = "\n\
        #version 150 \n\
        precision highp float; \n\
        uniform sampler2D samplerUnit;\n\
        in vec2 texCoord;\n\
        out vec4 fragmentColor;\n\
        void main()\n\
        {\n\
            fragmentColor = texture(samplerUnit, texCoord);\n\
        }\n\
    ";
        
        CompileProgram(vertexSource, fragmentSource);
        
        glBindAttribLocation(shaderProgram, 0, "vertexPosition");
        glBindAttribLocation(shaderProgram, 1, "vertexTexCoord");
        glBindFragDataLocation(shaderProgram, 0, "fragmentColor");
        
        LinkProgram();
        
    }
    
    void UploadSamplerID()
    {
        int samplerUnit = 0;
        int location = glGetUniformLocation(shaderProgram, "samplerUnit");
        glUniform1i(location, samplerUnit);
        glActiveTexture(GL_TEXTURE0 + samplerUnit);
    }
};

class Material
{
protected:
    
    SuperShader* shader;
    vec4 color;
    Texture* texture;
    
public:
    
    Material(SuperShader* s, vec4 c, Texture* t = 0)
    {
        shader = s;
        color = c;
        texture = t;
    }
    
    virtual void UploadAttributes()
    {
        if(texture != 0)
        {
            shader->UploadSamplerID();
            texture->Bind();
        }
        else
            shader->UploadColor(color);
    }
    
    void setColor (vec4 col) {
        color = col;
    }
    
    SuperShader* getShader() {return shader;}

    
};



class AnimatedMaterial : public Material
{
public:

    AnimatedMaterial (Shader* s, vec4 c, Texture* t = 0) : Material(s, c, t) {}
    
    void UploadAttributes()
    {
        float intensity = (sin(T) + 1)/2.0;
        shader->UploadColor(color * intensity);
    }
    
};

class Geometry
{
protected: unsigned int vao;
    
public:
    
    Geometry()
    {
        glGenVertexArrays(1, &vao);
    }
    
    virtual ~Geometry()
    {
        delete this;
    }
    
    virtual void Draw() = 0;
};

class Mesh
{
    Material* material;
    Geometry* geometry;
    int objectID;
    
public:
    
    Mesh(Geometry* g, Material* m, int id)
    {
        material = m;
        geometry = g;
        objectID = id;
    }
    
    SuperShader* getShader() {return material->getShader();}
    
    void Draw()
    {
        material->UploadAttributes();
        geometry->Draw();
    }
    
    int getID() {return objectID;}
};

class Object
{
protected:
    SuperShader* shader;
    Mesh* mesh;
    vec2 position;
    vec2 scaling;
    float orientation;
    float rotation;
    float scale;
    
public:
    
    Object(SuperShader* sh, Mesh* m, vec2 p, vec2 sc, float o, float r, float s = 0)
    {
        shader = sh;
        mesh = m;
        position = p;
        scaling = sc;
        orientation = o;
        rotation = r;
        scale = s;
    }
    
    vec2& GetPosition() {return position;}
    
    void SetPosition(vec2& pos) {
        position = pos;
    }
    
    virtual void UploadAttributes()
    {
        // calculate T, S, R from position, scaling, and orientation
        mat4 T = mat4(
                      1.0, 0.0, 0.0, 0.0,
                      0.0, 1.0, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      position.x, position.y, 0.0, 1.0);
        
        mat4 S = mat4(
                      scaling.x, 0.0, 0.0, 0.0,
                      0.0, scaling.y, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      0.0, 0.0, 0.0, 1.0);
        
        float alpha = orientation / 180.0 * M_PI;
        orientation += (rotation * DT);
        
        // Dramatic Exit
        if (scaling.x > 0) {
        scaling = scaling + vec2(0.01, 0.01) * -1 * scale * (float)sin(DT);
        }
        
        mat4 R = mat4(
                      cos(alpha), sin(alpha), 0.0, 0.0,
                      -sin(alpha), cos(alpha), 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      0.0, 0.0, 0.0, 1.0);
        
        mat4 V = camera.GetViewTransformationMatrix();
        
        mat4 M = S * R * T * V;
        shader->UploadM(M);
    }
    
    void Draw()
    {
        shader->Run();
        UploadAttributes();
        mesh->Draw();
    }
    
    int getID() {
        return mesh->getID();
    }
    
    void SetRotation(float val) {
        rotation = val;
    }
    
    void SetScale(float s) {
        scale = s;
    }
    
    vec2 GetScaling() {
        return scaling;
    }
};

class Triangle : public Geometry
{
    unsigned int vbo;    // vertex array object id
    
public:
    
    Triangle()
    {
        glBindVertexArray(vao);
        
        glGenBuffers(1, &vbo);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        static float vertexCoords[] = {-0.8, -0.8, 0, 0.8, 0.8, -0.8};
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords),
                     vertexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    }
    
    void Draw()
    {
        glBindVertexArray(vao);    // make the vao and its vbos active playing the role of the data source
        glDrawArrays(GL_TRIANGLES, 0, 3); // draw a single triangle with vertices defined in vao
    }
};

class Quad : public Geometry
{
    unsigned int vbo;
    
public:
    Quad()
    {
        glBindVertexArray(vao);
        
        glGenBuffers(1, &vbo);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        static float vertexCoords[] = {-0.7, 0.7, 0.7, 0.7, -0.7, -0.7, 0.7, -0.7};
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    }
    
    void Draw()
    {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
};

class TexturedQuad : public Quad
{
    unsigned int vboTex;
    
public:
    TexturedQuad()
    {
        glBindVertexArray(vao);
        glGenBuffers(1, &vboTex);
        
        glBindBuffer(GL_ARRAY_BUFFER, vboTex);
        static float vertexTexCoords[] = {0,0,1,0,0,1,1,1};
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexTexCoords), vertexTexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        
        // define the texture coordinates here
        // assign the array of texture coordinates to
        // vertex attribute array 1
    }
    
    void Draw()
    {
        glEnable(GL_BLEND); // necessary for transparent pixels
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisable(GL_BLEND);
    }
};

class Star : public Geometry
{
    unsigned int vbo;
    
public:
    Star()
    {
        glBindVertexArray(vao);
        
        glGenBuffers(1, &vbo);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        
        float R = 1.0;
        float r = R * cos(2*M_PI/5) / cos(M_PI/5);
        
        static float vertexCoords[24];
        
        vertexCoords[0] = 0.0;
        vertexCoords[1] = 0.0;
        
        float phi = 0.0;
        
        for (int i = 1; i < 12; i++) {
            if (i % 2 != 0) {
                vertexCoords[i * 2] = R * sin(phi);
                vertexCoords[i * 2 + 1] = R * cos(phi);
            } else {
                vertexCoords[i * 2] = r * sin(phi);
                vertexCoords[i * 2 + 1] = r * cos(phi);
            }
            phi += M_PI/5;
        }
        
        vertexCoords[22] = 0;
        vertexCoords[23] = R;
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        
    }
    
    void Draw()
    {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 12);
    }
};

class Heart : public Geometry
{
    unsigned int vbo;
    
public:
    Heart()
    {
        glBindVertexArray(vao);
        
        glGenBuffers(1, &vbo);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        
        
        static float vertexCoords[100];
        
        float t = -1 * M_PI;
        double change = M_PI / 20;
        
        vertexCoords[0] = 0;
        vertexCoords[1] = 0;
        
        
        for (int i = 2; i < 100; i += 2) {
            vertexCoords[i] = (16 * pow(sin(t), 3)) * 0.05;
            vertexCoords[i+1] = (13 * cos(t) - 5 * cos(2 * t) - 2 * cos(3 * t) - cos(4 * t)) * 0.05;
            t += change;
        }
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        
    }
    
    void Draw()
    {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 50);
    }
};

class Empty : public Geometry
{
    unsigned int vbo;
    
public:
    Empty()
    {
        glBindVertexArray(vao);
        
        glGenBuffers(1, &vbo);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        static float vertexCoords[] = {-0.7, 0.7, 0.7, 0.7, -0.7, -0.7, 0.7, -0.7};
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    }
    
    void Draw()
    {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
};

class Scene {
    Shader* shader;
    TexturedShader* textureShader;
    std::vector<Material*> materials;
    std::vector<Geometry*> geometries;
    std::vector<Mesh*> meshes;
    std::vector<Object*> objects;
    Object* grid[10][10];
    Object* selected;
    int x;
    int y;
    Texture* asteroid;
    Texture* fireball;
    bool activateThree = false;
    Object* emptyObj;
    
public:
    Scene() { shader = 0; textureShader = 0; }
    
    void Initialize() {
        shader = new Shader();
        textureShader = new TexturedShader();
        
        asteroid = new Texture("/Users/sanahsuri/Desktop/AIT/Computer Graphics/GemSwap/GemSwap/asteroid.png");
        fireball = new Texture("/Users/sanahsuri/Desktop/AIT/Computer Graphics/GemSwap/GemSwap/fireball.png");
        
        
        
        materials.push_back(new Material(shader, vec4(1, 0, 0)));
        materials.push_back(new Material(shader, vec4(0, 1, 0)));
        materials.push_back(new Material(shader, vec4(0, 0, 1)));
        materials.push_back(new AnimatedMaterial(shader, vec4(0, 1, 1)));
        materials.push_back(new Material(textureShader, vec4(0, 1, 0), asteroid));
        materials.push_back(new Material(textureShader, vec4(1, 0, 0), fireball));
        
        geometries.push_back(new Triangle());
        geometries.push_back(new Quad());
        geometries.push_back(new Star());
        geometries.push_back(new Heart());
        geometries.push_back(new TexturedQuad());
        geometries.push_back(new TexturedQuad());
        
        meshes.push_back(new Mesh(geometries[0], materials[0], 0));
        meshes.push_back(new Mesh(geometries[1], materials[1], 1));
        meshes.push_back(new Mesh(geometries[2], materials[2], 2));
        meshes.push_back(new Mesh(geometries[3], materials[3], 3));
        meshes.push_back(new Mesh(geometries[4], materials[4], 4));
        meshes.push_back(new Mesh(geometries[5], materials[5], 5));
        
        objects.push_back(new Object(shader, meshes[0], vec2(0,0), vec2(0.06, 0.06), 0, 0));
        objects.push_back(new Object(shader, meshes[1], vec2(0,0), vec2(0.06, 0.06), 0, 0));
        objects.push_back(new Object(shader, meshes[2], vec2(0,0), vec2(0.06, 0.06), 0, 0));
        objects.push_back(new Object(shader, meshes[3], vec2(0,0), vec2(0.06, 0.06), 0, 0));
        
        Material* emptyMat = new Material(shader, vec4(1,0,0));
        Geometry* emptyGeo = new Empty();
        Mesh* emptyMesh = new Mesh(emptyGeo, emptyMat, 99);
        emptyObj = new Object(shader, emptyMesh, vec2(0,0), vec2(0.06, 0.06), 0, 0, 1);
        
        
        for (int j = 0; j < 10; j++) {
            float y = 0.2 * j - 1.0 + 0.1;
            for (int i = 0; i < 10; i++) {
                float x = 0.2 * i - 1.0 + 0.1;
                
                int m = rand() % 6;
                if (m == 2) {
                    grid[j][i] = new Object(shader, meshes[m], vec2(x,y), vec2(0.06, 0.06), 0, 45);
                } else if (m == 4) {
                    grid[j][i] = new Object(textureShader, meshes[m], vec2(x,y), vec2(0.08,0.08), 0, 0);
                } else if (m == 5) {
                    grid[j][i] = new Object(textureShader, meshes[m], vec2(x,y), vec2(0.15, 0.15), 0, 100);
                } else {
                    grid[j][i] = new Object(shader, meshes[m], vec2(x,y), vec2(0.06, 0.06), 0, 0);
                }
            }
        }
        
    }
    
    void Select(int u, int v) {
        x = u;
        y = v;
    }
    
    void Swap(int u, int v) {
        vec2 pos1 = grid[u][v]->GetPosition();
        vec2 pos2 = grid[x][y]->GetPosition();
        if (Legal(x, y, u, v)) {
        grid[u][v]->SetPosition(pos2);
        grid[x][y]->SetPosition(pos1);
        std::swap(grid[u][v], grid[x][y]);
        }
        x = NULL;
        y = NULL;
        u = NULL;
        v = NULL;
        activateThree = true;
    }
    
    bool Legal(int a, int b, int c, int d) {
        // selected  = grid[a][b]
        // toswap = grid[c][d]
            if (a+1 < 10 && a+2 < 10) {
            if (grid[c][d]->getID() == grid[a+1][b]->getID() && grid[a+1][b]->getID() == grid[a+2][b]->getID()) {
                return true;
            } }
            if (a-1 > 0 && a-2 > 0) {
            if (grid[c][d]->getID() == grid[a-1][b]->getID() && grid[a-1][b]->getID() == grid[a-2][b]->getID()) {
                return true;
            } }
            if (a-1 > 0 && a+1 < 10) {
            if (grid[c][d]->getID() == grid[a-1][b]->getID() && grid[a-1][b]->getID() == grid[a+1][b]->getID()) {
            return true;
            } }
            if (b+1 < 10 && b+2 < 10) {
                if (grid[c][d]->getID() == grid[a][b+1]->getID() && grid[a][b+1]->getID() == grid[a][b+2]->getID()){
            return true;
            } }
            if (b-1 > 0 && b-2 > 0) {
                if (grid[c][d]->getID() == grid[a][b-1]->getID() && grid[a][b-1]->getID() == grid[a][b-2]->getID()) {
                    return true;
            } }
            if (b-1 > 0 && b+1 < 10) {
                if (grid[c][d]->getID() == grid[a][b-1]->getID() && grid[a][b-1]->getID() == grid[a][b+1]->getID()) {
                    return true;
            } }
        if (c+1 < 10 && c+2 < 10) {
            if (grid[a][b]->getID() == grid[c+1][d]->getID() && grid[c+1][d]->getID() == grid[c+2][d]->getID()) {
                return true;
            } }
        if (c-1 > 0 && c-2 > 0) {
            if (grid[a][b]->getID() == grid[c-1][d]->getID() && grid[c-1][d]->getID() == grid[c-2][d]->getID()) {
                return true;
            } }
        if (c-1 > 0 && c+1 < 10) {
            if (grid[a][b]->getID() == grid[c-1][d]->getID() && grid[c-1][d]->getID() == grid[c+1][d]->getID()) {
                return true;
            } }
        if (d+1 < 10 && d+2 < 10) {
            if (grid[a][b]->getID() == grid[c][d+1]->getID() && grid[c][d+1]->getID() == grid[c][d+2]->getID()){
                return true;
            } }
        if (d-1 > 0 && d-2 > 0) {
            if (grid[a][b]->getID() == grid[c][d-1]->getID() && grid[c][d-1]->getID() == grid[c][d-2]->getID()) {
                return true;
            } }
        if (d-1 > 0 && d+1 < 10) {
            if (grid[a][b]->getID() == grid[c][d-1]->getID() && grid[c][d-1]->getID() == grid[c][d+1]->getID()) {
                return true;
            } }
        return false;
    }
    
    void Bomb(int u, int v) {
        grid[u][v]->SetRotation(270);
        grid[u][v]->SetScale(6);
        activateThree = true;
    }
    
    
    void QuakeBye() {
        if (keyboardState['q']) {
            int i = rand()%10;
            int j = rand()%10;
            int r = rand()%1000;
            if (r == 1) {
                Bomb(i, j);
            }
        }
    }
    
    void ThreeInARow() {
        if (activateThree) {
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                if (i-1 > 0 && i+1 < 10) {
               if (grid[i][j]->getID() == grid[i+1][j]->getID() && grid[i+1][j]->getID() == grid[i-1][j]->getID()) {
                   Bomb(i, j);
                  Bomb(i-1, j);
                   Bomb(i+1, j);
               }
                    if (j-1 > 0 && j+1 < 10) {
                        if (grid[i][j]->getID() == grid[i][j+1]->getID() && grid[i][j+1]->getID() == grid[i][j-1]->getID()) {
                            Bomb(i, j);
                            Bomb(i, j-1);
                            Bomb(i, j+1);
                    }
                    
                   }
        }
    }
    }
    }
    }
    
    ~Scene() {
        for(int i = 0; i < materials.size(); i++) delete materials[i];
        for(int i = 0; i < geometries.size(); i++) delete geometries[i];
        for(int i = 0; i < meshes.size(); i++) delete meshes[i];
        for(int i = 0; i < objects.size(); i++) delete objects[i];
        for(int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                delete grid[i][j];
            }
        }
        if(shader) delete shader;
    }
    
    void Draw()
    {
        for (int j = 0; j < 10; j++) {
            for (int i = 0; i < 10; i++) {
                grid[j][i]->Draw();
            }
        }
    }
};

Scene scene;

// initialization, create an OpenGL context
void onInitialization()
{
    for(int i = 0; i < 256; i++) keyboardState[i] = false;
    
    glViewport(0, 0, windowWidth, windowHeight);
    scene.Initialize();
    
}

void onExit()
{
    scene.~Scene();
    printf("exit");
}

void onMouse(int button, int state, int i, int j) {
    
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    
    float x = ((float)i/viewport[2] * 2.0) - 1.0;
    float y = 1.0 - ((float) j / viewport[3]) * 2.0;
    
    mat4 invV = camera.GetInverseViewTransformationMatrix();
    
    vec4 p = vec4(x, y, 0, 1) * invV;
    
    int u = (int) floor((p.v[0] + 1.0) * 5.0);
    int v = (int) floor((p.v[1] + 1.0) * 5.0);
    
    if((u < 0) || (u > 9) || (v< 0) || (v > 9)) return;
    
    if (state == GLUT_DOWN) scene.Select(v, u);
    if (state == GLUT_UP) scene.Swap(v, u);
    if (state == GLUT_DOWN && keyboardState['b']) {
        printf("bomb break\n");
        scene.Bomb(v, u);
    }
}

// window has become invalid: redraw
void onDisplay()
{
    
    glClearColor(0, 0, 0, 0); // background color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen
    
    scene.Draw();
    
    glutSwapBuffers(); // exchange the two buffers
    
}

void onKeyboard(unsigned char key, int x, int y)
{
    keyboardState[key] = true;
}

void onKeyboardUp(unsigned char key, int x, int y)
{
    keyboardState[key] = false;
}

void onReshape(int winWidth0, int winHeight0)
{
    camera.SetAspectRatio(winWidth0, winHeight0);
    glViewport(0, 0, winWidth0, winHeight0);
}

void onIdle( ) {
    // time elapsed since program started, in seconds
    double t = glutGet(GLUT_ELAPSED_TIME) * 0.001;
    T = t;
    // variable to remember last time idle was called
    static double lastTime = 0.0;
    // time difference between calls: time step
    double dt = t - lastTime;
    DT = dt;
    // store time
    lastTime = t;
    
    camera.Move(dt);
    camera.Quake();
    scene.QuakeBye();
    scene.ThreeInARow();
    
    glutPostRedisplay();
}

int main(int argc, char * argv[])
{
    glutInit(&argc, argv);
#if !defined(__APPLE__)
    glutInitContextVersion(majorVersion, minorVersion);
#endif
    glutInitWindowSize(windowWidth, windowHeight);     // application window is initially of resolution 512x512
    glutInitWindowPosition(50, 50);            // relative location of the application window
#if defined(__APPLE__)
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_3_2_CORE_PROFILE);  // 8 bit R,G,B,A + double buffer + depth buffer
#else
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
    glutCreateWindow("Triangle Rendering");
    
#if !defined(__APPLE__)
    glewExperimental = true;
    glewInit();
#endif
    printf("GL Vendor    : %s\n", glGetString(GL_VENDOR));
    printf("GL Renderer  : %s\n", glGetString(GL_RENDERER));
    printf("GL Version (string)  : %s\n", glGetString(GL_VERSION));
    glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
    printf("GL Version (integer) : %d.%d\n", majorVersion, minorVersion);
    printf("GLSL Version : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    
    onInitialization();
    
    glutDisplayFunc(onDisplay); // register event handlers
    glutIdleFunc(onIdle);
    glutKeyboardFunc(onKeyboard);
    glutKeyboardUpFunc(onKeyboardUp);
    glutReshapeFunc(onReshape);
    glutMouseFunc(onMouse);
    
    glutMainLoop();
    onExit();
    return 1;
}
