/**********************************************************************************************
*
*   rmodels - Basic functions to draw 3d shapes and load and draw 3d models
*
*   CONFIGURATION:
*       #define SUPPORT_MODULE_RMODELS
*           rmodels module is included in the build
*
*       #define SUPPORT_FILEFORMAT_OBJ
*       #define SUPPORT_FILEFORMAT_MTL
*       #define SUPPORT_FILEFORMAT_IQM
*       #define SUPPORT_FILEFORMAT_GLTF
*       #define SUPPORT_FILEFORMAT_VOX
*       #define SUPPORT_FILEFORMAT_M3D
*           Selected desired fileformats to be supported for model data loading.
*
*       #define SUPPORT_MESH_GENERATION
*           Support procedural mesh generation functions, uses external par_shapes.h library
*           NOTE: Some generated meshes DO NOT include generated texture coordinates
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2024 Ramon Santamaria (@raysan5)
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#include "raylib.h"         // Declares module functions

// Check if config flags have been externally provided on compilation line
#if !defined(EXTERNAL_CONFIG_FLAGS)
    #include "config.h"     // Defines module configuration flags
#endif

#if defined(SUPPORT_MODULE_RMODELS)

#include "utils.h"          // Required for: TRACELOG(), RL_LoadFileData(), RL_LoadFileText(), RL_SaveFileText()
#include "rlgl.h"           // OpenGL abstraction layer to OpenGL 1.1, 2.1, 3.3+ or ES2
#include "raymath.h"        // Required for: RL_Vector3, RL_Quaternion and RL_Matrix functionality

#include <stdio.h>          // Required for: sprintf()
#include <stdlib.h>         // Required for: malloc(), calloc(), free()
#include <string.h>         // Required for: memcmp(), strlen(), strncpy()
#include <math.h>           // Required for: sinf(), cosf(), sqrtf(), fabsf()

#if defined(SUPPORT_FILEFORMAT_OBJ) || defined(SUPPORT_FILEFORMAT_MTL)
    #define TINYOBJ_MALLOC RL_MALLOC
    #define TINYOBJ_CALLOC RL_CALLOC
    #define TINYOBJ_REALLOC RL_REALLOC
    #define TINYOBJ_FREE RL_FREE

    #define TINYOBJ_LOADER_C_IMPLEMENTATION
    #include "external/tinyobj_loader_c.h"      // OBJ/MTL file formats loading
#endif

#if defined(SUPPORT_FILEFORMAT_GLTF)
    #define CGLTF_MALLOC RL_MALLOC
    #define CGLTF_FREE RL_FREE

    #define CGLTF_IMPLEMENTATION
    #include "external/cgltf.h"         // glTF file format loading
#endif

#if defined(SUPPORT_FILEFORMAT_VOX)
    #define VOX_MALLOC RL_MALLOC
    #define VOX_CALLOC RL_CALLOC
    #define VOX_REALLOC RL_REALLOC
    #define VOX_FREE RL_FREE

    #define VOX_LOADER_IMPLEMENTATION
    #include "external/vox_loader.h"    // VOX file format loading (MagikaVoxel)
#endif

#if defined(SUPPORT_FILEFORMAT_M3D)
    #define M3D_MALLOC RL_MALLOC
    #define M3D_REALLOC RL_REALLOC
    #define M3D_FREE RL_FREE

    #define M3D_IMPLEMENTATION
    #include "external/m3d.h"           // Model3D file format loading
#endif

#if defined(SUPPORT_MESH_GENERATION)
    #define PAR_MALLOC(T, N) ((T*)RL_MALLOC(N*sizeof(T)))
    #define PAR_CALLOC(T, N) ((T*)RL_CALLOC(N*sizeof(T), 1))
    #define PAR_REALLOC(T, BUF, N) ((T*)RL_REALLOC(BUF, sizeof(T)*(N)))
    #define PAR_FREE RL_FREE

    #if defined(_MSC_VER)           // Disable some MSVC warning
        #pragma warning(push)
        #pragma warning(disable : 4244)
        #pragma warning(disable : 4305)
    #endif

    #define PAR_SHAPES_IMPLEMENTATION
    #include "external/par_shapes.h"    // Shapes 3d parametric generation

    #if defined(_MSC_VER)
        #pragma warning(pop)        // Disable MSVC warning suppression
    #endif
#endif

#if defined(_WIN32)
    #include <direct.h>     // Required for: _chdir() [Used in LoadOBJ()]
    #define CHDIR _chdir
#else
    #include <unistd.h>     // Required for: chdir() (POSIX) [Used in LoadOBJ()]
    #define CHDIR chdir
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef MAX_MATERIAL_MAPS
    #define MAX_MATERIAL_MAPS       12    // Maximum number of maps supported
#endif
#ifndef MAX_MESH_VERTEX_BUFFERS
    #define MAX_MESH_VERTEX_BUFFERS  9    // Maximum vertex buffers (VBO) per mesh
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// ...

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
// ...

//----------------------------------------------------------------------------------
// Module specific Functions Declaration
//----------------------------------------------------------------------------------
#if defined(SUPPORT_FILEFORMAT_OBJ)
static RL_Model LoadOBJ(const char *fileName);     // Load OBJ mesh data
#endif
#if defined(SUPPORT_FILEFORMAT_IQM)
static RL_Model LoadIQM(const char *fileName);     // Load IQM mesh data
static RL_ModelAnimation *LoadModelAnimationsIQM(const char *fileName, int *animCount);   // Load IQM animation data
#endif
#if defined(SUPPORT_FILEFORMAT_GLTF)
static RL_Model LoadGLTF(const char *fileName);    // Load GLTF mesh data
static RL_ModelAnimation *LoadModelAnimationsGLTF(const char *fileName, int *animCount);  // Load GLTF animation data
#endif
#if defined(SUPPORT_FILEFORMAT_VOX)
static RL_Model LoadVOX(const char *filename);     // Load VOX mesh data
#endif
#if defined(SUPPORT_FILEFORMAT_M3D)
static RL_Model LoadM3D(const char *filename);     // Load M3D mesh data
static RL_ModelAnimation *LoadModelAnimationsM3D(const char *fileName, int *animCount);   // Load M3D animation data
#endif
#if defined(SUPPORT_FILEFORMAT_OBJ) || defined(SUPPORT_FILEFORMAT_MTL)
static void ProcessMaterialsOBJ(RL_Material *rayMaterials, tinyobj_material_t *materials, int materialCount);  // Process obj materials
#endif

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------

// Draw a line in 3D world space
void RL_DrawLine3D(RL_Vector3 startPos, RL_Vector3 endPos, RL_Color color)
{
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex3f(startPos.x, startPos.y, startPos.z);
        rlVertex3f(endPos.x, endPos.y, endPos.z);
    rlEnd();
}

// Draw a point in 3D space, actually a small line
void RL_DrawPoint3D(RL_Vector3 position, RL_Color color)
{
    rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        rlBegin(RL_LINES);
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex3f(0.0f, 0.0f, 0.0f);
            rlVertex3f(0.0f, 0.0f, 0.1f);
        rlEnd();
    rlPopMatrix();
}

// Draw a circle in 3D world space
void RL_DrawCircle3D(RL_Vector3 center, float radius, RL_Vector3 rotationAxis, float rotationAngle, RL_Color color)
{
    rlPushMatrix();
        rlTranslatef(center.x, center.y, center.z);
        rlRotatef(rotationAngle, rotationAxis.x, rotationAxis.y, rotationAxis.z);

        rlBegin(RL_LINES);
            for (int i = 0; i < 360; i += 10)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);

                rlVertex3f(sinf(DEG2RAD*i)*radius, cosf(DEG2RAD*i)*radius, 0.0f);
                rlVertex3f(sinf(DEG2RAD*(i + 10))*radius, cosf(DEG2RAD*(i + 10))*radius, 0.0f);
            }
        rlEnd();
    rlPopMatrix();
}

// Draw a color-filled triangle (vertex in counter-clockwise order!)
void RL_DrawTriangle3D(RL_Vector3 v1, RL_Vector3 v2, RL_Vector3 v3, RL_Color color)
{
    rlBegin(RL_TRIANGLES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex3f(v1.x, v1.y, v1.z);
        rlVertex3f(v2.x, v2.y, v2.z);
        rlVertex3f(v3.x, v3.y, v3.z);
    rlEnd();
}

// Draw a triangle strip defined by points
void RL_DrawTriangleStrip3D(const RL_Vector3 *points, int pointCount, RL_Color color)
{
    if (pointCount < 3) return; // Security check

    rlBegin(RL_TRIANGLES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        for (int i = 2; i < pointCount; i++)
        {
            if ((i%2) == 0)
            {
                rlVertex3f(points[i].x, points[i].y, points[i].z);
                rlVertex3f(points[i - 2].x, points[i - 2].y, points[i - 2].z);
                rlVertex3f(points[i - 1].x, points[i - 1].y, points[i - 1].z);
            }
            else
            {
                rlVertex3f(points[i].x, points[i].y, points[i].z);
                rlVertex3f(points[i - 1].x, points[i - 1].y, points[i - 1].z);
                rlVertex3f(points[i - 2].x, points[i - 2].y, points[i - 2].z);
            }
        }
    rlEnd();
}

// Draw cube
// NOTE: Cube position is the center position
void RL_DrawCube(RL_Vector3 position, float width, float height, float length, RL_Color color)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    rlPushMatrix();
        // NOTE: Transformation is applied in inverse order (scale -> rotate -> translate)
        rlTranslatef(position.x, position.y, position.z);
        //rlRotatef(45, 0, 1, 0);
        //rlScalef(1.0f, 1.0f, 1.0f);   // NOTE: Vertices are directly scaled on definition

        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            // Front face
            rlNormal3f(0.0f, 0.0f, 1.0f);
            rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Left
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Left

            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top Right
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Left
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right

            // Back face
            rlNormal3f(0.0f, 0.0f, -1.0f);
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Left
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right

            rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Right
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left

            // Top face
            rlNormal3f(0.0f, 1.0f, 0.0f);
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Bottom Left
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Bottom Right

            rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Right
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Bottom Right

            // Bottom face
            rlNormal3f(0.0f, -1.0f, 0.0f);
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Top Left
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right
            rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Left

            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Top Right
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Top Left

            // Right face
            rlNormal3f(1.0f, 0.0f, 0.0f);
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right
            rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Right
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top Left

            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Left
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top Left

            // Left face
            rlNormal3f(-1.0f, 0.0f, 0.0f);
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Right
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Left
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Right

            rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Left
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Left
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Right
        rlEnd();
    rlPopMatrix();
}

// Draw cube (Vector version)
void RL_DrawCubeV(RL_Vector3 position, RL_Vector3 size, RL_Color color)
{
    RL_DrawCube(position, size.x, size.y, size.z, color);
}

// Draw cube wires
void RL_DrawCubeWires(RL_Vector3 position, float width, float height, float length, RL_Color color)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);

        rlBegin(RL_LINES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            // Front face
            //------------------------------------------------------------------
            // Bottom line
            rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom left
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom right

            // Left line
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom right
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top right

            // Top line
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top right
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top left

            // Right line
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top left
            rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom left

            // Back face
            //------------------------------------------------------------------
            // Bottom line
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom left
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom right

            // Left line
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom right
            rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top right

            // Top line
            rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top right
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top left

            // Right line
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top left
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom left

            // Top face
            //------------------------------------------------------------------
            // Left line
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top left front
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top left back

            // Right line
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top right front
            rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top right back

            // Bottom face
            //------------------------------------------------------------------
            // Left line
            rlVertex3f(x - width/2, y - height/2, z + length/2);  // Top left front
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Top left back

            // Right line
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Top right front
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Top right back
        rlEnd();
    rlPopMatrix();
}

// Draw cube wires (vector version)
void RL_DrawCubeWiresV(RL_Vector3 position, RL_Vector3 size, RL_Color color)
{
    RL_DrawCubeWires(position, size.x, size.y, size.z, color);
}

// Draw sphere
void RL_DrawSphere(RL_Vector3 centerPos, float radius, RL_Color color)
{
    RL_DrawSphereEx(centerPos, radius, 16, 16, color);
}

// Draw sphere with extended parameters
void RL_DrawSphereEx(RL_Vector3 centerPos, float radius, int rings, int slices, RL_Color color)
{
#if 0
    // Basic implementation, do not use it!
    // For a sphere with 16 rings and 16 slices it requires 8640 cos()/sin() function calls! 
    // New optimized version below only requires 4 cos()/sin() calls
    
    rlPushMatrix();
        // NOTE: Transformation is applied in inverse order (scale -> translate)
        rlTranslatef(centerPos.x, centerPos.y, centerPos.z);
        rlScalef(radius, radius, radius);
        
        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (int i = 0; i < (rings + 2); i++)
            {
                for (int j = 0; j < slices; j++)
                {
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*i)),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*cosf(DEG2RAD*(360.0f*j/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*(j + 1)/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*(j + 1)/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*j/slices)));

                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*i)),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*cosf(DEG2RAD*(360.0f*j/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i)))*sinf(DEG2RAD*(360.0f*(j + 1)/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i)))*cosf(DEG2RAD*(360.0f*(j + 1)/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*(j + 1)/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*(j + 1)/slices)));
                }
            }
        rlEnd();
    rlPopMatrix();
#endif

    rlPushMatrix();
        // NOTE: Transformation is applied in inverse order (scale -> translate)
        rlTranslatef(centerPos.x, centerPos.y, centerPos.z);
        rlScalef(radius, radius, radius);

        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            float ringangle = DEG2RAD*(180.0f/(rings + 1)); // Angle between latitudinal parallels
            float sliceangle = DEG2RAD*(360.0f/slices); // Angle between longitudinal meridians

            float cosring = cosf(ringangle);
            float sinring = sinf(ringangle);
            float cosslice = cosf(sliceangle);
            float sinslice = sinf(sliceangle);

            RL_Vector3 vertices[4] = { 0 }; // Required to store face vertices
            vertices[2] = (RL_Vector3){ 0, 1, 0 };
            vertices[3] = (RL_Vector3){ sinring, cosring, 0 };

            for (int i = 0; i < rings + 1; i++)
            {
                for (int j = 0; j < slices; j++) 
                {
                    vertices[0] = vertices[2]; // Rotate around y axis to set up vertices for next face
                    vertices[1] = vertices[3];
                    vertices[2] = (RL_Vector3){ cosslice*vertices[2].x - sinslice*vertices[2].z, vertices[2].y, sinslice*vertices[2].x + cosslice*vertices[2].z }; // Rotation matrix around y axis
                    vertices[3] = (RL_Vector3){ cosslice*vertices[3].x - sinslice*vertices[3].z, vertices[3].y, sinslice*vertices[3].x + cosslice*vertices[3].z };

                    rlVertex3f(vertices[0].x, vertices[0].y, vertices[0].z);
                    rlVertex3f(vertices[3].x, vertices[3].y, vertices[3].z);
                    rlVertex3f(vertices[1].x, vertices[1].y, vertices[1].z);

                    rlVertex3f(vertices[0].x, vertices[0].y, vertices[0].z);
                    rlVertex3f(vertices[2].x, vertices[2].y, vertices[2].z);
                    rlVertex3f(vertices[3].x, vertices[3].y, vertices[3].z);
                }

                vertices[2] = vertices[3]; // Rotate around z axis to set up  starting vertices for next ring
                vertices[3] = (RL_Vector3){ cosring*vertices[3].x + sinring*vertices[3].y, -sinring*vertices[3].x + cosring*vertices[3].y, vertices[3].z }; // Rotation matrix around z axis
            }
        rlEnd();
    rlPopMatrix();
}

// Draw sphere wires
void RL_DrawSphereWires(RL_Vector3 centerPos, float radius, int rings, int slices, RL_Color color)
{
    rlPushMatrix();
        // NOTE: Transformation is applied in inverse order (scale -> translate)
        rlTranslatef(centerPos.x, centerPos.y, centerPos.z);
        rlScalef(radius, radius, radius);

        rlBegin(RL_LINES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (int i = 0; i < (rings + 2); i++)
            {
                for (int j = 0; j < slices; j++)
                {
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*i)),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*cosf(DEG2RAD*(360.0f*j/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*(j + 1)/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*(j + 1)/slices)));

                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*(j + 1)/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*(j + 1)/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*j/slices)));

                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*j/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*i)),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*cosf(DEG2RAD*(360.0f*j/slices)));
                }
            }
        rlEnd();
    rlPopMatrix();
}

// Draw a cylinder
// NOTE: It could be also used for pyramid and cone
void RL_DrawCylinder(RL_Vector3 position, float radiusTop, float radiusBottom, float height, int sides, RL_Color color)
{
    if (sides < 3) sides = 3;

    const float angleStep = 360.0f/sides;

    rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);

        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            if (radiusTop > 0)
            {
                // Draw Body -------------------------------------------------------------------------------------
                for (int i = 0; i < sides; i++)
                {
                    rlVertex3f(sinf(DEG2RAD*i*angleStep)*radiusBottom, 0, cosf(DEG2RAD*i*angleStep)*radiusBottom); //Bottom Left
                    rlVertex3f(sinf(DEG2RAD*(i+1)*angleStep)*radiusBottom, 0, cosf(DEG2RAD*(i+1)*angleStep)*radiusBottom); //Bottom Right
                    rlVertex3f(sinf(DEG2RAD*(i+1)*angleStep)*radiusTop, height, cosf(DEG2RAD*(i+1)*angleStep)*radiusTop); //Top Right

                    rlVertex3f(sinf(DEG2RAD*i*angleStep)*radiusTop, height, cosf(DEG2RAD*i*angleStep)*radiusTop); //Top Left
                    rlVertex3f(sinf(DEG2RAD*i*angleStep)*radiusBottom, 0, cosf(DEG2RAD*i*angleStep)*radiusBottom); //Bottom Left
                    rlVertex3f(sinf(DEG2RAD*(i+1)*angleStep)*radiusTop, height, cosf(DEG2RAD*(i+1)*angleStep)*radiusTop); //Top Right
                }

                // Draw Cap --------------------------------------------------------------------------------------
                for (int i = 0; i < sides; i++)
                {
                    rlVertex3f(0, height, 0);
                    rlVertex3f(sinf(DEG2RAD*i*angleStep)*radiusTop, height, cosf(DEG2RAD*i*angleStep)*radiusTop);
                    rlVertex3f(sinf(DEG2RAD*(i+1)*angleStep)*radiusTop, height, cosf(DEG2RAD*(i+1)*angleStep)*radiusTop);
                }
            }
            else
            {
                // Draw Cone -------------------------------------------------------------------------------------
                for (int i = 0; i < sides; i++)
                {
                    rlVertex3f(0, height, 0);
                    rlVertex3f(sinf(DEG2RAD*i*angleStep)*radiusBottom, 0, cosf(DEG2RAD*i*angleStep)*radiusBottom);
                    rlVertex3f(sinf(DEG2RAD*(i+1)*angleStep)*radiusBottom, 0, cosf(DEG2RAD*(i+1)*angleStep)*radiusBottom);
                }
            }

            // Draw Base -----------------------------------------------------------------------------------------
            for (int i = 0; i < sides; i++)
            {
                rlVertex3f(0, 0, 0);
                rlVertex3f(sinf(DEG2RAD*i*angleStep)*radiusBottom, 0, cosf(DEG2RAD*(i+1)*angleStep)*radiusBottom);
                rlVertex3f(sinf(DEG2RAD*i*angleStep)*radiusBottom, 0, cosf(DEG2RAD*i*angleStep)*radiusBottom);
            }

        rlEnd();
    rlPopMatrix();
}

// Draw a cylinder with base at startPos and top at endPos
// NOTE: It could be also used for pyramid and cone
void RL_DrawCylinderEx(RL_Vector3 startPos, RL_Vector3 endPos, float startRadius, float endRadius, int sides, RL_Color color)
{
    if (sides < 3) sides = 3;

    RL_Vector3 direction = { endPos.x - startPos.x, endPos.y - startPos.y, endPos.z - startPos.z };
    if ((direction.x == 0) && (direction.y == 0) && (direction.z == 0)) return; // Security check

    // Construct a basis of the base and the top face:
    RL_Vector3 b1 = Vector3Normalize(Vector3Perpendicular(direction));
    RL_Vector3 b2 = Vector3Normalize(Vector3CrossProduct(b1, direction));

    float baseAngle = (2.0f*PI)/sides;

    rlBegin(RL_TRIANGLES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        for (int i = 0; i < sides; i++)
        {
            // Compute the four vertices
            float s1 = sinf(baseAngle*(i + 0))*startRadius;
            float c1 = cosf(baseAngle*(i + 0))*startRadius;
            RL_Vector3 w1 = { startPos.x + s1*b1.x + c1*b2.x, startPos.y + s1*b1.y + c1*b2.y, startPos.z + s1*b1.z + c1*b2.z };
            float s2 = sinf(baseAngle*(i + 1))*startRadius;
            float c2 = cosf(baseAngle*(i + 1))*startRadius;
            RL_Vector3 w2 = { startPos.x + s2*b1.x + c2*b2.x, startPos.y + s2*b1.y + c2*b2.y, startPos.z + s2*b1.z + c2*b2.z };
            float s3 = sinf(baseAngle*(i + 0))*endRadius;
            float c3 = cosf(baseAngle*(i + 0))*endRadius;
            RL_Vector3 w3 = { endPos.x + s3*b1.x + c3*b2.x, endPos.y + s3*b1.y + c3*b2.y, endPos.z + s3*b1.z + c3*b2.z };
            float s4 = sinf(baseAngle*(i + 1))*endRadius;
            float c4 = cosf(baseAngle*(i + 1))*endRadius;
            RL_Vector3 w4 = { endPos.x + s4*b1.x + c4*b2.x, endPos.y + s4*b1.y + c4*b2.y, endPos.z + s4*b1.z + c4*b2.z };

            if (startRadius > 0)
            {
                rlVertex3f(startPos.x, startPos.y, startPos.z); // |
                rlVertex3f(w2.x, w2.y, w2.z);                   // T0
                rlVertex3f(w1.x, w1.y, w1.z);                   // |
            }
                                                                //          w2 x.-----------x startPos
            rlVertex3f(w1.x, w1.y, w1.z);                       // |           |\'.  T0    /
            rlVertex3f(w2.x, w2.y, w2.z);                       // T1          | \ '.     /
            rlVertex3f(w3.x, w3.y, w3.z);                       // |           |T \  '.  /
                                                                //             | 2 \ T 'x w1
            rlVertex3f(w2.x, w2.y, w2.z);                       // |        w4 x.---\-1-|---x endPos
            rlVertex3f(w4.x, w4.y, w4.z);                       // T2            '.  \  |T3/
            rlVertex3f(w3.x, w3.y, w3.z);                       // |               '. \ | /
                                                                //                   '.\|/
            if (endRadius > 0)                                  //                     'x w3
            {
                rlVertex3f(endPos.x, endPos.y, endPos.z);       // |
                rlVertex3f(w3.x, w3.y, w3.z);                   // T3
                rlVertex3f(w4.x, w4.y, w4.z);                   // |
            }                                                   //
        }
    rlEnd();
}

// Draw a wired cylinder
// NOTE: It could be also used for pyramid and cone
void RL_DrawCylinderWires(RL_Vector3 position, float radiusTop, float radiusBottom, float height, int sides, RL_Color color)
{
    if (sides < 3) sides = 3;

    const float angleStep = 360.0f/sides;

    rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);

        rlBegin(RL_LINES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (int i = 0; i < sides; i++)
            {
                rlVertex3f(sinf(DEG2RAD*i*angleStep)*radiusBottom, 0, cosf(DEG2RAD*i*angleStep)*radiusBottom);
                rlVertex3f(sinf(DEG2RAD*(i+1)*angleStep)*radiusBottom, 0, cosf(DEG2RAD*(i+1)*angleStep)*radiusBottom);

                rlVertex3f(sinf(DEG2RAD*(i+1)*angleStep)*radiusBottom, 0, cosf(DEG2RAD*(i+1)*angleStep)*radiusBottom);
                rlVertex3f(sinf(DEG2RAD*(i+1)*angleStep)*radiusTop, height, cosf(DEG2RAD*(i+1)*angleStep)*radiusTop);

                rlVertex3f(sinf(DEG2RAD*(i+1)*angleStep)*radiusTop, height, cosf(DEG2RAD*(i+1)*angleStep)*radiusTop);
                rlVertex3f(sinf(DEG2RAD*i*angleStep)*radiusTop, height, cosf(DEG2RAD*i*angleStep)*radiusTop);

                rlVertex3f(sinf(DEG2RAD*i*angleStep)*radiusTop, height, cosf(DEG2RAD*i*angleStep)*radiusTop);
                rlVertex3f(sinf(DEG2RAD*i*angleStep)*radiusBottom, 0, cosf(DEG2RAD*i*angleStep)*radiusBottom);
            }
        rlEnd();
    rlPopMatrix();
}


// Draw a wired cylinder with base at startPos and top at endPos
// NOTE: It could be also used for pyramid and cone
void RL_DrawCylinderWiresEx(RL_Vector3 startPos, RL_Vector3 endPos, float startRadius, float endRadius, int sides, RL_Color color)
{
    if (sides < 3) sides = 3;

    RL_Vector3 direction = { endPos.x - startPos.x, endPos.y - startPos.y, endPos.z - startPos.z };
    if ((direction.x == 0) && (direction.y == 0) && (direction.z == 0)) return; // Security check

    // Construct a basis of the base and the top face:
    RL_Vector3 b1 = Vector3Normalize(Vector3Perpendicular(direction));
    RL_Vector3 b2 = Vector3Normalize(Vector3CrossProduct(b1, direction));

    float baseAngle = (2.0f*PI)/sides;

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        for (int i = 0; i < sides; i++)
        {
            // Compute the four vertices
            float s1 = sinf(baseAngle*(i + 0))*startRadius;
            float c1 = cosf(baseAngle*(i + 0))*startRadius;
            RL_Vector3 w1 = { startPos.x + s1*b1.x + c1*b2.x, startPos.y + s1*b1.y + c1*b2.y, startPos.z + s1*b1.z + c1*b2.z };
            float s2 = sinf(baseAngle*(i + 1))*startRadius;
            float c2 = cosf(baseAngle*(i + 1))*startRadius;
            RL_Vector3 w2 = { startPos.x + s2*b1.x + c2*b2.x, startPos.y + s2*b1.y + c2*b2.y, startPos.z + s2*b1.z + c2*b2.z };
            float s3 = sinf(baseAngle*(i + 0))*endRadius;
            float c3 = cosf(baseAngle*(i + 0))*endRadius;
            RL_Vector3 w3 = { endPos.x + s3*b1.x + c3*b2.x, endPos.y + s3*b1.y + c3*b2.y, endPos.z + s3*b1.z + c3*b2.z };
            float s4 = sinf(baseAngle*(i + 1))*endRadius;
            float c4 = cosf(baseAngle*(i + 1))*endRadius;
            RL_Vector3 w4 = { endPos.x + s4*b1.x + c4*b2.x, endPos.y + s4*b1.y + c4*b2.y, endPos.z + s4*b1.z + c4*b2.z };

            rlVertex3f(w1.x, w1.y, w1.z);
            rlVertex3f(w2.x, w2.y, w2.z);

            rlVertex3f(w1.x, w1.y, w1.z);
            rlVertex3f(w3.x, w3.y, w3.z);

            rlVertex3f(w3.x, w3.y, w3.z);
            rlVertex3f(w4.x, w4.y, w4.z);
        }
    rlEnd();
}

// Draw a capsule with the center of its sphere caps at startPos and endPos
void RL_DrawCapsule(RL_Vector3 startPos, RL_Vector3 endPos, float radius, int slices, int rings, RL_Color color)
{
    if (slices < 3) slices = 3;

    RL_Vector3 direction = { endPos.x - startPos.x, endPos.y - startPos.y, endPos.z - startPos.z };

    // draw a sphere if start and end points are the same
    bool sphereCase = (direction.x == 0) && (direction.y == 0) && (direction.z == 0);
    if (sphereCase) direction = (RL_Vector3){0.0f, 1.0f, 0.0f};

    // Construct a basis of the base and the caps:
    RL_Vector3 b0 = Vector3Normalize(direction);
    RL_Vector3 b1 = Vector3Normalize(Vector3Perpendicular(direction));
    RL_Vector3 b2 = Vector3Normalize(Vector3CrossProduct(b1, direction));
    RL_Vector3 capCenter = endPos;

    float baseSliceAngle = (2.0f*PI)/slices;
    float baseRingAngle  = PI*0.5f/rings;

    rlBegin(RL_TRIANGLES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        // render both caps
        for (int c = 0; c < 2; c++)
        {
            for (int i = 0; i < rings; i++)
            {
                for (int j = 0; j < slices; j++)
                {

                    // we build up the rings from capCenter in the direction of the 'direction' vector we computed earlier

                    // as we iterate through the rings they must be placed higher above the center, the height we need is sin(angle(i))
                    // as we iterate through the rings they must get smaller by the cos(angle(i))

                    // compute the four vertices
                    float ringSin1 = sinf(baseSliceAngle*(j + 0))*cosf(baseRingAngle*( i + 0 ));
                    float ringCos1 = cosf(baseSliceAngle*(j + 0))*cosf(baseRingAngle*( i + 0 ));
                    RL_Vector3 w1 = (RL_Vector3){
                        capCenter.x + (sinf(baseRingAngle*( i + 0 ))*b0.x + ringSin1*b1.x + ringCos1*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*( i + 0 ))*b0.y + ringSin1*b1.y + ringCos1*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*( i + 0 ))*b0.z + ringSin1*b1.z + ringCos1*b2.z)*radius
                    };
                    float ringSin2 = sinf(baseSliceAngle*(j + 1))*cosf(baseRingAngle*( i + 0 ));
                    float ringCos2 = cosf(baseSliceAngle*(j + 1))*cosf(baseRingAngle*( i + 0 ));
                    RL_Vector3 w2 = (RL_Vector3){
                        capCenter.x + (sinf(baseRingAngle*( i + 0 ))*b0.x + ringSin2*b1.x + ringCos2*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*( i + 0 ))*b0.y + ringSin2*b1.y + ringCos2*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*( i + 0 ))*b0.z + ringSin2*b1.z + ringCos2*b2.z)*radius
                    };

                    float ringSin3 = sinf(baseSliceAngle*(j + 0))*cosf(baseRingAngle*( i + 1 ));
                    float ringCos3 = cosf(baseSliceAngle*(j + 0))*cosf(baseRingAngle*( i + 1 ));
                    RL_Vector3 w3 = (RL_Vector3){
                        capCenter.x + (sinf(baseRingAngle*( i + 1 ))*b0.x + ringSin3*b1.x + ringCos3*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*( i + 1 ))*b0.y + ringSin3*b1.y + ringCos3*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*( i + 1 ))*b0.z + ringSin3*b1.z + ringCos3*b2.z)*radius
                    };
                    float ringSin4 = sinf(baseSliceAngle*(j + 1))*cosf(baseRingAngle*( i + 1 ));
                    float ringCos4 = cosf(baseSliceAngle*(j + 1))*cosf(baseRingAngle*( i + 1 ));
                    RL_Vector3 w4 = (RL_Vector3){
                        capCenter.x + (sinf(baseRingAngle*( i + 1 ))*b0.x + ringSin4*b1.x + ringCos4*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*( i + 1 ))*b0.y + ringSin4*b1.y + ringCos4*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*( i + 1 ))*b0.z + ringSin4*b1.z + ringCos4*b2.z)*radius
                    };

                    // Make sure cap triangle normals are facing outwards
                    if (c == 0)
                    {
                        rlVertex3f(w1.x, w1.y, w1.z);
                        rlVertex3f(w2.x, w2.y, w2.z);
                        rlVertex3f(w3.x, w3.y, w3.z);

                        rlVertex3f(w2.x, w2.y, w2.z);
                        rlVertex3f(w4.x, w4.y, w4.z);
                        rlVertex3f(w3.x, w3.y, w3.z);
                    }
                    else
                    {
                        rlVertex3f(w1.x, w1.y, w1.z);
                        rlVertex3f(w3.x, w3.y, w3.z);
                        rlVertex3f(w2.x, w2.y, w2.z);

                        rlVertex3f(w2.x, w2.y, w2.z);
                        rlVertex3f(w3.x, w3.y, w3.z);
                        rlVertex3f(w4.x, w4.y, w4.z);
                    }
                }
            }
            capCenter = startPos;
            b0 = Vector3Scale(b0, -1.0f);
        }
        // render middle
        if (!sphereCase)
        {
            for (int j = 0; j < slices; j++)
            {
                // compute the four vertices
                float ringSin1 = sinf(baseSliceAngle*(j + 0))*radius;
                float ringCos1 = cosf(baseSliceAngle*(j + 0))*radius;
                RL_Vector3 w1 = {
                    startPos.x + ringSin1*b1.x + ringCos1*b2.x,
                    startPos.y + ringSin1*b1.y + ringCos1*b2.y,
                    startPos.z + ringSin1*b1.z + ringCos1*b2.z
                };
                float ringSin2 = sinf(baseSliceAngle*(j + 1))*radius;
                float ringCos2 = cosf(baseSliceAngle*(j + 1))*radius;
                RL_Vector3 w2 = {
                    startPos.x + ringSin2*b1.x + ringCos2*b2.x,
                    startPos.y + ringSin2*b1.y + ringCos2*b2.y,
                    startPos.z + ringSin2*b1.z + ringCos2*b2.z
                };

                float ringSin3 = sinf(baseSliceAngle*(j + 0))*radius;
                float ringCos3 = cosf(baseSliceAngle*(j + 0))*radius;
                RL_Vector3 w3 = {
                    endPos.x + ringSin3*b1.x + ringCos3*b2.x,
                    endPos.y + ringSin3*b1.y + ringCos3*b2.y,
                    endPos.z + ringSin3*b1.z + ringCos3*b2.z
                };
                float ringSin4 = sinf(baseSliceAngle*(j + 1))*radius;
                float ringCos4 = cosf(baseSliceAngle*(j + 1))*radius;
                RL_Vector3 w4 = {
                    endPos.x + ringSin4*b1.x + ringCos4*b2.x,
                    endPos.y + ringSin4*b1.y + ringCos4*b2.y,
                    endPos.z + ringSin4*b1.z + ringCos4*b2.z
                };
                                                                        //          w2 x.-----------x startPos
                rlVertex3f(w1.x, w1.y, w1.z);                         // |           |\'.  T0    /
                rlVertex3f(w2.x, w2.y, w2.z);                         // T1          | \ '.     /
                rlVertex3f(w3.x, w3.y, w3.z);                         // |           |T \  '.  /
                                                                        //             | 2 \ T 'x w1
                rlVertex3f(w2.x, w2.y, w2.z);                         // |        w4 x.---\-1-|---x endPos
                rlVertex3f(w4.x, w4.y, w4.z);                         // T2            '.  \  |T3/
                rlVertex3f(w3.x, w3.y, w3.z);                         // |               '. \ | /
                                                                        //                   '.\|/
                                                                        //                   'x w3
            }
        }
    rlEnd();
}

// Draw capsule wires with the center of its sphere caps at startPos and endPos
void RL_DrawCapsuleWires(RL_Vector3 startPos, RL_Vector3 endPos, float radius, int slices, int rings, RL_Color color)
{
    if (slices < 3) slices = 3;

    RL_Vector3 direction = { endPos.x - startPos.x, endPos.y - startPos.y, endPos.z - startPos.z };

    // draw a sphere if start and end points are the same
    bool sphereCase = (direction.x == 0) && (direction.y == 0) && (direction.z == 0);
    if (sphereCase) direction = (RL_Vector3){0.0f, 1.0f, 0.0f};

    // Construct a basis of the base and the caps:
    RL_Vector3 b0 = Vector3Normalize(direction);
    RL_Vector3 b1 = Vector3Normalize(Vector3Perpendicular(direction));
    RL_Vector3 b2 = Vector3Normalize(Vector3CrossProduct(b1, direction));
    RL_Vector3 capCenter = endPos;

    float baseSliceAngle = (2.0f*PI)/slices;
    float baseRingAngle  = PI*0.5f/rings;

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        // render both caps
        for (int c = 0; c < 2; c++)
        {
            for (int i = 0; i < rings; i++)
            {
                for (int j = 0; j < slices; j++)
                {

                    // we build up the rings from capCenter in the direction of the 'direction' vector we computed earlier

                    // as we iterate through the rings they must be placed higher above the center, the height we need is sin(angle(i))
                    // as we iterate through the rings they must get smaller by the cos(angle(i))

                    // compute the four vertices
                    float ringSin1 = sinf(baseSliceAngle*(j + 0))*cosf(baseRingAngle*( i + 0 ));
                    float ringCos1 = cosf(baseSliceAngle*(j + 0))*cosf(baseRingAngle*( i + 0 ));
                    RL_Vector3 w1 = (RL_Vector3){
                        capCenter.x + (sinf(baseRingAngle*( i + 0 ))*b0.x + ringSin1*b1.x + ringCos1*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*( i + 0 ))*b0.y + ringSin1*b1.y + ringCos1*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*( i + 0 ))*b0.z + ringSin1*b1.z + ringCos1*b2.z)*radius
                    };
                    float ringSin2 = sinf(baseSliceAngle*(j + 1))*cosf(baseRingAngle*( i + 0 ));
                    float ringCos2 = cosf(baseSliceAngle*(j + 1))*cosf(baseRingAngle*( i + 0 ));
                    RL_Vector3 w2 = (RL_Vector3){
                        capCenter.x + (sinf(baseRingAngle*( i + 0 ))*b0.x + ringSin2*b1.x + ringCos2*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*( i + 0 ))*b0.y + ringSin2*b1.y + ringCos2*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*( i + 0 ))*b0.z + ringSin2*b1.z + ringCos2*b2.z)*radius
                    };

                    float ringSin3 = sinf(baseSliceAngle*(j + 0))*cosf(baseRingAngle*( i + 1 ));
                    float ringCos3 = cosf(baseSliceAngle*(j + 0))*cosf(baseRingAngle*( i + 1 ));
                    RL_Vector3 w3 = (RL_Vector3){
                        capCenter.x + (sinf(baseRingAngle*( i + 1 ))*b0.x + ringSin3*b1.x + ringCos3*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*( i + 1 ))*b0.y + ringSin3*b1.y + ringCos3*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*( i + 1 ))*b0.z + ringSin3*b1.z + ringCos3*b2.z)*radius
                    };
                    float ringSin4 = sinf(baseSliceAngle*(j + 1))*cosf(baseRingAngle*( i + 1 ));
                    float ringCos4 = cosf(baseSliceAngle*(j + 1))*cosf(baseRingAngle*( i + 1 ));
                    RL_Vector3 w4 = (RL_Vector3){
                        capCenter.x + (sinf(baseRingAngle*( i + 1 ))*b0.x + ringSin4*b1.x + ringCos4*b2.x)*radius,
                        capCenter.y + (sinf(baseRingAngle*( i + 1 ))*b0.y + ringSin4*b1.y + ringCos4*b2.y)*radius,
                        capCenter.z + (sinf(baseRingAngle*( i + 1 ))*b0.z + ringSin4*b1.z + ringCos4*b2.z)*radius
                    };

                    rlVertex3f(w1.x, w1.y, w1.z);
                    rlVertex3f(w2.x, w2.y, w2.z);

                    rlVertex3f(w2.x, w2.y, w2.z);
                    rlVertex3f(w3.x, w3.y, w3.z);

                    rlVertex3f(w1.x, w1.y, w1.z);
                    rlVertex3f(w3.x, w3.y, w3.z);

                    rlVertex3f(w2.x, w2.y, w2.z);
                    rlVertex3f(w4.x, w4.y, w4.z);

                    rlVertex3f(w3.x, w3.y, w3.z);
                    rlVertex3f(w4.x, w4.y, w4.z);
                }
            }
            capCenter = startPos;
            b0 = Vector3Scale(b0, -1.0f);
        }
        // render middle
        if (!sphereCase)
        {
            for (int j = 0; j < slices; j++)
            {
                // compute the four vertices
                float ringSin1 = sinf(baseSliceAngle*(j + 0))*radius;
                float ringCos1 = cosf(baseSliceAngle*(j + 0))*radius;
                RL_Vector3 w1 = {
                    startPos.x + ringSin1*b1.x + ringCos1*b2.x,
                    startPos.y + ringSin1*b1.y + ringCos1*b2.y,
                    startPos.z + ringSin1*b1.z + ringCos1*b2.z
                };
                float ringSin2 = sinf(baseSliceAngle*(j + 1))*radius;
                float ringCos2 = cosf(baseSliceAngle*(j + 1))*radius;
                RL_Vector3 w2 = {
                    startPos.x + ringSin2*b1.x + ringCos2*b2.x,
                    startPos.y + ringSin2*b1.y + ringCos2*b2.y,
                    startPos.z + ringSin2*b1.z + ringCos2*b2.z
                };

                float ringSin3 = sinf(baseSliceAngle*(j + 0))*radius;
                float ringCos3 = cosf(baseSliceAngle*(j + 0))*radius;
                RL_Vector3 w3 = {
                    endPos.x + ringSin3*b1.x + ringCos3*b2.x,
                    endPos.y + ringSin3*b1.y + ringCos3*b2.y,
                    endPos.z + ringSin3*b1.z + ringCos3*b2.z
                };
                float ringSin4 = sinf(baseSliceAngle*(j + 1))*radius;
                float ringCos4 = cosf(baseSliceAngle*(j + 1))*radius;
                RL_Vector3 w4 = {
                    endPos.x + ringSin4*b1.x + ringCos4*b2.x,
                    endPos.y + ringSin4*b1.y + ringCos4*b2.y,
                    endPos.z + ringSin4*b1.z + ringCos4*b2.z
                };

                rlVertex3f(w1.x, w1.y, w1.z);
                rlVertex3f(w3.x, w3.y, w3.z);

                rlVertex3f(w2.x, w2.y, w2.z);
                rlVertex3f(w4.x, w4.y, w4.z);

                rlVertex3f(w2.x, w2.y, w2.z);
                rlVertex3f(w3.x, w3.y, w3.z);
            }
        }
    rlEnd();
}

// Draw a plane
void RL_DrawPlane(RL_Vector3 centerPos, RL_Vector2 size, RL_Color color)
{
    // NOTE: Plane is always created on XZ ground
    rlPushMatrix();
        rlTranslatef(centerPos.x, centerPos.y, centerPos.z);
        rlScalef(size.x, 1.0f, size.y);

        rlBegin(RL_QUADS);
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlNormal3f(0.0f, 1.0f, 0.0f);

            rlVertex3f(-0.5f, 0.0f, -0.5f);
            rlVertex3f(-0.5f, 0.0f, 0.5f);
            rlVertex3f(0.5f, 0.0f, 0.5f);
            rlVertex3f(0.5f, 0.0f, -0.5f);
        rlEnd();
    rlPopMatrix();
}

// Draw a ray line
void RL_DrawRay(RL_Ray ray, RL_Color color)
{
    float scale = 10000;

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlColor4ub(color.r, color.g, color.b, color.a);

        rlVertex3f(ray.position.x, ray.position.y, ray.position.z);
        rlVertex3f(ray.position.x + ray.direction.x*scale, ray.position.y + ray.direction.y*scale, ray.position.z + ray.direction.z*scale);
    rlEnd();
}

// Draw a grid centered at (0, 0, 0)
void RL_DrawGrid(int slices, float spacing)
{
    int halfSlices = slices/2;

    rlBegin(RL_LINES);
        for (int i = -halfSlices; i <= halfSlices; i++)
        {
            if (i == 0)
            {
                rlColor3f(0.5f, 0.5f, 0.5f);
            }
            else
            {
                rlColor3f(0.75f, 0.75f, 0.75f);
            }

            rlVertex3f((float)i*spacing, 0.0f, (float)-halfSlices*spacing);
            rlVertex3f((float)i*spacing, 0.0f, (float)halfSlices*spacing);

            rlVertex3f((float)-halfSlices*spacing, 0.0f, (float)i*spacing);
            rlVertex3f((float)halfSlices*spacing, 0.0f, (float)i*spacing);
        }
    rlEnd();
}

// Load model from files (mesh and material)
RL_Model RL_LoadModel(const char *fileName)
{
    RL_Model model = { 0 };

#if defined(SUPPORT_FILEFORMAT_OBJ)
    if (RL_IsFileExtension(fileName, ".obj")) model = LoadOBJ(fileName);
#endif
#if defined(SUPPORT_FILEFORMAT_IQM)
    if (RL_IsFileExtension(fileName, ".iqm")) model = LoadIQM(fileName);
#endif
#if defined(SUPPORT_FILEFORMAT_GLTF)
    if (RL_IsFileExtension(fileName, ".gltf") || RL_IsFileExtension(fileName, ".glb")) model = LoadGLTF(fileName);
#endif
#if defined(SUPPORT_FILEFORMAT_VOX)
    if (RL_IsFileExtension(fileName, ".vox")) model = LoadVOX(fileName);
#endif
#if defined(SUPPORT_FILEFORMAT_M3D)
    if (RL_IsFileExtension(fileName, ".m3d")) model = LoadM3D(fileName);
#endif

    // Make sure model transform is set to identity matrix!
    model.transform = MatrixIdentity();

    if ((model.meshCount != 0) && (model.meshes != NULL))
    {
        // Upload vertex data to GPU (static meshes)
        for (int i = 0; i < model.meshCount; i++) RL_UploadMesh(&model.meshes[i], false);
    }
    else TRACELOG(LOG_WARNING, "MESH: [%s] Failed to load model mesh(es) data", fileName);

    if (model.materialCount == 0)
    {
        TRACELOG(LOG_WARNING, "MATERIAL: [%s] Failed to load model material data, default to white material", fileName);

        model.materialCount = 1;
        model.materials = (RL_Material *)RL_CALLOC(model.materialCount, sizeof(RL_Material));
        model.materials[0] = RL_LoadMaterialDefault();

        if (model.meshMaterial == NULL) model.meshMaterial = (int *)RL_CALLOC(model.meshCount, sizeof(int));
    }

    return model;
}

// Load model from generated mesh
// WARNING: A shallow copy of mesh is generated, passed by value,
// as long as struct contains pointers to data and some values, we get a copy
// of mesh pointing to same data as original version... be careful!
RL_Model RL_LoadModelFromMesh(RL_Mesh mesh)
{
    RL_Model model = { 0 };

    model.transform = MatrixIdentity();

    model.meshCount = 1;
    model.meshes = (RL_Mesh *)RL_CALLOC(model.meshCount, sizeof(RL_Mesh));
    model.meshes[0] = mesh;

    model.materialCount = 1;
    model.materials = (RL_Material *)RL_CALLOC(model.materialCount, sizeof(RL_Material));
    model.materials[0] = RL_LoadMaterialDefault();

    model.meshMaterial = (int *)RL_CALLOC(model.meshCount, sizeof(int));
    model.meshMaterial[0] = 0;  // First material index

    return model;
}

// Check if a model is ready
bool RL_IsModelReady(RL_Model model)
{
    bool result = false;

    if ((model.meshes != NULL) &&           // Validate model contains some mesh
        (model.materials != NULL) &&        // Validate model contains some material (at least default one)
        (model.meshMaterial != NULL) &&     // Validate mesh-material linkage
        (model.meshCount > 0) &&            // Validate mesh count
        (model.materialCount > 0)) result = true; // Validate material count

    // NOTE: This is a very general model validation, many elements could be validated from a model...

    return result;
}

// Unload model (meshes/materials) from memory (RAM and/or VRAM)
// NOTE: This function takes care of all model elements, for a detailed control
// over them, use RL_UnloadMesh() and RL_UnloadMaterial()
void RL_UnloadModel(RL_Model model)
{
    // Unload meshes
    for (int i = 0; i < model.meshCount; i++) RL_UnloadMesh(model.meshes[i]);

    // Unload materials maps
    // NOTE: As the user could be sharing shaders and textures between models,
    // we don't unload the material but just free its maps,
    // the user is responsible for freeing models shaders and textures
    for (int i = 0; i < model.materialCount; i++) RL_FREE(model.materials[i].maps);

    // Unload arrays
    RL_FREE(model.meshes);
    RL_FREE(model.materials);
    RL_FREE(model.meshMaterial);

    // Unload animation data
    RL_FREE(model.bones);
    RL_FREE(model.bindPose);

    TRACELOG(LOG_INFO, "MODEL: Unloaded model (and meshes) from RAM and VRAM");
}

// Compute model bounding box limits (considers all meshes)
RL_BoundingBox RL_GetModelBoundingBox(RL_Model model)
{
    RL_BoundingBox bounds = { 0 };

    if (model.meshCount > 0)
    {
        RL_Vector3 temp = { 0 };
        bounds = RL_GetMeshBoundingBox(model.meshes[0]);

        for (int i = 1; i < model.meshCount; i++)
        {
            RL_BoundingBox tempBounds = RL_GetMeshBoundingBox(model.meshes[i]);

            temp.x = (bounds.min.x < tempBounds.min.x)? bounds.min.x : tempBounds.min.x;
            temp.y = (bounds.min.y < tempBounds.min.y)? bounds.min.y : tempBounds.min.y;
            temp.z = (bounds.min.z < tempBounds.min.z)? bounds.min.z : tempBounds.min.z;
            bounds.min = temp;

            temp.x = (bounds.max.x > tempBounds.max.x)? bounds.max.x : tempBounds.max.x;
            temp.y = (bounds.max.y > tempBounds.max.y)? bounds.max.y : tempBounds.max.y;
            temp.z = (bounds.max.z > tempBounds.max.z)? bounds.max.z : tempBounds.max.z;
            bounds.max = temp;
        }
    }

    // Apply model.transform to bounding box
    // WARNING: Current RL_BoundingBox structure design does not support rotation transformations,
    // in those cases is up to the user to calculate the proper box bounds (8 vertices transformed)
    bounds.min = Vector3Transform(bounds.min, model.transform);
    bounds.max = Vector3Transform(bounds.max, model.transform);

    return bounds;
}

// Upload vertex data into a VAO (if supported) and VBO
void RL_UploadMesh(RL_Mesh *mesh, bool dynamic)
{
    if (mesh->vaoId > 0)
    {
        // Check if mesh has already been loaded in GPU
        TRACELOG(LOG_WARNING, "VAO: [ID %i] Trying to re-load an already loaded mesh", mesh->vaoId);
        return;
    }

    mesh->vboId = (unsigned int *)RL_CALLOC(MAX_MESH_VERTEX_BUFFERS, sizeof(unsigned int));

    mesh->vaoId = 0;        // Vertex Array Object
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION] = 0;     // Vertex buffer: positions
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD] = 0;     // Vertex buffer: texcoords
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL] = 0;       // Vertex buffer: normals
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] = 0;        // Vertex buffer: colors
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT] = 0;      // Vertex buffer: tangents
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2] = 0;    // Vertex buffer: texcoords2
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES] = 0;      // Vertex buffer: indices

#ifdef RL_SUPPORT_MESH_GPU_SKINNING
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEIDS] = 0;      // Vertex buffer: boneIds
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS] = 0;  // Vertex buffer: boneWeights
#endif


#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    mesh->vaoId = rlLoadVertexArray();
    rlEnableVertexArray(mesh->vaoId);

    // NOTE: Vertex attributes must be uploaded considering default locations points and available vertex data

    // Enable vertex attributes: position (shader-location = 0)
    void *vertices = (mesh->animVertices != NULL)? mesh->animVertices : mesh->vertices;
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION] = rlLoadVertexBuffer(vertices, mesh->vertexCount*3*sizeof(float), dynamic);
    rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION, 3, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION);

    // Enable vertex attributes: texcoords (shader-location = 1)
    mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD] = rlLoadVertexBuffer(mesh->texcoords, mesh->vertexCount*2*sizeof(float), dynamic);
    rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, 2, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD);

    // WARNING: When setting default vertex attribute values, the values for each generic vertex attribute
    // is part of current state, and it is maintained even if a different program object is used

    if (mesh->normals != NULL)
    {
        // Enable vertex attributes: normals (shader-location = 2)
        void *normals = (mesh->animNormals != NULL)? mesh->animNormals : mesh->normals;
        mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL] = rlLoadVertexBuffer(normals, mesh->vertexCount*3*sizeof(float), dynamic);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL, 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL);
    }
    else
    {
        // Default vertex attribute: normal
        // WARNING: Default value provided to shader if location available
        float value[3] = { 1.0f, 1.0f, 1.0f };
        rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL, value, SHADER_ATTRIB_VEC3, 3);
        rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL);
    }

    if (mesh->colors != NULL)
    {
        // Enable vertex attribute: color (shader-location = 3)
        mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] = rlLoadVertexBuffer(mesh->colors, mesh->vertexCount*4*sizeof(unsigned char), dynamic);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, 4, RL_UNSIGNED_BYTE, 1, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
    }
    else
    {
        // Default vertex attribute: color
        // WARNING: Default value provided to shader if location available
        float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };    // RL_WHITE
        rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, value, SHADER_ATTRIB_VEC4, 4);
        rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR);
    }

    if (mesh->tangents != NULL)
    {
        // Enable vertex attribute: tangent (shader-location = 4)
        mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT] = rlLoadVertexBuffer(mesh->tangents, mesh->vertexCount*4*sizeof(float), dynamic);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT, 4, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT);
    }
    else
    {
        // Default vertex attribute: tangent
        // WARNING: Default value provided to shader if location available
        float value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT, value, SHADER_ATTRIB_VEC4, 4);
        rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT);
    }

    if (mesh->texcoords2 != NULL)
    {
        // Enable vertex attribute: texcoord2 (shader-location = 5)
        mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2] = rlLoadVertexBuffer(mesh->texcoords2, mesh->vertexCount*2*sizeof(float), dynamic);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2, 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2);
    }
    else
    {
        // Default vertex attribute: texcoord2
        // WARNING: Default value provided to shader if location available
        float value[2] = { 0.0f, 0.0f };
        rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2, value, SHADER_ATTRIB_VEC2, 2);
        rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2);
    }

#ifdef RL_SUPPORT_MESH_GPU_SKINNING
    if (mesh->boneIds != NULL)
    {
        // Enable vertex attribute: boneIds (shader-location = 6)
        mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEIDS] = rlLoadVertexBuffer(mesh->boneIds, mesh->vertexCount*4*sizeof(unsigned char), dynamic);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEIDS, 4, RL_UNSIGNED_BYTE, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEIDS);
    }
    else
    {
        // Default vertex attribute: boneIds
        // WARNING: Default value provided to shader if location available
        float value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEIDS, value, SHADER_ATTRIB_VEC4, 4);
        rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEIDS);
    }
    
    if (mesh->boneWeights != NULL)
    {
        // Enable vertex attribute: boneWeights (shader-location = 7)
        mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS] = rlLoadVertexBuffer(mesh->boneWeights, mesh->vertexCount*4*sizeof(float), dynamic);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS, 4, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS);
    }
    else
    {
        // Default vertex attribute: boneWeights
        // WARNING: Default value provided to shader if location available
        float value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        rlSetVertexAttributeDefault(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS, value, SHADER_ATTRIB_VEC4, 2);
        rlDisableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS);
    }
#endif

    if (mesh->indices != NULL)
    {
        mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES] = rlLoadVertexBufferElement(mesh->indices, mesh->triangleCount*3*sizeof(unsigned short), dynamic);
    }

    if (mesh->vaoId > 0) TRACELOG(LOG_INFO, "VAO: [ID %i] RL_Mesh uploaded successfully to VRAM (GPU)", mesh->vaoId);
    else TRACELOG(LOG_INFO, "VBO: RL_Mesh uploaded successfully to VRAM (GPU)");

    rlDisableVertexArray();
#endif
}

// Update mesh vertex data in GPU for a specific buffer index
void RL_UpdateMeshBuffer(RL_Mesh mesh, int index, const void *data, int dataSize, int offset)
{
    rlUpdateVertexBuffer(mesh.vboId[index], data, dataSize, offset);
}

// Draw a 3d mesh with material and transform
void RL_DrawMesh(RL_Mesh mesh, RL_Material material, RL_Matrix transform)
{
#if defined(GRAPHICS_API_OPENGL_11)
    #define GL_VERTEX_ARRAY         0x8074
    #define GL_NORMAL_ARRAY         0x8075
    #define GL_COLOR_ARRAY          0x8076
    #define GL_TEXTURE_COORD_ARRAY  0x8078

    rlEnableTexture(material.maps[MATERIAL_MAP_DIFFUSE].texture.id);

    rlEnableStatePointer(GL_VERTEX_ARRAY, mesh.vertices);
    rlEnableStatePointer(GL_TEXTURE_COORD_ARRAY, mesh.texcoords);
    rlEnableStatePointer(GL_NORMAL_ARRAY, mesh.normals);
    rlEnableStatePointer(GL_COLOR_ARRAY, mesh.colors);

    rlPushMatrix();
        rlMultMatrixf(MatrixToFloat(transform));
        rlColor4ub(material.maps[MATERIAL_MAP_DIFFUSE].color.r,
                   material.maps[MATERIAL_MAP_DIFFUSE].color.g,
                   material.maps[MATERIAL_MAP_DIFFUSE].color.b,
                   material.maps[MATERIAL_MAP_DIFFUSE].color.a);

        if (mesh.indices != NULL) rlDrawVertexArrayElements(0, mesh.triangleCount*3, mesh.indices);
        else rlDrawVertexArray(0, mesh.vertexCount);
    rlPopMatrix();

    rlDisableStatePointer(GL_VERTEX_ARRAY);
    rlDisableStatePointer(GL_TEXTURE_COORD_ARRAY);
    rlDisableStatePointer(GL_NORMAL_ARRAY);
    rlDisableStatePointer(GL_COLOR_ARRAY);

    rlDisableTexture();
#endif

#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    // Bind shader program
    rlEnableShader(material.shader.id);

    // Send required data to shader (matrices, values)
    //-----------------------------------------------------
    // Upload to shader material.colDiffuse
    if (material.shader.locs[SHADER_LOC_COLOR_DIFFUSE] != -1)
    {
        float values[4] = {
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.r/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.g/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.b/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.a/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_DIFFUSE], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Upload to shader material.colSpecular (if location available)
    if (material.shader.locs[SHADER_LOC_COLOR_SPECULAR] != -1)
    {
        float values[4] = {
            (float)material.maps[MATERIAL_MAP_SPECULAR].color.r/255.0f,
            (float)material.maps[MATERIAL_MAP_SPECULAR].color.g/255.0f,
            (float)material.maps[MATERIAL_MAP_SPECULAR].color.b/255.0f,
            (float)material.maps[MATERIAL_MAP_SPECULAR].color.a/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_SPECULAR], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Get a copy of current matrices to work with,
    // just in case stereo render is required, and we need to modify them
    // NOTE: At this point the modelview matrix just contains the view matrix (camera)
    // That's because RL_BeginMode3D() sets it and there is no model-drawing function
    // that modifies it, all use rlPushMatrix() and rlPopMatrix()
    RL_Matrix matModel = MatrixIdentity();
    RL_Matrix matView = rlGetMatrixModelview();
    RL_Matrix matModelView = MatrixIdentity();
    RL_Matrix matProjection = rlGetMatrixProjection();

    // Upload view and projection matrices (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_VIEW] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_VIEW], matView);
    if (material.shader.locs[SHADER_LOC_MATRIX_PROJECTION] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_PROJECTION], matProjection);

    // Accumulate several model transformations:
    //    transform: model transformation provided (includes RL_DrawModel() params combined with model.transform)
    //    rlGetMatrixTransform(): rlgl internal transform matrix due to push/pop matrix stack
    matModel = MatrixMultiply(transform, rlGetMatrixTransform());

    // RL_Model transformation matrix is sent to shader uniform location: SHADER_LOC_MATRIX_MODEL
    if (material.shader.locs[SHADER_LOC_MATRIX_MODEL] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MODEL], matModel);

    // Get model-view matrix
    matModelView = MatrixMultiply(matModel, matView);

    // Upload model normal matrix (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_NORMAL] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_NORMAL], MatrixTranspose(MatrixInvert(matModel)));

#ifdef RL_SUPPORT_MESH_GPU_SKINNING
    // Upload Bone Transforms    
    if (material.shader.locs[SHADER_LOC_BONE_MATRICES] != -1 && mesh.boneMatrices)
    {
        rlSetUniformMatrices(material.shader.locs[SHADER_LOC_BONE_MATRICES], mesh.boneMatrices, mesh.boneCount);
    }
#endif
    //-----------------------------------------------------

    // Bind active texture maps (if available)
    for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        if (material.maps[i].texture.id > 0)
        {
            // Select current shader texture slot
            rlActiveTextureSlot(i);

            // Enable texture for active slot
            if ((i == MATERIAL_MAP_IRRADIANCE) ||
                (i == MATERIAL_MAP_PREFILTER) ||
                (i == MATERIAL_MAP_CUBEMAP)) rlEnableTextureCubemap(material.maps[i].texture.id);
            else rlEnableTexture(material.maps[i].texture.id);

            rlSetUniform(material.shader.locs[SHADER_LOC_MAP_DIFFUSE + i], &i, SHADER_UNIFORM_INT, 1);
        }
    }

    // Try binding vertex array objects (VAO) or use VBOs if not possible
    // WARNING: RL_UploadMesh() enables all vertex attributes available in mesh and sets default attribute values
    // for shader expected vertex attributes that are not provided by the mesh (i.e. colors)
    // This could be a dangerous approach because different meshes with different shaders can enable/disable some attributes
    if (!rlEnableVertexArray(mesh.vaoId))
    {
        // Bind mesh VBO data: vertex position (shader-location = 0)
        rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION], 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION]);

        // Bind mesh VBO data: vertex texcoords (shader-location = 1)
        rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01], 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01]);

        if (material.shader.locs[SHADER_LOC_VERTEX_NORMAL] != -1)
        {
            // Bind mesh VBO data: vertex normals (shader-location = 2)
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL], 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL]);
        }

        // Bind mesh VBO data: vertex colors (shader-location = 3, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_COLOR] != -1)
        {
            if (mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0)
            {
                rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR]);
                rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR], 4, RL_UNSIGNED_BYTE, 1, 0, 0);
                rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
            else
            {
                // Set default value for defined vertex attribute in shader but not provided by mesh
                // WARNING: It could result in GPU undefined behaviour
                float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                rlSetVertexAttributeDefault(material.shader.locs[SHADER_LOC_VERTEX_COLOR], value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
        }

        // Bind mesh VBO data: vertex tangents (shader-location = 4, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TANGENT] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT], 4, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT]);
        }

        // Bind mesh VBO data: vertex texcoords2 (shader-location = 5, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02], 2, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02]);
        }

#ifdef RL_SUPPORT_MESH_GPU_SKINNING
        // Bind mesh VBO data: vertex bone ids (shader-location = 6, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_BONEIDS] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEIDS]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEIDS], 4, RL_UNSIGNED_BYTE, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEIDS]);
        }
        
        // Bind mesh VBO data: vertex bone weights (shader-location = 7, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS], 4, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS]);
        }
#endif

        if (mesh.indices != NULL) rlEnableVertexBufferElement(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
    }

    int eyeCount = 1;
    if (rlIsStereoRenderEnabled()) eyeCount = 2;

    for (int eye = 0; eye < eyeCount; eye++)
    {
        // Calculate model-view-projection matrix (MVP)
        RL_Matrix matModelViewProjection = MatrixIdentity();
        if (eyeCount == 1) matModelViewProjection = MatrixMultiply(matModelView, matProjection);
        else
        {
            // Setup current eye viewport (half screen width)
            rlViewport(eye*rlGetFramebufferWidth()/2, 0, rlGetFramebufferWidth()/2, rlGetFramebufferHeight());
            matModelViewProjection = MatrixMultiply(MatrixMultiply(matModelView, rlGetMatrixViewOffsetStereo(eye)), rlGetMatrixProjectionStereo(eye));
        }

        // Send combined model-view-projection matrix to shader
        rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], matModelViewProjection);

        // Draw mesh
        if (mesh.indices != NULL) rlDrawVertexArrayElements(0, mesh.triangleCount*3, 0);
        else rlDrawVertexArray(0, mesh.vertexCount);
    }

    // Unbind all bound texture maps
    for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        if (material.maps[i].texture.id > 0)
        {
            // Select current shader texture slot
            rlActiveTextureSlot(i);

            // Disable texture for active slot
            if ((i == MATERIAL_MAP_IRRADIANCE) ||
                (i == MATERIAL_MAP_PREFILTER) ||
                (i == MATERIAL_MAP_CUBEMAP)) rlDisableTextureCubemap();
            else rlDisableTexture();
        }
    }

    // Disable all possible vertex array objects (or VBOs)
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Disable shader program
    rlDisableShader();

    // Restore rlgl internal modelview and projection matrices
    rlSetMatrixModelview(matView);
    rlSetMatrixProjection(matProjection);
#endif
}

// Draw multiple mesh instances with material and different transforms
void RL_DrawMeshInstanced(RL_Mesh mesh, RL_Material material, const RL_Matrix *transforms, int instances)
{
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    // Instancing required variables
    float16 *instanceTransforms = NULL;
    unsigned int instancesVboId = 0;

    // Bind shader program
    rlEnableShader(material.shader.id);

    // Send required data to shader (matrices, values)
    //-----------------------------------------------------
    // Upload to shader material.colDiffuse
    if (material.shader.locs[SHADER_LOC_COLOR_DIFFUSE] != -1)
    {
        float values[4] = {
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.r/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.g/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.b/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.a/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_DIFFUSE], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Upload to shader material.colSpecular (if location available)
    if (material.shader.locs[SHADER_LOC_COLOR_SPECULAR] != -1)
    {
        float values[4] = {
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.r/255.0f,
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.g/255.0f,
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.b/255.0f,
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.a/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_SPECULAR], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Get a copy of current matrices to work with,
    // just in case stereo render is required, and we need to modify them
    // NOTE: At this point the modelview matrix just contains the view matrix (camera)
    // That's because RL_BeginMode3D() sets it and there is no model-drawing function
    // that modifies it, all use rlPushMatrix() and rlPopMatrix()
    RL_Matrix matModel = MatrixIdentity();
    RL_Matrix matView = rlGetMatrixModelview();
    RL_Matrix matModelView = MatrixIdentity();
    RL_Matrix matProjection = rlGetMatrixProjection();

    // Upload view and projection matrices (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_VIEW] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_VIEW], matView);
    if (material.shader.locs[SHADER_LOC_MATRIX_PROJECTION] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_PROJECTION], matProjection);

    // Create instances buffer
    instanceTransforms = (float16 *)RL_MALLOC(instances*sizeof(float16));

    // Fill buffer with instances transformations as float16 arrays
    for (int i = 0; i < instances; i++) instanceTransforms[i] = MatrixToFloatV(transforms[i]);

    // Enable mesh VAO to attach new buffer
    rlEnableVertexArray(mesh.vaoId);

    // This could alternatively use a static VBO and either glMapBuffer() or glBufferSubData()
    // It isn't clear which would be reliably faster in all cases and on all platforms,
    // anecdotally glMapBuffer() seems very slow (syncs) while glBufferSubData() seems
    // no faster, since we're transferring all the transform matrices anyway
    instancesVboId = rlLoadVertexBuffer(instanceTransforms, instances*sizeof(float16), false);

    // Instances transformation matrices are send to shader attribute location: SHADER_LOC_MATRIX_MODEL
    for (unsigned int i = 0; i < 4; i++)
    {
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_MATRIX_MODEL] + i);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_MATRIX_MODEL] + i, 4, RL_FLOAT, 0, sizeof(RL_Matrix), i*sizeof(RL_Vector4));
        rlSetVertexAttributeDivisor(material.shader.locs[SHADER_LOC_MATRIX_MODEL] + i, 1);
    }

    rlDisableVertexBuffer();
    rlDisableVertexArray();

    // Accumulate internal matrix transform (push/pop) and view matrix
    // NOTE: In this case, model instance transformation must be computed in the shader
    matModelView = MatrixMultiply(rlGetMatrixTransform(), matView);

    // Upload model normal matrix (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_NORMAL] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_NORMAL], MatrixTranspose(MatrixInvert(matModel)));
    
#ifdef RL_SUPPORT_MESH_GPU_SKINNING
    // Upload Bone Transforms    
    if (material.shader.locs[SHADER_LOC_BONE_MATRICES] != -1 && mesh.boneMatrices)
    {
        rlSetUniformMatrices(material.shader.locs[SHADER_LOC_BONE_MATRICES], mesh.boneMatrices, mesh.boneCount);
    }
#endif
    
    //-----------------------------------------------------

    // Bind active texture maps (if available)
    for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        if (material.maps[i].texture.id > 0)
        {
            // Select current shader texture slot
            rlActiveTextureSlot(i);

            // Enable texture for active slot
            if ((i == MATERIAL_MAP_IRRADIANCE) ||
                (i == MATERIAL_MAP_PREFILTER) ||
                (i == MATERIAL_MAP_CUBEMAP)) rlEnableTextureCubemap(material.maps[i].texture.id);
            else rlEnableTexture(material.maps[i].texture.id);

            rlSetUniform(material.shader.locs[SHADER_LOC_MAP_DIFFUSE + i], &i, SHADER_UNIFORM_INT, 1);
        }
    }

    // Try binding vertex array objects (VAO)
    // or use VBOs if not possible
    if (!rlEnableVertexArray(mesh.vaoId))
    {
        // Bind mesh VBO data: vertex position (shader-location = 0)
        rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION], 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION]);

        // Bind mesh VBO data: vertex texcoords (shader-location = 1)
        rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01], 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01]);

        if (material.shader.locs[SHADER_LOC_VERTEX_NORMAL] != -1)
        {
            // Bind mesh VBO data: vertex normals (shader-location = 2)
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL], 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL]);
        }

        // Bind mesh VBO data: vertex colors (shader-location = 3, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_COLOR] != -1)
        {
            if (mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0)
            {
                rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR]);
                rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR], 4, RL_UNSIGNED_BYTE, 1, 0, 0);
                rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
            else
            {
                // Set default value for unused attribute
                // NOTE: Required when using default shader and no VAO support
                float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                rlSetVertexAttributeDefault(material.shader.locs[SHADER_LOC_VERTEX_COLOR], value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
        }

        // Bind mesh VBO data: vertex tangents (shader-location = 4, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TANGENT] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT], 4, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT]);
        }

        // Bind mesh VBO data: vertex texcoords2 (shader-location = 5, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02], 2, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02]);
        }

#ifdef RL_SUPPORT_MESH_GPU_SKINNING
        // Bind mesh VBO data: vertex bone ids (shader-location = 6, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_BONEIDS] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEIDS]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEIDS], 4, RL_UNSIGNED_BYTE, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEIDS]);
        }
        
        // Bind mesh VBO data: vertex bone weights (shader-location = 7, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS], 4, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS]);
        }
#endif

        if (mesh.indices != NULL) rlEnableVertexBufferElement(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
    }

    int eyeCount = 1;
    if (rlIsStereoRenderEnabled()) eyeCount = 2;

    for (int eye = 0; eye < eyeCount; eye++)
    {
        // Calculate model-view-projection matrix (MVP)
        RL_Matrix matModelViewProjection = MatrixIdentity();
        if (eyeCount == 1) matModelViewProjection = MatrixMultiply(matModelView, matProjection);
        else
        {
            // Setup current eye viewport (half screen width)
            rlViewport(eye*rlGetFramebufferWidth()/2, 0, rlGetFramebufferWidth()/2, rlGetFramebufferHeight());
            matModelViewProjection = MatrixMultiply(MatrixMultiply(matModelView, rlGetMatrixViewOffsetStereo(eye)), rlGetMatrixProjectionStereo(eye));
        }

        // Send combined model-view-projection matrix to shader
        rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], matModelViewProjection);

        // Draw mesh instanced
        if (mesh.indices != NULL) rlDrawVertexArrayElementsInstanced(0, mesh.triangleCount*3, 0, instances);
        else rlDrawVertexArrayInstanced(0, mesh.vertexCount, instances);
    }

    // Unbind all bound texture maps
    for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        if (material.maps[i].texture.id > 0)
        {
            // Select current shader texture slot
            rlActiveTextureSlot(i);

            // Disable texture for active slot
            if ((i == MATERIAL_MAP_IRRADIANCE) ||
                (i == MATERIAL_MAP_PREFILTER) ||
                (i == MATERIAL_MAP_CUBEMAP)) rlDisableTextureCubemap();
            else rlDisableTexture();
        }
    }

    // Disable all possible vertex array objects (or VBOs)
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Disable shader program
    rlDisableShader();

    // Remove instance transforms buffer
    rlUnloadVertexBuffer(instancesVboId);
    RL_FREE(instanceTransforms);
#endif
}

// Unload mesh from memory (RAM and VRAM)
void RL_UnloadMesh(RL_Mesh mesh)
{
    // Unload rlgl mesh vboId data
    rlUnloadVertexArray(mesh.vaoId);

    if (mesh.vboId != NULL) for (int i = 0; i < MAX_MESH_VERTEX_BUFFERS; i++) rlUnloadVertexBuffer(mesh.vboId[i]);
    RL_FREE(mesh.vboId);

    RL_FREE(mesh.vertices);
    RL_FREE(mesh.texcoords);
    RL_FREE(mesh.normals);
    RL_FREE(mesh.colors);
    RL_FREE(mesh.tangents);
    RL_FREE(mesh.texcoords2);
    RL_FREE(mesh.indices);

    RL_FREE(mesh.animVertices);
    RL_FREE(mesh.animNormals);
    RL_FREE(mesh.boneWeights);
    RL_FREE(mesh.boneIds);
    RL_FREE(mesh.boneMatrices);
}

// Export mesh data to file
bool RL_ExportMesh(RL_Mesh mesh, const char *fileName)
{
    bool success = false;

    if (RL_IsFileExtension(fileName, ".obj"))
    {
        // Estimated data size, it should be enough...
        int dataSize = mesh.vertexCount*(int)strlen("v 0000.00f 0000.00f 0000.00f") +
                       mesh.vertexCount*(int)strlen("vt 0.000f 0.00f") +
                       mesh.vertexCount*(int)strlen("vn 0.000f 0.00f 0.00f") +
                       mesh.triangleCount*(int)strlen("f 00000/00000/00000 00000/00000/00000 00000/00000/00000");

        // NOTE: Text data buffer size is estimated considering mesh data size
        char *txtData = (char *)RL_CALLOC(dataSize*2 + 2000, sizeof(char));

        int byteCount = 0;
        byteCount += sprintf(txtData + byteCount, "# //////////////////////////////////////////////////////////////////////////////////\n");
        byteCount += sprintf(txtData + byteCount, "# //                                                                              //\n");
        byteCount += sprintf(txtData + byteCount, "# // rMeshOBJ exporter v1.0 - RL_Mesh exported as triangle faces and not optimized   //\n");
        byteCount += sprintf(txtData + byteCount, "# //                                                                              //\n");
        byteCount += sprintf(txtData + byteCount, "# // more info and bugs-report:  github.com/raysan5/raylib                        //\n");
        byteCount += sprintf(txtData + byteCount, "# // feedback and support:       ray[at]raylib.com                                //\n");
        byteCount += sprintf(txtData + byteCount, "# //                                                                              //\n");
        byteCount += sprintf(txtData + byteCount, "# // Copyright (c) 2018-2024 Ramon Santamaria (@raysan5)                          //\n");
        byteCount += sprintf(txtData + byteCount, "# //                                                                              //\n");
        byteCount += sprintf(txtData + byteCount, "# //////////////////////////////////////////////////////////////////////////////////\n\n");
        byteCount += sprintf(txtData + byteCount, "# Vertex Count:     %i\n", mesh.vertexCount);
        byteCount += sprintf(txtData + byteCount, "# Triangle Count:   %i\n\n", mesh.triangleCount);

        byteCount += sprintf(txtData + byteCount, "g mesh\n");

        for (int i = 0, v = 0; i < mesh.vertexCount; i++, v += 3)
        {
            byteCount += sprintf(txtData + byteCount, "v %.2f %.2f %.2f\n", mesh.vertices[v], mesh.vertices[v + 1], mesh.vertices[v + 2]);
        }

        for (int i = 0, v = 0; i < mesh.vertexCount; i++, v += 2)
        {
            byteCount += sprintf(txtData + byteCount, "vt %.3f %.3f\n", mesh.texcoords[v], mesh.texcoords[v + 1]);
        }

        for (int i = 0, v = 0; i < mesh.vertexCount; i++, v += 3)
        {
            byteCount += sprintf(txtData + byteCount, "vn %.3f %.3f %.3f\n", mesh.normals[v], mesh.normals[v + 1], mesh.normals[v + 2]);
        }

        if (mesh.indices != NULL)
        {
            for (int i = 0, v = 0; i < mesh.triangleCount; i++, v += 3)
            {
                byteCount += sprintf(txtData + byteCount, "f %i/%i/%i %i/%i/%i %i/%i/%i\n",
                    mesh.indices[v] + 1, mesh.indices[v] + 1, mesh.indices[v] + 1,
                    mesh.indices[v + 1] + 1, mesh.indices[v + 1] + 1, mesh.indices[v + 1] + 1,
                    mesh.indices[v + 2] + 1, mesh.indices[v + 2] + 1, mesh.indices[v + 2] + 1);
            }
        }
        else
        {
            for (int i = 0, v = 1; i < mesh.triangleCount; i++, v += 3)
            {
                byteCount += sprintf(txtData + byteCount, "f %i/%i/%i %i/%i/%i %i/%i/%i\n", v, v, v, v + 1, v + 1, v + 1, v + 2, v + 2, v + 2);
            }
        }

        byteCount += sprintf(txtData + byteCount, "\n");

        // NOTE: Text data length exported is determined by '\0' (NULL) character
        success = RL_SaveFileText(fileName, txtData);

        RL_FREE(txtData);
    }
    else if (RL_IsFileExtension(fileName, ".raw"))
    {
        // TODO: Support additional file formats to export mesh vertex data
    }

    return success;
}

// Export mesh as code file (.h) defining multiple arrays of vertex attributes
bool RL_ExportMeshAsCode(RL_Mesh mesh, const char *fileName)
{
    bool success = false;

#ifndef TEXT_BYTES_PER_LINE
    #define TEXT_BYTES_PER_LINE     20
#endif

    // NOTE: Text data buffer size is fixed to 64MB
    char *txtData = (char *)RL_CALLOC(64*1024*1024, sizeof(char));  // 64 MB

    int byteCount = 0;
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// MeshAsCode exporter v1.0 - RL_Mesh vertex data exported as arrays                     //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// more info and bugs-report:  github.com/raysan5/raylib                              //\n");
    byteCount += sprintf(txtData + byteCount, "// feedback and support:       ray[at]raylib.com                                      //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// Copyright (c) 2023 Ramon Santamaria (@raysan5)                                     //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n\n");

    // Get file name from path and convert variable name to uppercase
    char varFileName[256] = { 0 };
    strcpy(varFileName, RL_GetFileNameWithoutExt(fileName));
    for (int i = 0; varFileName[i] != '\0'; i++) if ((varFileName[i] >= 'a') && (varFileName[i] <= 'z')) { varFileName[i] = varFileName[i] - 32; }

    // Add image information
    byteCount += sprintf(txtData + byteCount, "// RL_Mesh basic information\n");
    byteCount += sprintf(txtData + byteCount, "#define %s_VERTEX_COUNT    %i\n", varFileName, mesh.vertexCount);
    byteCount += sprintf(txtData + byteCount, "#define %s_TRIANGLE_COUNT   %i\n\n", varFileName, mesh.triangleCount);

    // Define vertex attributes data as separate arrays
    //-----------------------------------------------------------------------------------------
    if (mesh.vertices != NULL)      // Vertex position (XYZ - 3 components per vertex - float)
    {
        byteCount += sprintf(txtData + byteCount, "static float %s_VERTEX_DATA[%i] = { ", varFileName, mesh.vertexCount*3);
        for (int i = 0; i < mesh.vertexCount*3 - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "%.3ff,\n" : "%.3ff, "), mesh.vertices[i]);
        byteCount += sprintf(txtData + byteCount, "%.3ff };\n\n", mesh.vertices[mesh.vertexCount*3 - 1]);
    }

    if (mesh.texcoords != NULL)      // Vertex texture coordinates (UV - 2 components per vertex - float)
    {
        byteCount += sprintf(txtData + byteCount, "static float %s_TEXCOORD_DATA[%i] = { ", varFileName, mesh.vertexCount*2);
        for (int i = 0; i < mesh.vertexCount*2 - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "%.3ff,\n" : "%.3ff, "), mesh.texcoords[i]);
        byteCount += sprintf(txtData + byteCount, "%.3ff };\n\n", mesh.texcoords[mesh.vertexCount*2 - 1]);
    }

    if (mesh.texcoords2 != NULL)      // Vertex texture coordinates (UV - 2 components per vertex - float)
    {
        byteCount += sprintf(txtData + byteCount, "static float %s_TEXCOORD2_DATA[%i] = { ", varFileName, mesh.vertexCount*2);
        for (int i = 0; i < mesh.vertexCount*2 - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "%.3ff,\n" : "%.3ff, "), mesh.texcoords2[i]);
        byteCount += sprintf(txtData + byteCount, "%.3ff };\n\n", mesh.texcoords2[mesh.vertexCount*2 - 1]);
    }

    if (mesh.normals != NULL)      // Vertex normals (XYZ - 3 components per vertex - float)
    {
        byteCount += sprintf(txtData + byteCount, "static float %s_NORMAL_DATA[%i] = { ", varFileName, mesh.vertexCount*3);
        for (int i = 0; i < mesh.vertexCount*3 - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "%.3ff,\n" : "%.3ff, "), mesh.normals[i]);
        byteCount += sprintf(txtData + byteCount, "%.3ff };\n\n", mesh.normals[mesh.vertexCount*3 - 1]);
    }

    if (mesh.tangents != NULL)      // Vertex tangents (XYZW - 4 components per vertex - float)
    {
        byteCount += sprintf(txtData + byteCount, "static float %s_TANGENT_DATA[%i] = { ", varFileName, mesh.vertexCount*4);
        for (int i = 0; i < mesh.vertexCount*4 - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "%.3ff,\n" : "%.3ff, "), mesh.tangents[i]);
        byteCount += sprintf(txtData + byteCount, "%.3ff };\n\n", mesh.tangents[mesh.vertexCount*4 - 1]);
    }

    if (mesh.colors != NULL)        // Vertex colors (RGBA - 4 components per vertex - unsigned char)
    {
        byteCount += sprintf(txtData + byteCount, "static unsigned char %s_COLOR_DATA[%i] = { ", varFileName, mesh.vertexCount*4);
        for (int i = 0; i < mesh.vertexCount*4 - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "0x%x,\n" : "0x%x, "), mesh.colors[i]);
        byteCount += sprintf(txtData + byteCount, "0x%x };\n\n", mesh.colors[mesh.vertexCount*4 - 1]);
    }

    if (mesh.indices != NULL)       // Vertex indices (3 index per triangle - unsigned short)
    {
        byteCount += sprintf(txtData + byteCount, "static unsigned short %s_INDEX_DATA[%i] = { ", varFileName, mesh.triangleCount*3);
        for (int i = 0; i < mesh.triangleCount*3 - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "%i,\n" : "%i, "), mesh.indices[i]);
        byteCount += sprintf(txtData + byteCount, "%i };\n", mesh.indices[mesh.triangleCount*3 - 1]);
    }
    //-----------------------------------------------------------------------------------------

    // NOTE: Text data size exported is determined by '\0' (NULL) character
    success = RL_SaveFileText(fileName, txtData);

    RL_FREE(txtData);

    //if (success != 0) TRACELOG(LOG_INFO, "FILEIO: [%s] RL_Image as code exported successfully", fileName);
    //else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to export image as code", fileName);

    return success;
}


#if defined(SUPPORT_FILEFORMAT_OBJ) || defined(SUPPORT_FILEFORMAT_MTL)
// Process obj materials
static void ProcessMaterialsOBJ(RL_Material *materials, tinyobj_material_t *mats, int materialCount)
{
    // Init model mats
    for (int m = 0; m < materialCount; m++)
    {
        // Init material to default
        // NOTE: Uses default shader, which only supports MATERIAL_MAP_DIFFUSE
        materials[m] = RL_LoadMaterialDefault();

        if (mats == NULL) continue;

        // Get default texture, in case no texture is defined
        // NOTE: rlgl default texture is a 1x1 pixel UNCOMPRESSED_R8G8B8A8
        materials[m].maps[MATERIAL_MAP_DIFFUSE].texture = (RL_Texture2D){ rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };

        if (mats[m].diffuse_texname != NULL) materials[m].maps[MATERIAL_MAP_DIFFUSE].texture = RL_LoadTexture(mats[m].diffuse_texname);  //char *diffuse_texname; // map_Kd
        else materials[m].maps[MATERIAL_MAP_DIFFUSE].color = (RL_Color){ (unsigned char)(mats[m].diffuse[0]*255.0f), (unsigned char)(mats[m].diffuse[1]*255.0f), (unsigned char)(mats[m].diffuse[2]*255.0f), 255 }; //float diffuse[3];
        materials[m].maps[MATERIAL_MAP_DIFFUSE].value = 0.0f;

        if (mats[m].specular_texname != NULL) materials[m].maps[MATERIAL_MAP_SPECULAR].texture = RL_LoadTexture(mats[m].specular_texname);  //char *specular_texname; // map_Ks
        materials[m].maps[MATERIAL_MAP_SPECULAR].color = (RL_Color){ (unsigned char)(mats[m].specular[0]*255.0f), (unsigned char)(mats[m].specular[1]*255.0f), (unsigned char)(mats[m].specular[2]*255.0f), 255 }; //float specular[3];
        materials[m].maps[MATERIAL_MAP_SPECULAR].value = 0.0f;

        if (mats[m].bump_texname != NULL) materials[m].maps[MATERIAL_MAP_NORMAL].texture = RL_LoadTexture(mats[m].bump_texname);  //char *bump_texname; // map_bump, bump
        materials[m].maps[MATERIAL_MAP_NORMAL].color = RL_WHITE;
        materials[m].maps[MATERIAL_MAP_NORMAL].value = mats[m].shininess;

        materials[m].maps[MATERIAL_MAP_EMISSION].color = (RL_Color){ (unsigned char)(mats[m].emission[0]*255.0f), (unsigned char)(mats[m].emission[1]*255.0f), (unsigned char)(mats[m].emission[2]*255.0f), 255 }; //float emission[3];

        if (mats[m].displacement_texname != NULL) materials[m].maps[MATERIAL_MAP_HEIGHT].texture = RL_LoadTexture(mats[m].displacement_texname);  //char *displacement_texname; // disp
    }
}
#endif

// Load materials from model file
RL_Material *RL_LoadMaterials(const char *fileName, int *materialCount)
{
    RL_Material *materials = NULL;
    unsigned int count = 0;

    // TODO: Support IQM and GLTF for materials parsing

#if defined(SUPPORT_FILEFORMAT_MTL)
    if (RL_IsFileExtension(fileName, ".mtl"))
    {
        tinyobj_material_t *mats = NULL;

        int result = tinyobj_parse_mtl_file(&mats, &count, fileName);
        if (result != TINYOBJ_SUCCESS) TRACELOG(LOG_WARNING, "MATERIAL: [%s] Failed to parse materials file", fileName);

        materials = RL_MALLOC(count*sizeof(RL_Material));
        ProcessMaterialsOBJ(materials, mats, count);

        tinyobj_materials_free(mats, count);
    }
#else
    TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to load material file", fileName);
#endif

    *materialCount = count;
    return materials;
}

// Load default material (Supports: DIFFUSE, SPECULAR, NORMAL maps)
RL_Material RL_LoadMaterialDefault(void)
{
    RL_Material material = { 0 };
    material.maps = (RL_MaterialMap *)RL_CALLOC(MAX_MATERIAL_MAPS, sizeof(RL_MaterialMap));

    // Using rlgl default shader
    material.shader.id = rlGetShaderIdDefault();
    material.shader.locs = rlGetShaderLocsDefault();

    // Using rlgl default texture (1x1 pixel, UNCOMPRESSED_R8G8B8A8, 1 mipmap)
    material.maps[MATERIAL_MAP_DIFFUSE].texture = (RL_Texture2D){ rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    //material.maps[MATERIAL_MAP_NORMAL].texture;         // NOTE: By default, not set
    //material.maps[MATERIAL_MAP_SPECULAR].texture;       // NOTE: By default, not set

    material.maps[MATERIAL_MAP_DIFFUSE].color = RL_WHITE;    // Diffuse color
    material.maps[MATERIAL_MAP_SPECULAR].color = RL_WHITE;   // Specular color

    return material;
}

// Check if a material is ready
bool RL_IsMaterialReady(RL_Material material)
{
    bool result = false;

    if ((material.maps != NULL) &&      // Validate material contain some map
        (material.shader.id > 0)) result = true; // Validate material shader is valid

    return result;
}

// Unload material from memory
void RL_UnloadMaterial(RL_Material material)
{
    // Unload material shader (avoid unloading default shader, managed by raylib)
    if (material.shader.id != rlGetShaderIdDefault()) RL_UnloadShader(material.shader);

    // Unload loaded texture maps (avoid unloading default texture, managed by raylib)
    if (material.maps != NULL)
    {
        for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
        {
            if (material.maps[i].texture.id != rlGetTextureIdDefault()) rlUnloadTexture(material.maps[i].texture.id);
        }
    }

    RL_FREE(material.maps);
}

// Set texture for a material map type (MATERIAL_MAP_DIFFUSE, MATERIAL_MAP_SPECULAR...)
// NOTE: Previous texture should be manually unloaded
void RL_SetMaterialTexture(RL_Material *material, int mapType, RL_Texture2D texture)
{
    material->maps[mapType].texture = texture;
}

// Set the material for a mesh
void RL_SetModelMeshMaterial(RL_Model *model, int meshId, int materialId)
{
    if (meshId >= model->meshCount) TRACELOG(LOG_WARNING, "MESH: Id greater than mesh count");
    else if (materialId >= model->materialCount) TRACELOG(LOG_WARNING, "MATERIAL: Id greater than material count");
    else  model->meshMaterial[meshId] = materialId;
}

// Load model animations from file
RL_ModelAnimation *RL_LoadModelAnimations(const char *fileName, int *animCount)
{
    RL_ModelAnimation *animations = NULL;

#if defined(SUPPORT_FILEFORMAT_IQM)
    if (RL_IsFileExtension(fileName, ".iqm")) animations = LoadModelAnimationsIQM(fileName, animCount);
#endif
#if defined(SUPPORT_FILEFORMAT_M3D)
    if (RL_IsFileExtension(fileName, ".m3d")) animations = LoadModelAnimationsM3D(fileName, animCount);
#endif
#if defined(SUPPORT_FILEFORMAT_GLTF)
    if (RL_IsFileExtension(fileName, ".gltf;.glb")) animations = LoadModelAnimationsGLTF(fileName, animCount);
#endif

    return animations;
}

// Update model animated vertex data (positions and normals) for a given frame
// NOTE: Updated data is uploaded to GPU
void RL_UpdateModelAnimation(RL_Model model, RL_ModelAnimation anim, int frame)
{
    if ((anim.frameCount > 0) && (anim.bones != NULL) && (anim.framePoses != NULL))
    {
        if (frame >= anim.frameCount) frame = frame%anim.frameCount;

        for (int m = 0; m < model.meshCount; m++)
        {
            RL_Mesh mesh = model.meshes[m];

            if (mesh.boneIds == NULL || mesh.boneWeights == NULL)
            {
                TRACELOG(LOG_WARNING, "MODEL: RL_UpdateModelAnimation(): RL_Mesh %i has no connection to bones", m);
                continue;
            }

            bool updated = false;           // Flag to check when anim vertex information is updated
            RL_Vector3 animVertex = { 0 };
            RL_Vector3 animNormal = { 0 };

            RL_Vector3 inTranslation = { 0 };
            RL_Quaternion inRotation = { 0 };
            // RL_Vector3 inScale = { 0 };

            RL_Vector3 outTranslation = { 0 };
            RL_Quaternion outRotation = { 0 };
            RL_Vector3 outScale = { 0 };

            int boneId = 0;
            int boneCounter = 0;
            float boneWeight = 0.0;

            const int vValues = mesh.vertexCount*3;
            for (int vCounter = 0; vCounter < vValues; vCounter += 3)
            {
                mesh.animVertices[vCounter] = 0;
                mesh.animVertices[vCounter + 1] = 0;
                mesh.animVertices[vCounter + 2] = 0;

                if (mesh.animNormals != NULL)
                {
                    mesh.animNormals[vCounter] = 0;
                    mesh.animNormals[vCounter + 1] = 0;
                    mesh.animNormals[vCounter + 2] = 0;
                }

                // Iterates over 4 bones per vertex
                for (int j = 0; j < 4; j++, boneCounter++)
                {
                    boneWeight = mesh.boneWeights[boneCounter];

                    // Early stop when no transformation will be applied
                    if (boneWeight == 0.0f) continue;

                    boneId = mesh.boneIds[boneCounter];
                    //int boneIdParent = model.bones[boneId].parent;
                    inTranslation = model.bindPose[boneId].translation;
                    inRotation = model.bindPose[boneId].rotation;
                    //inScale = model.bindPose[boneId].scale;
                    outTranslation = anim.framePoses[frame][boneId].translation;
                    outRotation = anim.framePoses[frame][boneId].rotation;
                    outScale = anim.framePoses[frame][boneId].scale;

                    // Vertices processing
                    // NOTE: We use meshes.vertices (default vertex position) to calculate meshes.animVertices (animated vertex position)
                    animVertex = (RL_Vector3){ mesh.vertices[vCounter], mesh.vertices[vCounter + 1], mesh.vertices[vCounter + 2] };
                    animVertex = Vector3Subtract(animVertex, inTranslation);
                    animVertex = Vector3Multiply(animVertex, outScale);
                    animVertex = Vector3RotateByQuaternion(animVertex, QuaternionMultiply(outRotation, QuaternionInvert(inRotation)));
                    animVertex = Vector3Add(animVertex, outTranslation);
                    //animVertex = Vector3Transform(animVertex, model.transform);
                    mesh.animVertices[vCounter] += animVertex.x*boneWeight;
                    mesh.animVertices[vCounter + 1] += animVertex.y*boneWeight;
                    mesh.animVertices[vCounter + 2] += animVertex.z*boneWeight;
                    updated = true;

                    // Normals processing
                    // NOTE: We use meshes.baseNormals (default normal) to calculate meshes.normals (animated normals)
                    if (mesh.normals != NULL)
                    {
                        animNormal = (RL_Vector3){ mesh.normals[vCounter], mesh.normals[vCounter + 1], mesh.normals[vCounter + 2] };
                        animNormal = Vector3RotateByQuaternion(animNormal, QuaternionMultiply(outRotation, QuaternionInvert(inRotation)));
                        mesh.animNormals[vCounter] += animNormal.x*boneWeight;
                        mesh.animNormals[vCounter + 1] += animNormal.y*boneWeight;
                        mesh.animNormals[vCounter + 2] += animNormal.z*boneWeight;
                    }
                }
            }

            // Upload new vertex data to GPU for model drawing
            // NOTE: Only update data when values changed
            if (updated)
            {
                rlUpdateVertexBuffer(mesh.vboId[0], mesh.animVertices, mesh.vertexCount*3*sizeof(float), 0); // Update vertex position
                rlUpdateVertexBuffer(mesh.vboId[2], mesh.animNormals, mesh.vertexCount*3*sizeof(float), 0);  // Update vertex normals
            }
        }
    }
}

void UpdateModelAnimationBoneMatrices(RL_Model model, RL_ModelAnimation anim, int frame)
{
    if ((anim.frameCount > 0) && (anim.bones != NULL) && (anim.framePoses != NULL))
    {
        if (frame >= anim.frameCount) frame = frame%anim.frameCount;    
    
        for (int i = 0; i < model.meshCount; i++)
        {
            if (model.meshes[i].boneMatrices)
            {
                assert(model.meshes[i].boneCount == anim.boneCount);
              
                for (int boneId = 0; boneId < model.meshes[i].boneCount; boneId++)
                {
                    RL_Vector3 inTranslation = model.bindPose[boneId].translation;
                    RL_Quaternion inRotation = model.bindPose[boneId].rotation;
                    RL_Vector3 inScale = model.bindPose[boneId].scale;
                    
                    RL_Vector3 outTranslation = anim.framePoses[frame][boneId].translation;
                    RL_Quaternion outRotation = anim.framePoses[frame][boneId].rotation;
                    RL_Vector3 outScale = anim.framePoses[frame][boneId].scale;

                    RL_Vector3 invTranslation = Vector3RotateByQuaternion(Vector3Negate(inTranslation), QuaternionInvert(inRotation));
                    RL_Quaternion invRotation = QuaternionInvert(inRotation);
                    RL_Vector3 invScale = Vector3Divide((RL_Vector3){ 1.0f, 1.0f, 1.0f }, inScale);

                    RL_Vector3 boneTranslation = Vector3Add(
                        Vector3RotateByQuaternion(Vector3Multiply(outScale, invTranslation),
                        outRotation), outTranslation); 
                    RL_Quaternion boneRotation = QuaternionMultiply(outRotation, invRotation);
                    RL_Vector3 boneScale = Vector3Multiply(outScale, invScale);
                    
                    RL_Matrix boneMatrix = MatrixMultiply(MatrixMultiply(
                        QuaternionToMatrix(boneRotation),
                        MatrixTranslate(boneTranslation.x, boneTranslation.y, boneTranslation.z)),
                        MatrixScale(boneScale.x, boneScale.y, boneScale.z));
                    
                    model.meshes[i].boneMatrices[boneId] = boneMatrix;
                }
            }
        }
    }
}

// Unload animation array data
void RL_UnloadModelAnimations(RL_ModelAnimation *animations, int animCount)
{
    for (int i = 0; i < animCount; i++) RL_UnloadModelAnimation(animations[i]);
    RL_FREE(animations);
}

// Unload animation data
void RL_UnloadModelAnimation(RL_ModelAnimation anim)
{
    for (int i = 0; i < anim.frameCount; i++) RL_FREE(anim.framePoses[i]);

    RL_FREE(anim.bones);
    RL_FREE(anim.framePoses);
}

// Check model animation skeleton match
// NOTE: Only number of bones and parent connections are checked
bool RL_IsModelAnimationValid(RL_Model model, RL_ModelAnimation anim)
{
    int result = true;

    if (model.boneCount != anim.boneCount) result = false;
    else
    {
        for (int i = 0; i < model.boneCount; i++)
        {
            if (model.bones[i].parent != anim.bones[i].parent) { result = false; break; }
        }
    }

    return result;
}

#if defined(SUPPORT_MESH_GENERATION)
// Generate polygonal mesh
RL_Mesh RL_GenMeshPoly(int sides, float radius)
{
    RL_Mesh mesh = { 0 };

    if (sides < 3) return mesh; // Security check

    int vertexCount = sides*3;

    // Vertices definition
    RL_Vector3 *vertices = (RL_Vector3 *)RL_MALLOC(vertexCount*sizeof(RL_Vector3));

    float d = 0.0f, dStep = 360.0f/sides;
    for (int v = 0; v < vertexCount - 2; v += 3)
    {
        vertices[v] = (RL_Vector3){ 0.0f, 0.0f, 0.0f };
        vertices[v + 1] = (RL_Vector3){ sinf(DEG2RAD*d)*radius, 0.0f, cosf(DEG2RAD*d)*radius };
        vertices[v + 2] = (RL_Vector3){ sinf(DEG2RAD*(d+dStep))*radius, 0.0f, cosf(DEG2RAD*(d+dStep))*radius };
        d += dStep;
    }

    // Normals definition
    RL_Vector3 *normals = (RL_Vector3 *)RL_MALLOC(vertexCount*sizeof(RL_Vector3));
    for (int n = 0; n < vertexCount; n++) normals[n] = (RL_Vector3){ 0.0f, 1.0f, 0.0f };   // RL_Vector3.up;

    // TexCoords definition
    RL_Vector2 *texcoords = (RL_Vector2 *)RL_MALLOC(vertexCount*sizeof(RL_Vector2));
    for (int n = 0; n < vertexCount; n++) texcoords[n] = (RL_Vector2){ 0.0f, 0.0f };

    mesh.vertexCount = vertexCount;
    mesh.triangleCount = sides;
    mesh.vertices = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.texcoords = (float *)RL_MALLOC(mesh.vertexCount*2*sizeof(float));
    mesh.normals = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));

    // RL_Mesh vertices position array
    for (int i = 0; i < mesh.vertexCount; i++)
    {
        mesh.vertices[3*i] = vertices[i].x;
        mesh.vertices[3*i + 1] = vertices[i].y;
        mesh.vertices[3*i + 2] = vertices[i].z;
    }

    // RL_Mesh texcoords array
    for (int i = 0; i < mesh.vertexCount; i++)
    {
        mesh.texcoords[2*i] = texcoords[i].x;
        mesh.texcoords[2*i + 1] = texcoords[i].y;
    }

    // RL_Mesh normals array
    for (int i = 0; i < mesh.vertexCount; i++)
    {
        mesh.normals[3*i] = normals[i].x;
        mesh.normals[3*i + 1] = normals[i].y;
        mesh.normals[3*i + 2] = normals[i].z;
    }

    RL_FREE(vertices);
    RL_FREE(normals);
    RL_FREE(texcoords);

    // Upload vertex data to GPU (static mesh)
    // NOTE: mesh.vboId array is allocated inside RL_UploadMesh()
    RL_UploadMesh(&mesh, false);

    return mesh;
}

// Generate plane mesh (with subdivisions)
RL_Mesh RL_GenMeshPlane(float width, float length, int resX, int resZ)
{
    RL_Mesh mesh = { 0 };

#define CUSTOM_MESH_GEN_PLANE
#if defined(CUSTOM_MESH_GEN_PLANE)
    resX++;
    resZ++;

    // Vertices definition
    int vertexCount = resX*resZ; // vertices get reused for the faces

    RL_Vector3 *vertices = (RL_Vector3 *)RL_MALLOC(vertexCount*sizeof(RL_Vector3));
    for (int z = 0; z < resZ; z++)
    {
        // [-length/2, length/2]
        float zPos = ((float)z/(resZ - 1) - 0.5f)*length;
        for (int x = 0; x < resX; x++)
        {
            // [-width/2, width/2]
            float xPos = ((float)x/(resX - 1) - 0.5f)*width;
            vertices[x + z*resX] = (RL_Vector3){ xPos, 0.0f, zPos };
        }
    }

    // Normals definition
    RL_Vector3 *normals = (RL_Vector3 *)RL_MALLOC(vertexCount*sizeof(RL_Vector3));
    for (int n = 0; n < vertexCount; n++) normals[n] = (RL_Vector3){ 0.0f, 1.0f, 0.0f };   // RL_Vector3.up;

    // TexCoords definition
    RL_Vector2 *texcoords = (RL_Vector2 *)RL_MALLOC(vertexCount*sizeof(RL_Vector2));
    for (int v = 0; v < resZ; v++)
    {
        for (int u = 0; u < resX; u++)
        {
            texcoords[u + v*resX] = (RL_Vector2){ (float)u/(resX - 1), (float)v/(resZ - 1) };
        }
    }

    // Triangles definition (indices)
    int numFaces = (resX - 1)*(resZ - 1);
    int *triangles = (int *)RL_MALLOC(numFaces*6*sizeof(int));
    int t = 0;
    for (int face = 0; face < numFaces; face++)
    {
        // Retrieve lower left corner from face ind
        int i = face + face/(resX - 1);

        triangles[t++] = i + resX;
        triangles[t++] = i + 1;
        triangles[t++] = i;

        triangles[t++] = i + resX;
        triangles[t++] = i + resX + 1;
        triangles[t++] = i + 1;
    }

    mesh.vertexCount = vertexCount;
    mesh.triangleCount = numFaces*2;
    mesh.vertices = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.texcoords = (float *)RL_MALLOC(mesh.vertexCount*2*sizeof(float));
    mesh.normals = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.indices = (unsigned short *)RL_MALLOC(mesh.triangleCount*3*sizeof(unsigned short));

    // RL_Mesh vertices position array
    for (int i = 0; i < mesh.vertexCount; i++)
    {
        mesh.vertices[3*i] = vertices[i].x;
        mesh.vertices[3*i + 1] = vertices[i].y;
        mesh.vertices[3*i + 2] = vertices[i].z;
    }

    // RL_Mesh texcoords array
    for (int i = 0; i < mesh.vertexCount; i++)
    {
        mesh.texcoords[2*i] = texcoords[i].x;
        mesh.texcoords[2*i + 1] = texcoords[i].y;
    }

    // RL_Mesh normals array
    for (int i = 0; i < mesh.vertexCount; i++)
    {
        mesh.normals[3*i] = normals[i].x;
        mesh.normals[3*i + 1] = normals[i].y;
        mesh.normals[3*i + 2] = normals[i].z;
    }

    // RL_Mesh indices array initialization
    for (int i = 0; i < mesh.triangleCount*3; i++) mesh.indices[i] = triangles[i];

    RL_FREE(vertices);
    RL_FREE(normals);
    RL_FREE(texcoords);
    RL_FREE(triangles);

#else       // Use par_shapes library to generate plane mesh

    par_shapes_mesh *plane = par_shapes_create_plane(resX, resZ);   // No normals/texcoords generated!!!
    par_shapes_scale(plane, width, length, 1.0f);
    par_shapes_rotate(plane, -PI/2.0f, (float[]){ 1, 0, 0 });
    par_shapes_translate(plane, -width/2, 0.0f, length/2);

    mesh.vertices = (float *)RL_MALLOC(plane->ntriangles*3*3*sizeof(float));
    mesh.texcoords = (float *)RL_MALLOC(plane->ntriangles*3*2*sizeof(float));
    mesh.normals = (float *)RL_MALLOC(plane->ntriangles*3*3*sizeof(float));

    mesh.vertexCount = plane->ntriangles*3;
    mesh.triangleCount = plane->ntriangles;

    for (int k = 0; k < mesh.vertexCount; k++)
    {
        mesh.vertices[k*3] = plane->points[plane->triangles[k]*3];
        mesh.vertices[k*3 + 1] = plane->points[plane->triangles[k]*3 + 1];
        mesh.vertices[k*3 + 2] = plane->points[plane->triangles[k]*3 + 2];

        mesh.normals[k*3] = plane->normals[plane->triangles[k]*3];
        mesh.normals[k*3 + 1] = plane->normals[plane->triangles[k]*3 + 1];
        mesh.normals[k*3 + 2] = plane->normals[plane->triangles[k]*3 + 2];

        mesh.texcoords[k*2] = plane->tcoords[plane->triangles[k]*2];
        mesh.texcoords[k*2 + 1] = plane->tcoords[plane->triangles[k]*2 + 1];
    }

    par_shapes_free_mesh(plane);
#endif

    // Upload vertex data to GPU (static mesh)
    RL_UploadMesh(&mesh, false);

    return mesh;
}

// Generated cuboid mesh
RL_Mesh RL_GenMeshCube(float width, float height, float length)
{
    RL_Mesh mesh = { 0 };

#define CUSTOM_MESH_GEN_CUBE
#if defined(CUSTOM_MESH_GEN_CUBE)
    float vertices[] = {
        -width/2, -height/2, length/2,
        width/2, -height/2, length/2,
        width/2, height/2, length/2,
        -width/2, height/2, length/2,
        -width/2, -height/2, -length/2,
        -width/2, height/2, -length/2,
        width/2, height/2, -length/2,
        width/2, -height/2, -length/2,
        -width/2, height/2, -length/2,
        -width/2, height/2, length/2,
        width/2, height/2, length/2,
        width/2, height/2, -length/2,
        -width/2, -height/2, -length/2,
        width/2, -height/2, -length/2,
        width/2, -height/2, length/2,
        -width/2, -height/2, length/2,
        width/2, -height/2, -length/2,
        width/2, height/2, -length/2,
        width/2, height/2, length/2,
        width/2, -height/2, length/2,
        -width/2, -height/2, -length/2,
        -width/2, -height/2, length/2,
        -width/2, height/2, length/2,
        -width/2, height/2, -length/2
    };

    float texcoords[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };

    float normals[] = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f,-1.0f,
        0.0f, 0.0f,-1.0f,
        0.0f, 0.0f,-1.0f,
        0.0f, 0.0f,-1.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f,-1.0f, 0.0f,
        0.0f,-1.0f, 0.0f,
        0.0f,-1.0f, 0.0f,
        0.0f,-1.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, 0.0f
    };

    mesh.vertices = (float *)RL_MALLOC(24*3*sizeof(float));
    memcpy(mesh.vertices, vertices, 24*3*sizeof(float));

    mesh.texcoords = (float *)RL_MALLOC(24*2*sizeof(float));
    memcpy(mesh.texcoords, texcoords, 24*2*sizeof(float));

    mesh.normals = (float *)RL_MALLOC(24*3*sizeof(float));
    memcpy(mesh.normals, normals, 24*3*sizeof(float));

    mesh.indices = (unsigned short *)RL_MALLOC(36*sizeof(unsigned short));

    int k = 0;

    // Indices can be initialized right now
    for (int i = 0; i < 36; i += 6)
    {
        mesh.indices[i] = 4*k;
        mesh.indices[i + 1] = 4*k + 1;
        mesh.indices[i + 2] = 4*k + 2;
        mesh.indices[i + 3] = 4*k;
        mesh.indices[i + 4] = 4*k + 2;
        mesh.indices[i + 5] = 4*k + 3;

        k++;
    }

    mesh.vertexCount = 24;
    mesh.triangleCount = 12;

#else               // Use par_shapes library to generate cube mesh
/*
// Platonic solids:
par_shapes_mesh* par_shapes_create_tetrahedron();       // 4 sides polyhedron (pyramid)
par_shapes_mesh* par_shapes_create_cube();              // 6 sides polyhedron (cube)
par_shapes_mesh* par_shapes_create_octahedron();        // 8 sides polyhedron (diamond)
par_shapes_mesh* par_shapes_create_dodecahedron();      // 12 sides polyhedron
par_shapes_mesh* par_shapes_create_icosahedron();       // 20 sides polyhedron
*/
    // Platonic solid generation: cube (6 sides)
    // NOTE: No normals/texcoords generated by default
    par_shapes_mesh *cube = par_shapes_create_cube();
    cube->tcoords = PAR_MALLOC(float, 2*cube->npoints);
    for (int i = 0; i < 2*cube->npoints; i++) cube->tcoords[i] = 0.0f;
    par_shapes_scale(cube, width, height, length);
    par_shapes_translate(cube, -width/2, 0.0f, -length/2);
    par_shapes_compute_normals(cube);

    mesh.vertices = (float *)RL_MALLOC(cube->ntriangles*3*3*sizeof(float));
    mesh.texcoords = (float *)RL_MALLOC(cube->ntriangles*3*2*sizeof(float));
    mesh.normals = (float *)RL_MALLOC(cube->ntriangles*3*3*sizeof(float));

    mesh.vertexCount = cube->ntriangles*3;
    mesh.triangleCount = cube->ntriangles;

    for (int k = 0; k < mesh.vertexCount; k++)
    {
        mesh.vertices[k*3] = cube->points[cube->triangles[k]*3];
        mesh.vertices[k*3 + 1] = cube->points[cube->triangles[k]*3 + 1];
        mesh.vertices[k*3 + 2] = cube->points[cube->triangles[k]*3 + 2];

        mesh.normals[k*3] = cube->normals[cube->triangles[k]*3];
        mesh.normals[k*3 + 1] = cube->normals[cube->triangles[k]*3 + 1];
        mesh.normals[k*3 + 2] = cube->normals[cube->triangles[k]*3 + 2];

        mesh.texcoords[k*2] = cube->tcoords[cube->triangles[k]*2];
        mesh.texcoords[k*2 + 1] = cube->tcoords[cube->triangles[k]*2 + 1];
    }

    par_shapes_free_mesh(cube);
#endif

    // Upload vertex data to GPU (static mesh)
    RL_UploadMesh(&mesh, false);

    return mesh;
}

// Generate sphere mesh (standard sphere)
RL_Mesh RL_GenMeshSphere(float radius, int rings, int slices)
{
    RL_Mesh mesh = { 0 };

    if ((rings >= 3) && (slices >= 3))
    {
        par_shapes_mesh *sphere = par_shapes_create_parametric_sphere(slices, rings);
        par_shapes_scale(sphere, radius, radius, radius);
        // NOTE: Soft normals are computed internally

        mesh.vertices = (float *)RL_MALLOC(sphere->ntriangles*3*3*sizeof(float));
        mesh.texcoords = (float *)RL_MALLOC(sphere->ntriangles*3*2*sizeof(float));
        mesh.normals = (float *)RL_MALLOC(sphere->ntriangles*3*3*sizeof(float));

        mesh.vertexCount = sphere->ntriangles*3;
        mesh.triangleCount = sphere->ntriangles;

        for (int k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = sphere->points[sphere->triangles[k]*3];
            mesh.vertices[k*3 + 1] = sphere->points[sphere->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = sphere->points[sphere->triangles[k]*3 + 2];

            mesh.normals[k*3] = sphere->normals[sphere->triangles[k]*3];
            mesh.normals[k*3 + 1] = sphere->normals[sphere->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = sphere->normals[sphere->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = sphere->tcoords[sphere->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = sphere->tcoords[sphere->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(sphere);

        // Upload vertex data to GPU (static mesh)
        RL_UploadMesh(&mesh, false);
    }
    else TRACELOG(LOG_WARNING, "MESH: Failed to generate mesh: sphere");

    return mesh;
}

// Generate hemisphere mesh (half sphere, no bottom cap)
RL_Mesh RL_GenMeshHemiSphere(float radius, int rings, int slices)
{
    RL_Mesh mesh = { 0 };

    if ((rings >= 3) && (slices >= 3))
    {
        if (radius < 0.0f) radius = 0.0f;

        par_shapes_mesh *sphere = par_shapes_create_hemisphere(slices, rings);
        par_shapes_scale(sphere, radius, radius, radius);
        // NOTE: Soft normals are computed internally

        mesh.vertices = (float *)RL_MALLOC(sphere->ntriangles*3*3*sizeof(float));
        mesh.texcoords = (float *)RL_MALLOC(sphere->ntriangles*3*2*sizeof(float));
        mesh.normals = (float *)RL_MALLOC(sphere->ntriangles*3*3*sizeof(float));

        mesh.vertexCount = sphere->ntriangles*3;
        mesh.triangleCount = sphere->ntriangles;

        for (int k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = sphere->points[sphere->triangles[k]*3];
            mesh.vertices[k*3 + 1] = sphere->points[sphere->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = sphere->points[sphere->triangles[k]*3 + 2];

            mesh.normals[k*3] = sphere->normals[sphere->triangles[k]*3];
            mesh.normals[k*3 + 1] = sphere->normals[sphere->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = sphere->normals[sphere->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = sphere->tcoords[sphere->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = sphere->tcoords[sphere->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(sphere);

        // Upload vertex data to GPU (static mesh)
        RL_UploadMesh(&mesh, false);
    }
    else TRACELOG(LOG_WARNING, "MESH: Failed to generate mesh: hemisphere");

    return mesh;
}

// Generate cylinder mesh
RL_Mesh RL_GenMeshCylinder(float radius, float height, int slices)
{
    RL_Mesh mesh = { 0 };

    if (slices >= 3)
    {
        // Instance a cylinder that sits on the Z=0 plane using the given tessellation
        // levels across the UV domain.  Think of "slices" like a number of pizza
        // slices, and "stacks" like a number of stacked rings
        // Height and radius are both 1.0, but they can easily be changed with par_shapes_scale
        par_shapes_mesh *cylinder = par_shapes_create_cylinder(slices, 8);
        par_shapes_scale(cylinder, radius, radius, height);
        par_shapes_rotate(cylinder, -PI/2.0f, (float[]){ 1, 0, 0 });

        // Generate an orientable disk shape (top cap)
        par_shapes_mesh *capTop = par_shapes_create_disk(radius, slices, (float[]){ 0, 0, 0 }, (float[]){ 0, 0, 1 });
        capTop->tcoords = PAR_MALLOC(float, 2*capTop->npoints);
        for (int i = 0; i < 2*capTop->npoints; i++) capTop->tcoords[i] = 0.0f;
        par_shapes_rotate(capTop, -PI/2.0f, (float[]){ 1, 0, 0 });
        par_shapes_rotate(capTop, 90*DEG2RAD, (float[]){ 0, 1, 0 });
        par_shapes_translate(capTop, 0, height, 0);

        // Generate an orientable disk shape (bottom cap)
        par_shapes_mesh *capBottom = par_shapes_create_disk(radius, slices, (float[]){ 0, 0, 0 }, (float[]){ 0, 0, -1 });
        capBottom->tcoords = PAR_MALLOC(float, 2*capBottom->npoints);
        for (int i = 0; i < 2*capBottom->npoints; i++) capBottom->tcoords[i] = 0.95f;
        par_shapes_rotate(capBottom, PI/2.0f, (float[]){ 1, 0, 0 });
        par_shapes_rotate(capBottom, -90*DEG2RAD, (float[]){ 0, 1, 0 });

        par_shapes_merge_and_free(cylinder, capTop);
        par_shapes_merge_and_free(cylinder, capBottom);

        mesh.vertices = (float *)RL_MALLOC(cylinder->ntriangles*3*3*sizeof(float));
        mesh.texcoords = (float *)RL_MALLOC(cylinder->ntriangles*3*2*sizeof(float));
        mesh.normals = (float *)RL_MALLOC(cylinder->ntriangles*3*3*sizeof(float));

        mesh.vertexCount = cylinder->ntriangles*3;
        mesh.triangleCount = cylinder->ntriangles;

        for (int k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = cylinder->points[cylinder->triangles[k]*3];
            mesh.vertices[k*3 + 1] = cylinder->points[cylinder->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = cylinder->points[cylinder->triangles[k]*3 + 2];

            mesh.normals[k*3] = cylinder->normals[cylinder->triangles[k]*3];
            mesh.normals[k*3 + 1] = cylinder->normals[cylinder->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = cylinder->normals[cylinder->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = cylinder->tcoords[cylinder->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = cylinder->tcoords[cylinder->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(cylinder);

        // Upload vertex data to GPU (static mesh)
        RL_UploadMesh(&mesh, false);
    }
    else TRACELOG(LOG_WARNING, "MESH: Failed to generate mesh: cylinder");

    return mesh;
}

// Generate cone/pyramid mesh
RL_Mesh RL_GenMeshCone(float radius, float height, int slices)
{
    RL_Mesh mesh = { 0 };

    if (slices >= 3)
    {
        // Instance a cone that sits on the Z=0 plane using the given tessellation
        // levels across the UV domain.  Think of "slices" like a number of pizza
        // slices, and "stacks" like a number of stacked rings
        // Height and radius are both 1.0, but they can easily be changed with par_shapes_scale
        par_shapes_mesh *cone = par_shapes_create_cone(slices, 8);
        par_shapes_scale(cone, radius, radius, height);
        par_shapes_rotate(cone, -PI/2.0f, (float[]){ 1, 0, 0 });
        par_shapes_rotate(cone, PI/2.0f, (float[]){ 0, 1, 0 });

        // Generate an orientable disk shape (bottom cap)
        par_shapes_mesh *capBottom = par_shapes_create_disk(radius, slices, (float[]){ 0, 0, 0 }, (float[]){ 0, 0, -1 });
        capBottom->tcoords = PAR_MALLOC(float, 2*capBottom->npoints);
        for (int i = 0; i < 2*capBottom->npoints; i++) capBottom->tcoords[i] = 0.95f;
        par_shapes_rotate(capBottom, PI/2.0f, (float[]){ 1, 0, 0 });

        par_shapes_merge_and_free(cone, capBottom);

        mesh.vertices = (float *)RL_MALLOC(cone->ntriangles*3*3*sizeof(float));
        mesh.texcoords = (float *)RL_MALLOC(cone->ntriangles*3*2*sizeof(float));
        mesh.normals = (float *)RL_MALLOC(cone->ntriangles*3*3*sizeof(float));

        mesh.vertexCount = cone->ntriangles*3;
        mesh.triangleCount = cone->ntriangles;

        for (int k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = cone->points[cone->triangles[k]*3];
            mesh.vertices[k*3 + 1] = cone->points[cone->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = cone->points[cone->triangles[k]*3 + 2];

            mesh.normals[k*3] = cone->normals[cone->triangles[k]*3];
            mesh.normals[k*3 + 1] = cone->normals[cone->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = cone->normals[cone->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = cone->tcoords[cone->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = cone->tcoords[cone->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(cone);

        // Upload vertex data to GPU (static mesh)
        RL_UploadMesh(&mesh, false);
    }
    else TRACELOG(LOG_WARNING, "MESH: Failed to generate mesh: cone");

    return mesh;
}

// Generate torus mesh
RL_Mesh RL_GenMeshTorus(float radius, float size, int radSeg, int sides)
{
    RL_Mesh mesh = { 0 };

    if ((sides >= 3) && (radSeg >= 3))
    {
        if (radius > 1.0f) radius = 1.0f;
        else if (radius < 0.1f) radius = 0.1f;

        // Create a donut that sits on the Z=0 plane with the specified inner radius
        // The outer radius can be controlled with par_shapes_scale
        par_shapes_mesh *torus = par_shapes_create_torus(radSeg, sides, radius);
        par_shapes_scale(torus, size/2, size/2, size/2);

        mesh.vertices = (float *)RL_MALLOC(torus->ntriangles*3*3*sizeof(float));
        mesh.texcoords = (float *)RL_MALLOC(torus->ntriangles*3*2*sizeof(float));
        mesh.normals = (float *)RL_MALLOC(torus->ntriangles*3*3*sizeof(float));

        mesh.vertexCount = torus->ntriangles*3;
        mesh.triangleCount = torus->ntriangles;

        for (int k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = torus->points[torus->triangles[k]*3];
            mesh.vertices[k*3 + 1] = torus->points[torus->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = torus->points[torus->triangles[k]*3 + 2];

            mesh.normals[k*3] = torus->normals[torus->triangles[k]*3];
            mesh.normals[k*3 + 1] = torus->normals[torus->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = torus->normals[torus->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = torus->tcoords[torus->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = torus->tcoords[torus->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(torus);

        // Upload vertex data to GPU (static mesh)
        RL_UploadMesh(&mesh, false);
    }
    else TRACELOG(LOG_WARNING, "MESH: Failed to generate mesh: torus");

    return mesh;
}

// Generate trefoil knot mesh
RL_Mesh RL_GenMeshKnot(float radius, float size, int radSeg, int sides)
{
    RL_Mesh mesh = { 0 };

    if ((sides >= 3) && (radSeg >= 3))
    {
        if (radius > 3.0f) radius = 3.0f;
        else if (radius < 0.5f) radius = 0.5f;

        par_shapes_mesh *knot = par_shapes_create_trefoil_knot(radSeg, sides, radius);
        par_shapes_scale(knot, size, size, size);

        mesh.vertices = (float *)RL_MALLOC(knot->ntriangles*3*3*sizeof(float));
        mesh.texcoords = (float *)RL_MALLOC(knot->ntriangles*3*2*sizeof(float));
        mesh.normals = (float *)RL_MALLOC(knot->ntriangles*3*3*sizeof(float));

        mesh.vertexCount = knot->ntriangles*3;
        mesh.triangleCount = knot->ntriangles;

        for (int k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = knot->points[knot->triangles[k]*3];
            mesh.vertices[k*3 + 1] = knot->points[knot->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = knot->points[knot->triangles[k]*3 + 2];

            mesh.normals[k*3] = knot->normals[knot->triangles[k]*3];
            mesh.normals[k*3 + 1] = knot->normals[knot->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = knot->normals[knot->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = knot->tcoords[knot->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = knot->tcoords[knot->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(knot);

        // Upload vertex data to GPU (static mesh)
        RL_UploadMesh(&mesh, false);
    }
    else TRACELOG(LOG_WARNING, "MESH: Failed to generate mesh: knot");

    return mesh;
}

// Generate a mesh from heightmap
// NOTE: Vertex data is uploaded to GPU
RL_Mesh RL_GenMeshHeightmap(RL_Image heightmap, RL_Vector3 size)
{
    #define GRAY_VALUE(c) ((float)(c.r + c.g + c.b)/3.0f)

    RL_Mesh mesh = { 0 };

    int mapX = heightmap.width;
    int mapZ = heightmap.height;

    RL_Color *pixels = RL_LoadImageColors(heightmap);

    // NOTE: One vertex per pixel
    mesh.triangleCount = (mapX - 1)*(mapZ - 1)*2;    // One quad every four pixels

    mesh.vertexCount = mesh.triangleCount*3;

    mesh.vertices = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.normals = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.texcoords = (float *)RL_MALLOC(mesh.vertexCount*2*sizeof(float));
    mesh.colors = NULL;

    int vCounter = 0;       // Used to count vertices float by float
    int tcCounter = 0;      // Used to count texcoords float by float
    int nCounter = 0;       // Used to count normals float by float

    RL_Vector3 scaleFactor = { size.x/(mapX - 1), size.y/255.0f, size.z/(mapZ - 1) };

    RL_Vector3 vA = { 0 };
    RL_Vector3 vB = { 0 };
    RL_Vector3 vC = { 0 };
    RL_Vector3 vN = { 0 };

    for (int z = 0; z < mapZ-1; z++)
    {
        for (int x = 0; x < mapX-1; x++)
        {
            // Fill vertices array with data
            //----------------------------------------------------------

            // one triangle - 3 vertex
            mesh.vertices[vCounter] = (float)x*scaleFactor.x;
            mesh.vertices[vCounter + 1] = GRAY_VALUE(pixels[x + z*mapX])*scaleFactor.y;
            mesh.vertices[vCounter + 2] = (float)z*scaleFactor.z;

            mesh.vertices[vCounter + 3] = (float)x*scaleFactor.x;
            mesh.vertices[vCounter + 4] = GRAY_VALUE(pixels[x + (z + 1)*mapX])*scaleFactor.y;
            mesh.vertices[vCounter + 5] = (float)(z + 1)*scaleFactor.z;

            mesh.vertices[vCounter + 6] = (float)(x + 1)*scaleFactor.x;
            mesh.vertices[vCounter + 7] = GRAY_VALUE(pixels[(x + 1) + z*mapX])*scaleFactor.y;
            mesh.vertices[vCounter + 8] = (float)z*scaleFactor.z;

            // Another triangle - 3 vertex
            mesh.vertices[vCounter + 9] = mesh.vertices[vCounter + 6];
            mesh.vertices[vCounter + 10] = mesh.vertices[vCounter + 7];
            mesh.vertices[vCounter + 11] = mesh.vertices[vCounter + 8];

            mesh.vertices[vCounter + 12] = mesh.vertices[vCounter + 3];
            mesh.vertices[vCounter + 13] = mesh.vertices[vCounter + 4];
            mesh.vertices[vCounter + 14] = mesh.vertices[vCounter + 5];

            mesh.vertices[vCounter + 15] = (float)(x + 1)*scaleFactor.x;
            mesh.vertices[vCounter + 16] = GRAY_VALUE(pixels[(x + 1) + (z + 1)*mapX])*scaleFactor.y;
            mesh.vertices[vCounter + 17] = (float)(z + 1)*scaleFactor.z;
            vCounter += 18;     // 6 vertex, 18 floats

            // Fill texcoords array with data
            //--------------------------------------------------------------
            mesh.texcoords[tcCounter] = (float)x/(mapX - 1);
            mesh.texcoords[tcCounter + 1] = (float)z/(mapZ - 1);

            mesh.texcoords[tcCounter + 2] = (float)x/(mapX - 1);
            mesh.texcoords[tcCounter + 3] = (float)(z + 1)/(mapZ - 1);

            mesh.texcoords[tcCounter + 4] = (float)(x + 1)/(mapX - 1);
            mesh.texcoords[tcCounter + 5] = (float)z/(mapZ - 1);

            mesh.texcoords[tcCounter + 6] = mesh.texcoords[tcCounter + 4];
            mesh.texcoords[tcCounter + 7] = mesh.texcoords[tcCounter + 5];

            mesh.texcoords[tcCounter + 8] = mesh.texcoords[tcCounter + 2];
            mesh.texcoords[tcCounter + 9] = mesh.texcoords[tcCounter + 3];

            mesh.texcoords[tcCounter + 10] = (float)(x + 1)/(mapX - 1);
            mesh.texcoords[tcCounter + 11] = (float)(z + 1)/(mapZ - 1);
            tcCounter += 12;    // 6 texcoords, 12 floats

            // Fill normals array with data
            //--------------------------------------------------------------
            for (int i = 0; i < 18; i += 9)
            {
                vA.x = mesh.vertices[nCounter + i];
                vA.y = mesh.vertices[nCounter + i + 1];
                vA.z = mesh.vertices[nCounter + i + 2];

                vB.x = mesh.vertices[nCounter + i + 3];
                vB.y = mesh.vertices[nCounter + i + 4];
                vB.z = mesh.vertices[nCounter + i + 5];

                vC.x = mesh.vertices[nCounter + i + 6];
                vC.y = mesh.vertices[nCounter + i + 7];
                vC.z = mesh.vertices[nCounter + i + 8];

                vN = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(vB, vA), Vector3Subtract(vC, vA)));

                mesh.normals[nCounter + i] = vN.x;
                mesh.normals[nCounter + i + 1] = vN.y;
                mesh.normals[nCounter + i + 2] = vN.z;

                mesh.normals[nCounter + i + 3] = vN.x;
                mesh.normals[nCounter + i + 4] = vN.y;
                mesh.normals[nCounter + i + 5] = vN.z;

                mesh.normals[nCounter + i + 6] = vN.x;
                mesh.normals[nCounter + i + 7] = vN.y;
                mesh.normals[nCounter + i + 8] = vN.z;
            }

            nCounter += 18;     // 6 vertex, 18 floats
        }
    }

    RL_UnloadImageColors(pixels);  // Unload pixels color data

    // Upload vertex data to GPU (static mesh)
    RL_UploadMesh(&mesh, false);

    return mesh;
}

// Generate a cubes mesh from pixel data
// NOTE: Vertex data is uploaded to GPU
RL_Mesh RL_GenMeshCubicmap(RL_Image cubicmap, RL_Vector3 cubeSize)
{
    #define COLOR_EQUAL(col1, col2) ((col1.r == col2.r)&&(col1.g == col2.g)&&(col1.b == col2.b)&&(col1.a == col2.a))

    RL_Mesh mesh = { 0 };

    RL_Color *pixels = RL_LoadImageColors(cubicmap);

    // NOTE: Max possible number of triangles numCubes*(12 triangles by cube)
    int maxTriangles = cubicmap.width*cubicmap.height*12;

    int vCounter = 0;       // Used to count vertices
    int tcCounter = 0;      // Used to count texcoords
    int nCounter = 0;       // Used to count normals

    float w = cubeSize.x;
    float h = cubeSize.z;
    float h2 = cubeSize.y;

    RL_Vector3 *mapVertices = (RL_Vector3 *)RL_MALLOC(maxTriangles*3*sizeof(RL_Vector3));
    RL_Vector2 *mapTexcoords = (RL_Vector2 *)RL_MALLOC(maxTriangles*3*sizeof(RL_Vector2));
    RL_Vector3 *mapNormals = (RL_Vector3 *)RL_MALLOC(maxTriangles*3*sizeof(RL_Vector3));

    // Define the 6 normals of the cube, we will combine them accordingly later...
    RL_Vector3 n1 = { 1.0f, 0.0f, 0.0f };
    RL_Vector3 n2 = { -1.0f, 0.0f, 0.0f };
    RL_Vector3 n3 = { 0.0f, 1.0f, 0.0f };
    RL_Vector3 n4 = { 0.0f, -1.0f, 0.0f };
    RL_Vector3 n5 = { 0.0f, 0.0f, -1.0f };
    RL_Vector3 n6 = { 0.0f, 0.0f, 1.0f };

    // NOTE: We use texture rectangles to define different textures for top-bottom-front-back-right-left (6)
    typedef struct RectangleF {
        float x;
        float y;
        float width;
        float height;
    } RectangleF;

    RectangleF rightTexUV = { 0.0f, 0.0f, 0.5f, 0.5f };
    RectangleF leftTexUV = { 0.5f, 0.0f, 0.5f, 0.5f };
    RectangleF frontTexUV = { 0.0f, 0.0f, 0.5f, 0.5f };
    RectangleF backTexUV = { 0.5f, 0.0f, 0.5f, 0.5f };
    RectangleF topTexUV = { 0.0f, 0.5f, 0.5f, 0.5f };
    RectangleF bottomTexUV = { 0.5f, 0.5f, 0.5f, 0.5f };

    for (int z = 0; z < cubicmap.height; ++z)
    {
        for (int x = 0; x < cubicmap.width; ++x)
        {
            // Define the 8 vertex of the cube, we will combine them accordingly later...
            RL_Vector3 v1 = { w*(x - 0.5f), h2, h*(z - 0.5f) };
            RL_Vector3 v2 = { w*(x - 0.5f), h2, h*(z + 0.5f) };
            RL_Vector3 v3 = { w*(x + 0.5f), h2, h*(z + 0.5f) };
            RL_Vector3 v4 = { w*(x + 0.5f), h2, h*(z - 0.5f) };
            RL_Vector3 v5 = { w*(x + 0.5f), 0, h*(z - 0.5f) };
            RL_Vector3 v6 = { w*(x - 0.5f), 0, h*(z - 0.5f) };
            RL_Vector3 v7 = { w*(x - 0.5f), 0, h*(z + 0.5f) };
            RL_Vector3 v8 = { w*(x + 0.5f), 0, h*(z + 0.5f) };

            // We check pixel color to be RL_WHITE -> draw full cube
            if (COLOR_EQUAL(pixels[z*cubicmap.width + x], RL_WHITE))
            {
                // Define triangles and checking collateral cubes
                //------------------------------------------------

                // Define top triangles (2 tris, 6 vertex --> v1-v2-v3, v1-v3-v4)
                // WARNING: Not required for a RL_WHITE cubes, created to allow seeing the map from outside
                mapVertices[vCounter] = v1;
                mapVertices[vCounter + 1] = v2;
                mapVertices[vCounter + 2] = v3;
                mapVertices[vCounter + 3] = v1;
                mapVertices[vCounter + 4] = v3;
                mapVertices[vCounter + 5] = v4;
                vCounter += 6;

                mapNormals[nCounter] = n3;
                mapNormals[nCounter + 1] = n3;
                mapNormals[nCounter + 2] = n3;
                mapNormals[nCounter + 3] = n3;
                mapNormals[nCounter + 4] = n3;
                mapNormals[nCounter + 5] = n3;
                nCounter += 6;

                mapTexcoords[tcCounter] = (RL_Vector2){ topTexUV.x, topTexUV.y };
                mapTexcoords[tcCounter + 1] = (RL_Vector2){ topTexUV.x, topTexUV.y + topTexUV.height };
                mapTexcoords[tcCounter + 2] = (RL_Vector2){ topTexUV.x + topTexUV.width, topTexUV.y + topTexUV.height };
                mapTexcoords[tcCounter + 3] = (RL_Vector2){ topTexUV.x, topTexUV.y };
                mapTexcoords[tcCounter + 4] = (RL_Vector2){ topTexUV.x + topTexUV.width, topTexUV.y + topTexUV.height };
                mapTexcoords[tcCounter + 5] = (RL_Vector2){ topTexUV.x + topTexUV.width, topTexUV.y };
                tcCounter += 6;

                // Define bottom triangles (2 tris, 6 vertex --> v6-v8-v7, v6-v5-v8)
                mapVertices[vCounter] = v6;
                mapVertices[vCounter + 1] = v8;
                mapVertices[vCounter + 2] = v7;
                mapVertices[vCounter + 3] = v6;
                mapVertices[vCounter + 4] = v5;
                mapVertices[vCounter + 5] = v8;
                vCounter += 6;

                mapNormals[nCounter] = n4;
                mapNormals[nCounter + 1] = n4;
                mapNormals[nCounter + 2] = n4;
                mapNormals[nCounter + 3] = n4;
                mapNormals[nCounter + 4] = n4;
                mapNormals[nCounter + 5] = n4;
                nCounter += 6;

                mapTexcoords[tcCounter] = (RL_Vector2){ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y };
                mapTexcoords[tcCounter + 1] = (RL_Vector2){ bottomTexUV.x, bottomTexUV.y + bottomTexUV.height };
                mapTexcoords[tcCounter + 2] = (RL_Vector2){ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y + bottomTexUV.height };
                mapTexcoords[tcCounter + 3] = (RL_Vector2){ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y };
                mapTexcoords[tcCounter + 4] = (RL_Vector2){ bottomTexUV.x, bottomTexUV.y };
                mapTexcoords[tcCounter + 5] = (RL_Vector2){ bottomTexUV.x, bottomTexUV.y + bottomTexUV.height };
                tcCounter += 6;

                // Checking cube on bottom of current cube
                if (((z < cubicmap.height - 1) && COLOR_EQUAL(pixels[(z + 1)*cubicmap.width + x], RL_BLACK)) || (z == cubicmap.height - 1))
                {
                    // Define front triangles (2 tris, 6 vertex) --> v2 v7 v3, v3 v7 v8
                    // NOTE: Collateral occluded faces are not generated
                    mapVertices[vCounter] = v2;
                    mapVertices[vCounter + 1] = v7;
                    mapVertices[vCounter + 2] = v3;
                    mapVertices[vCounter + 3] = v3;
                    mapVertices[vCounter + 4] = v7;
                    mapVertices[vCounter + 5] = v8;
                    vCounter += 6;

                    mapNormals[nCounter] = n6;
                    mapNormals[nCounter + 1] = n6;
                    mapNormals[nCounter + 2] = n6;
                    mapNormals[nCounter + 3] = n6;
                    mapNormals[nCounter + 4] = n6;
                    mapNormals[nCounter + 5] = n6;
                    nCounter += 6;

                    mapTexcoords[tcCounter] = (RL_Vector2){ frontTexUV.x, frontTexUV.y };
                    mapTexcoords[tcCounter + 1] = (RL_Vector2){ frontTexUV.x, frontTexUV.y + frontTexUV.height };
                    mapTexcoords[tcCounter + 2] = (RL_Vector2){ frontTexUV.x + frontTexUV.width, frontTexUV.y };
                    mapTexcoords[tcCounter + 3] = (RL_Vector2){ frontTexUV.x + frontTexUV.width, frontTexUV.y };
                    mapTexcoords[tcCounter + 4] = (RL_Vector2){ frontTexUV.x, frontTexUV.y + frontTexUV.height };
                    mapTexcoords[tcCounter + 5] = (RL_Vector2){ frontTexUV.x + frontTexUV.width, frontTexUV.y + frontTexUV.height };
                    tcCounter += 6;
                }

                // Checking cube on top of current cube
                if (((z > 0) && COLOR_EQUAL(pixels[(z - 1)*cubicmap.width + x], RL_BLACK)) || (z == 0))
                {
                    // Define back triangles (2 tris, 6 vertex) --> v1 v5 v6, v1 v4 v5
                    // NOTE: Collateral occluded faces are not generated
                    mapVertices[vCounter] = v1;
                    mapVertices[vCounter + 1] = v5;
                    mapVertices[vCounter + 2] = v6;
                    mapVertices[vCounter + 3] = v1;
                    mapVertices[vCounter + 4] = v4;
                    mapVertices[vCounter + 5] = v5;
                    vCounter += 6;

                    mapNormals[nCounter] = n5;
                    mapNormals[nCounter + 1] = n5;
                    mapNormals[nCounter + 2] = n5;
                    mapNormals[nCounter + 3] = n5;
                    mapNormals[nCounter + 4] = n5;
                    mapNormals[nCounter + 5] = n5;
                    nCounter += 6;

                    mapTexcoords[tcCounter] = (RL_Vector2){ backTexUV.x + backTexUV.width, backTexUV.y };
                    mapTexcoords[tcCounter + 1] = (RL_Vector2){ backTexUV.x, backTexUV.y + backTexUV.height };
                    mapTexcoords[tcCounter + 2] = (RL_Vector2){ backTexUV.x + backTexUV.width, backTexUV.y + backTexUV.height };
                    mapTexcoords[tcCounter + 3] = (RL_Vector2){ backTexUV.x + backTexUV.width, backTexUV.y };
                    mapTexcoords[tcCounter + 4] = (RL_Vector2){ backTexUV.x, backTexUV.y };
                    mapTexcoords[tcCounter + 5] = (RL_Vector2){ backTexUV.x, backTexUV.y + backTexUV.height };
                    tcCounter += 6;
                }

                // Checking cube on right of current cube
                if (((x < cubicmap.width - 1) && COLOR_EQUAL(pixels[z*cubicmap.width + (x + 1)], RL_BLACK)) || (x == cubicmap.width - 1))
                {
                    // Define right triangles (2 tris, 6 vertex) --> v3 v8 v4, v4 v8 v5
                    // NOTE: Collateral occluded faces are not generated
                    mapVertices[vCounter] = v3;
                    mapVertices[vCounter + 1] = v8;
                    mapVertices[vCounter + 2] = v4;
                    mapVertices[vCounter + 3] = v4;
                    mapVertices[vCounter + 4] = v8;
                    mapVertices[vCounter + 5] = v5;
                    vCounter += 6;

                    mapNormals[nCounter] = n1;
                    mapNormals[nCounter + 1] = n1;
                    mapNormals[nCounter + 2] = n1;
                    mapNormals[nCounter + 3] = n1;
                    mapNormals[nCounter + 4] = n1;
                    mapNormals[nCounter + 5] = n1;
                    nCounter += 6;

                    mapTexcoords[tcCounter] = (RL_Vector2){ rightTexUV.x, rightTexUV.y };
                    mapTexcoords[tcCounter + 1] = (RL_Vector2){ rightTexUV.x, rightTexUV.y + rightTexUV.height };
                    mapTexcoords[tcCounter + 2] = (RL_Vector2){ rightTexUV.x + rightTexUV.width, rightTexUV.y };
                    mapTexcoords[tcCounter + 3] = (RL_Vector2){ rightTexUV.x + rightTexUV.width, rightTexUV.y };
                    mapTexcoords[tcCounter + 4] = (RL_Vector2){ rightTexUV.x, rightTexUV.y + rightTexUV.height };
                    mapTexcoords[tcCounter + 5] = (RL_Vector2){ rightTexUV.x + rightTexUV.width, rightTexUV.y + rightTexUV.height };
                    tcCounter += 6;
                }

                // Checking cube on left of current cube
                if (((x > 0) && COLOR_EQUAL(pixels[z*cubicmap.width + (x - 1)], RL_BLACK)) || (x == 0))
                {
                    // Define left triangles (2 tris, 6 vertex) --> v1 v7 v2, v1 v6 v7
                    // NOTE: Collateral occluded faces are not generated
                    mapVertices[vCounter] = v1;
                    mapVertices[vCounter + 1] = v7;
                    mapVertices[vCounter + 2] = v2;
                    mapVertices[vCounter + 3] = v1;
                    mapVertices[vCounter + 4] = v6;
                    mapVertices[vCounter + 5] = v7;
                    vCounter += 6;

                    mapNormals[nCounter] = n2;
                    mapNormals[nCounter + 1] = n2;
                    mapNormals[nCounter + 2] = n2;
                    mapNormals[nCounter + 3] = n2;
                    mapNormals[nCounter + 4] = n2;
                    mapNormals[nCounter + 5] = n2;
                    nCounter += 6;

                    mapTexcoords[tcCounter] = (RL_Vector2){ leftTexUV.x, leftTexUV.y };
                    mapTexcoords[tcCounter + 1] = (RL_Vector2){ leftTexUV.x + leftTexUV.width, leftTexUV.y + leftTexUV.height };
                    mapTexcoords[tcCounter + 2] = (RL_Vector2){ leftTexUV.x + leftTexUV.width, leftTexUV.y };
                    mapTexcoords[tcCounter + 3] = (RL_Vector2){ leftTexUV.x, leftTexUV.y };
                    mapTexcoords[tcCounter + 4] = (RL_Vector2){ leftTexUV.x, leftTexUV.y + leftTexUV.height };
                    mapTexcoords[tcCounter + 5] = (RL_Vector2){ leftTexUV.x + leftTexUV.width, leftTexUV.y + leftTexUV.height };
                    tcCounter += 6;
                }
            }
            // We check pixel color to be RL_BLACK, we will only draw floor and roof
            else if (COLOR_EQUAL(pixels[z*cubicmap.width + x], RL_BLACK))
            {
                // Define top triangles (2 tris, 6 vertex --> v1-v2-v3, v1-v3-v4)
                mapVertices[vCounter] = v1;
                mapVertices[vCounter + 1] = v3;
                mapVertices[vCounter + 2] = v2;
                mapVertices[vCounter + 3] = v1;
                mapVertices[vCounter + 4] = v4;
                mapVertices[vCounter + 5] = v3;
                vCounter += 6;

                mapNormals[nCounter] = n4;
                mapNormals[nCounter + 1] = n4;
                mapNormals[nCounter + 2] = n4;
                mapNormals[nCounter + 3] = n4;
                mapNormals[nCounter + 4] = n4;
                mapNormals[nCounter + 5] = n4;
                nCounter += 6;

                mapTexcoords[tcCounter] = (RL_Vector2){ topTexUV.x, topTexUV.y };
                mapTexcoords[tcCounter + 1] = (RL_Vector2){ topTexUV.x + topTexUV.width, topTexUV.y + topTexUV.height };
                mapTexcoords[tcCounter + 2] = (RL_Vector2){ topTexUV.x, topTexUV.y + topTexUV.height };
                mapTexcoords[tcCounter + 3] = (RL_Vector2){ topTexUV.x, topTexUV.y };
                mapTexcoords[tcCounter + 4] = (RL_Vector2){ topTexUV.x + topTexUV.width, topTexUV.y };
                mapTexcoords[tcCounter + 5] = (RL_Vector2){ topTexUV.x + topTexUV.width, topTexUV.y + topTexUV.height };
                tcCounter += 6;

                // Define bottom triangles (2 tris, 6 vertex --> v6-v8-v7, v6-v5-v8)
                mapVertices[vCounter] = v6;
                mapVertices[vCounter + 1] = v7;
                mapVertices[vCounter + 2] = v8;
                mapVertices[vCounter + 3] = v6;
                mapVertices[vCounter + 4] = v8;
                mapVertices[vCounter + 5] = v5;
                vCounter += 6;

                mapNormals[nCounter] = n3;
                mapNormals[nCounter + 1] = n3;
                mapNormals[nCounter + 2] = n3;
                mapNormals[nCounter + 3] = n3;
                mapNormals[nCounter + 4] = n3;
                mapNormals[nCounter + 5] = n3;
                nCounter += 6;

                mapTexcoords[tcCounter] = (RL_Vector2){ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y };
                mapTexcoords[tcCounter + 1] = (RL_Vector2){ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y + bottomTexUV.height };
                mapTexcoords[tcCounter + 2] = (RL_Vector2){ bottomTexUV.x, bottomTexUV.y + bottomTexUV.height };
                mapTexcoords[tcCounter + 3] = (RL_Vector2){ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y };
                mapTexcoords[tcCounter + 4] = (RL_Vector2){ bottomTexUV.x, bottomTexUV.y + bottomTexUV.height };
                mapTexcoords[tcCounter + 5] = (RL_Vector2){ bottomTexUV.x, bottomTexUV.y };
                tcCounter += 6;
            }
        }
    }

    // Move data from mapVertices temp arrays to vertices float array
    mesh.vertexCount = vCounter;
    mesh.triangleCount = vCounter/3;

    mesh.vertices = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.normals = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.texcoords = (float *)RL_MALLOC(mesh.vertexCount*2*sizeof(float));
    mesh.colors = NULL;

    int fCounter = 0;

    // Move vertices data
    for (int i = 0; i < vCounter; i++)
    {
        mesh.vertices[fCounter] = mapVertices[i].x;
        mesh.vertices[fCounter + 1] = mapVertices[i].y;
        mesh.vertices[fCounter + 2] = mapVertices[i].z;
        fCounter += 3;
    }

    fCounter = 0;

    // Move normals data
    for (int i = 0; i < nCounter; i++)
    {
        mesh.normals[fCounter] = mapNormals[i].x;
        mesh.normals[fCounter + 1] = mapNormals[i].y;
        mesh.normals[fCounter + 2] = mapNormals[i].z;
        fCounter += 3;
    }

    fCounter = 0;

    // Move texcoords data
    for (int i = 0; i < tcCounter; i++)
    {
        mesh.texcoords[fCounter] = mapTexcoords[i].x;
        mesh.texcoords[fCounter + 1] = mapTexcoords[i].y;
        fCounter += 2;
    }

    RL_FREE(mapVertices);
    RL_FREE(mapNormals);
    RL_FREE(mapTexcoords);

    RL_UnloadImageColors(pixels);   // Unload pixels color data

    // Upload vertex data to GPU (static mesh)
    RL_UploadMesh(&mesh, false);

    return mesh;
}
#endif      // SUPPORT_MESH_GENERATION

// Compute mesh bounding box limits
// NOTE: minVertex and maxVertex should be transformed by model transform matrix
RL_BoundingBox RL_GetMeshBoundingBox(RL_Mesh mesh)
{
    // Get min and max vertex to construct bounds (AABB)
    RL_Vector3 minVertex = { 0 };
    RL_Vector3 maxVertex = { 0 };

    if (mesh.vertices != NULL)
    {
        minVertex = (RL_Vector3){ mesh.vertices[0], mesh.vertices[1], mesh.vertices[2] };
        maxVertex = (RL_Vector3){ mesh.vertices[0], mesh.vertices[1], mesh.vertices[2] };

        for (int i = 1; i < mesh.vertexCount; i++)
        {
            minVertex = Vector3Min(minVertex, (RL_Vector3){ mesh.vertices[i*3], mesh.vertices[i*3 + 1], mesh.vertices[i*3 + 2] });
            maxVertex = Vector3Max(maxVertex, (RL_Vector3){ mesh.vertices[i*3], mesh.vertices[i*3 + 1], mesh.vertices[i*3 + 2] });
        }
    }

    // Create the bounding box
    RL_BoundingBox box = { 0 };
    box.min = minVertex;
    box.max = maxVertex;

    return box;
}

// Compute mesh tangents
// NOTE: To calculate mesh tangents and binormals we need mesh vertex positions and texture coordinates
// Implementation based on: https://answers.unity.com/questions/7789/calculating-tangents-vector4.html
void RL_GenMeshTangents(RL_Mesh *mesh)
{
    if ((mesh->vertices == NULL) || (mesh->texcoords == NULL))
    {
        TRACELOG(LOG_WARNING, "MESH: Tangents generation requires texcoord vertex attribute data");
        return;
    }

    if (mesh->tangents == NULL) mesh->tangents = (float *)RL_MALLOC(mesh->vertexCount*4*sizeof(float));
    else
    {
        RL_FREE(mesh->tangents);
        mesh->tangents = (float *)RL_MALLOC(mesh->vertexCount*4*sizeof(float));
    }

    RL_Vector3 *tan1 = (RL_Vector3 *)RL_MALLOC(mesh->vertexCount*sizeof(RL_Vector3));
    RL_Vector3 *tan2 = (RL_Vector3 *)RL_MALLOC(mesh->vertexCount*sizeof(RL_Vector3));

    if (mesh->vertexCount % 3 != 0)
    {
        TRACELOG(LOG_WARNING, "MESH: vertexCount expected to be a multiple of 3. Expect uninitialized values.");
    }

    for (int i = 0; i <= mesh->vertexCount - 3; i += 3)
    {
        // Get triangle vertices
        RL_Vector3 v1 = { mesh->vertices[(i + 0)*3 + 0], mesh->vertices[(i + 0)*3 + 1], mesh->vertices[(i + 0)*3 + 2] };
        RL_Vector3 v2 = { mesh->vertices[(i + 1)*3 + 0], mesh->vertices[(i + 1)*3 + 1], mesh->vertices[(i + 1)*3 + 2] };
        RL_Vector3 v3 = { mesh->vertices[(i + 2)*3 + 0], mesh->vertices[(i + 2)*3 + 1], mesh->vertices[(i + 2)*3 + 2] };

        // Get triangle texcoords
        RL_Vector2 uv1 = { mesh->texcoords[(i + 0)*2 + 0], mesh->texcoords[(i + 0)*2 + 1] };
        RL_Vector2 uv2 = { mesh->texcoords[(i + 1)*2 + 0], mesh->texcoords[(i + 1)*2 + 1] };
        RL_Vector2 uv3 = { mesh->texcoords[(i + 2)*2 + 0], mesh->texcoords[(i + 2)*2 + 1] };

        float x1 = v2.x - v1.x;
        float y1 = v2.y - v1.y;
        float z1 = v2.z - v1.z;
        float x2 = v3.x - v1.x;
        float y2 = v3.y - v1.y;
        float z2 = v3.z - v1.z;

        float s1 = uv2.x - uv1.x;
        float t1 = uv2.y - uv1.y;
        float s2 = uv3.x - uv1.x;
        float t2 = uv3.y - uv1.y;

        float div = s1*t2 - s2*t1;
        float r = (div == 0.0f)? 0.0f : 1.0f/div;

        RL_Vector3 sdir = { (t2*x1 - t1*x2)*r, (t2*y1 - t1*y2)*r, (t2*z1 - t1*z2)*r };
        RL_Vector3 tdir = { (s1*x2 - s2*x1)*r, (s1*y2 - s2*y1)*r, (s1*z2 - s2*z1)*r };

        tan1[i + 0] = sdir;
        tan1[i + 1] = sdir;
        tan1[i + 2] = sdir;

        tan2[i + 0] = tdir;
        tan2[i + 1] = tdir;
        tan2[i + 2] = tdir;
    }

    // Compute tangents considering normals
    for (int i = 0; i < mesh->vertexCount; i++)
    {
        RL_Vector3 normal = { mesh->normals[i*3 + 0], mesh->normals[i*3 + 1], mesh->normals[i*3 + 2] };
        RL_Vector3 tangent = tan1[i];

        // TODO: Review, not sure if tangent computation is right, just used reference proposed maths...
#if defined(COMPUTE_TANGENTS_METHOD_01)
        RL_Vector3 tmp = Vector3Subtract(tangent, Vector3Scale(normal, Vector3DotProduct(normal, tangent)));
        tmp = Vector3Normalize(tmp);
        mesh->tangents[i*4 + 0] = tmp.x;
        mesh->tangents[i*4 + 1] = tmp.y;
        mesh->tangents[i*4 + 2] = tmp.z;
        mesh->tangents[i*4 + 3] = 1.0f;
#else
        Vector3OrthoNormalize(&normal, &tangent);
        mesh->tangents[i*4 + 0] = tangent.x;
        mesh->tangents[i*4 + 1] = tangent.y;
        mesh->tangents[i*4 + 2] = tangent.z;
        mesh->tangents[i*4 + 3] = (Vector3DotProduct(Vector3CrossProduct(normal, tangent), tan2[i]) < 0.0f)? -1.0f : 1.0f;
#endif
    }

    RL_FREE(tan1);
    RL_FREE(tan2);

    if (mesh->vboId != NULL)
    {
        if (mesh->vboId[SHADER_LOC_VERTEX_TANGENT] != 0)
        {
            // Update existing vertex buffer
            rlUpdateVertexBuffer(mesh->vboId[SHADER_LOC_VERTEX_TANGENT], mesh->tangents, mesh->vertexCount*4*sizeof(float), 0);
        }
        else
        {
            // Load a new tangent attributes buffer
            mesh->vboId[SHADER_LOC_VERTEX_TANGENT] = rlLoadVertexBuffer(mesh->tangents, mesh->vertexCount*4*sizeof(float), false);
        }

        rlEnableVertexArray(mesh->vaoId);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT, 4, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT);
        rlDisableVertexArray();
    }

    TRACELOG(LOG_INFO, "MESH: Tangents data computed and uploaded for provided mesh");
}

// Draw a model (with texture if set)
void RL_DrawModel(RL_Model model, RL_Vector3 position, float scale, RL_Color tint)
{
    RL_Vector3 vScale = { scale, scale, scale };
    RL_Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f };

    RL_DrawModelEx(model, position, rotationAxis, 0.0f, vScale, tint);
}

// Draw a model with extended parameters
void RL_DrawModelEx(RL_Model model, RL_Vector3 position, RL_Vector3 rotationAxis, float rotationAngle, RL_Vector3 scale, RL_Color tint)
{
    // Calculate transformation matrix from function parameters
    // Get transform matrix (rotation -> scale -> translation)
    RL_Matrix matScale = MatrixScale(scale.x, scale.y, scale.z);
    RL_Matrix matRotation = MatrixRotate(rotationAxis, rotationAngle*DEG2RAD);
    RL_Matrix matTranslation = MatrixTranslate(position.x, position.y, position.z);

    RL_Matrix matTransform = MatrixMultiply(MatrixMultiply(matScale, matRotation), matTranslation);

    // Combine model transformation matrix (model.transform) with matrix generated by function parameters (matTransform)
    model.transform = MatrixMultiply(model.transform, matTransform);

    for (int i = 0; i < model.meshCount; i++)
    {
        RL_Color color = model.materials[model.meshMaterial[i]].maps[MATERIAL_MAP_DIFFUSE].color;

        RL_Color colorTint = RL_WHITE;
        colorTint.r = (unsigned char)(((int)color.r*(int)tint.r)/255);
        colorTint.g = (unsigned char)(((int)color.g*(int)tint.g)/255);
        colorTint.b = (unsigned char)(((int)color.b*(int)tint.b)/255);
        colorTint.a = (unsigned char)(((int)color.a*(int)tint.a)/255);

        model.materials[model.meshMaterial[i]].maps[MATERIAL_MAP_DIFFUSE].color = colorTint;
        RL_DrawMesh(model.meshes[i], model.materials[model.meshMaterial[i]], model.transform);
        model.materials[model.meshMaterial[i]].maps[MATERIAL_MAP_DIFFUSE].color = color;
    }
}

// Draw a model wires (with texture if set)
void RL_DrawModelWires(RL_Model model, RL_Vector3 position, float scale, RL_Color tint)
{
    rlEnableWireMode();

    RL_DrawModel(model, position, scale, tint);

    rlDisableWireMode();
}

// Draw a model wires (with texture if set) with extended parameters
void RL_DrawModelWiresEx(RL_Model model, RL_Vector3 position, RL_Vector3 rotationAxis, float rotationAngle, RL_Vector3 scale, RL_Color tint)
{
    rlEnableWireMode();

    RL_DrawModelEx(model, position, rotationAxis, rotationAngle, scale, tint);

    rlDisableWireMode();
}

// Draw a model points
void DrawModelPoints(RL_Model model, RL_Vector3 position, float scale, RL_Color tint)
{
    rlEnablePointMode();
    rlDisableBackfaceCulling();

    RL_DrawModel(model, position, scale, tint);

    rlEnableBackfaceCulling();
    rlDisableWireMode();
}

// Draw a model points
void DrawModelPointsEx(RL_Model model, RL_Vector3 position, RL_Vector3 rotationAxis, float rotationAngle, RL_Vector3 scale, RL_Color tint)
{
    rlEnablePointMode();
    rlDisableBackfaceCulling();

    RL_DrawModelEx(model, position, rotationAxis, rotationAngle, scale, tint);

    rlEnableBackfaceCulling();
    rlDisableWireMode();
}

// Draw a billboard
void RL_DrawBillboard(RL_Camera camera, RL_Texture2D texture, RL_Vector3 position, float scale, RL_Color tint)
{
    RL_Rectangle source = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };

    RL_DrawBillboardRec(camera, texture, source, position, (RL_Vector2) { scale*fabsf((float)source.width/source.height), scale }, tint);
}

// Draw a billboard (part of a texture defined by a rectangle)
void RL_DrawBillboardRec(RL_Camera camera, RL_Texture2D texture, RL_Rectangle source, RL_Vector3 position, RL_Vector2 size, RL_Color tint)
{
    // NOTE: Billboard locked on axis-Y
    RL_Vector3 up = { 0.0f, 1.0f, 0.0f };

    RL_DrawBillboardPro(camera, texture, source, position, up, size, Vector2Scale(size, 0.5), 0.0f, tint);
}

// Draw a billboard with additional parameters
void RL_DrawBillboardPro(RL_Camera camera, RL_Texture2D texture, RL_Rectangle source, RL_Vector3 position, RL_Vector3 up, RL_Vector2 size, RL_Vector2 origin, float rotation, RL_Color tint)
{
    // Compute the up vector and the right vector
    RL_Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);
    RL_Vector3 right = { matView.m0, matView.m4, matView.m8 };
    right = Vector3Scale(right, size.x);
    up = Vector3Scale(up, size.y);

    // Flip the content of the billboard while maintaining the counterclockwise edge rendering order
    if (size.x < 0.0f)
    {
        source.x += size.x;
        source.width *= -1.0;
        right = Vector3Negate(right);
        origin.x *= -1.0f;
    }
    if (size.y < 0.0f)
    {
        source.y += size.y;
        source.height *= -1.0;
        up = Vector3Negate(up);
        origin.y *= -1.0f;
    }

    // Draw the texture region described by source on the following rectangle in 3D space:
    //
    //                size.x          <--.
    //  3 ^---------------------------+ 2 \ rotation
    //    |                           |   /
    //    |                           |
    //    |   origin.x   position     |
    // up |..............             | size.y
    //    |             .             |
    //    |             . origin.y    |
    //    |             .             |
    //  0 +---------------------------> 1
    //                right
    RL_Vector3 forward;
    if (rotation != 0.0) forward = Vector3CrossProduct(right, up);

    RL_Vector3 origin3D = Vector3Add(Vector3Scale(Vector3Normalize(right), origin.x), Vector3Scale(Vector3Normalize(up), origin.y));

    RL_Vector3 points[4];
    points[0] = Vector3Zero();
    points[1] = right;
    points[2] = Vector3Add(up, right);
    points[3] = up;

    for (int i = 0; i < 4; i++)
    {
        points[i] = Vector3Subtract(points[i], origin3D);
        if (rotation != 0.0) points[i] = Vector3RotateByAxisAngle(points[i], forward, rotation * DEG2RAD);
        points[i] = Vector3Add(points[i], position);
    }

    RL_Vector2 texcoords[4];
    texcoords[0] = (RL_Vector2) { (float)source.x/texture.width, (float)(source.y + source.height)/texture.height };
    texcoords[1] = (RL_Vector2) { (float)(source.x + source.width)/texture.width, (float)(source.y + source.height)/texture.height };
    texcoords[2] = (RL_Vector2) { (float)(source.x + source.width)/texture.width, (float)source.y/texture.height };
    texcoords[3] = (RL_Vector2) { (float)source.x/texture.width, (float)source.y/texture.height };

    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);

        rlColor4ub(tint.r, tint.g, tint.b, tint.a);
        for (int i = 0; i < 4; i++)
        {
            rlTexCoord2f(texcoords[i].x, texcoords[i].y);
            rlVertex3f(points[i].x, points[i].y, points[i].z);
        }

    rlEnd();
    rlSetTexture(0);
}

// Draw a bounding box with wires
void RL_DrawBoundingBox(RL_BoundingBox box, RL_Color color)
{
    RL_Vector3 size = { 0 };

    size.x = fabsf(box.max.x - box.min.x);
    size.y = fabsf(box.max.y - box.min.y);
    size.z = fabsf(box.max.z - box.min.z);

    RL_Vector3 center = { box.min.x + size.x/2.0f, box.min.y + size.y/2.0f, box.min.z + size.z/2.0f };

    RL_DrawCubeWires(center, size.x, size.y, size.z, color);
}

// Check collision between two spheres
bool RL_CheckCollisionSpheres(RL_Vector3 center1, float radius1, RL_Vector3 center2, float radius2)
{
    bool collision = false;

    // Simple way to check for collision, just checking distance between two points
    // Unfortunately, sqrtf() is a costly operation, so we avoid it with following solution
    /*
    float dx = center1.x - center2.x;      // X distance between centers
    float dy = center1.y - center2.y;      // Y distance between centers
    float dz = center1.z - center2.z;      // Z distance between centers

    float distance = sqrtf(dx*dx + dy*dy + dz*dz);  // Distance between centers

    if (distance <= (radius1 + radius2)) collision = true;
    */

    // Check for distances squared to avoid sqrtf()
    if (Vector3DotProduct(Vector3Subtract(center2, center1), Vector3Subtract(center2, center1)) <= (radius1 + radius2)*(radius1 + radius2)) collision = true;

    return collision;
}

// Check collision between two boxes
// NOTE: Boxes are defined by two points minimum and maximum
bool RL_CheckCollisionBoxes(RL_BoundingBox box1, RL_BoundingBox box2)
{
    bool collision = true;

    if ((box1.max.x >= box2.min.x) && (box1.min.x <= box2.max.x))
    {
        if ((box1.max.y < box2.min.y) || (box1.min.y > box2.max.y)) collision = false;
        if ((box1.max.z < box2.min.z) || (box1.min.z > box2.max.z)) collision = false;
    }
    else collision = false;

    return collision;
}

// Check collision between box and sphere
bool RL_CheckCollisionBoxSphere(RL_BoundingBox box, RL_Vector3 center, float radius)
{
    bool collision = false;

    float dmin = 0;

    if (center.x < box.min.x) dmin += powf(center.x - box.min.x, 2);
    else if (center.x > box.max.x) dmin += powf(center.x - box.max.x, 2);

    if (center.y < box.min.y) dmin += powf(center.y - box.min.y, 2);
    else if (center.y > box.max.y) dmin += powf(center.y - box.max.y, 2);

    if (center.z < box.min.z) dmin += powf(center.z - box.min.z, 2);
    else if (center.z > box.max.z) dmin += powf(center.z - box.max.z, 2);

    if (dmin <= (radius*radius)) collision = true;

    return collision;
}

// Get collision info between ray and sphere
RL_RayCollision RL_GetRayCollisionSphere(RL_Ray ray, RL_Vector3 center, float radius)
{
    RL_RayCollision collision = { 0 };

    RL_Vector3 raySpherePos = Vector3Subtract(center, ray.position);
    float vector = Vector3DotProduct(raySpherePos, ray.direction);
    float distance = Vector3Length(raySpherePos);
    float d = radius*radius - (distance*distance - vector*vector);

    collision.hit = d >= 0.0f;

    // Check if ray origin is inside the sphere to calculate the correct collision point
    if (distance < radius)
    {
        collision.distance = vector + sqrtf(d);

        // Calculate collision point
        collision.point = Vector3Add(ray.position, Vector3Scale(ray.direction, collision.distance));

        // Calculate collision normal (pointing outwards)
        collision.normal = Vector3Negate(Vector3Normalize(Vector3Subtract(collision.point, center)));
    }
    else
    {
        collision.distance = vector - sqrtf(d);

        // Calculate collision point
        collision.point = Vector3Add(ray.position, Vector3Scale(ray.direction, collision.distance));

        // Calculate collision normal (pointing inwards)
        collision.normal = Vector3Normalize(Vector3Subtract(collision.point, center));
    }

    return collision;
}

// Get collision info between ray and box
RL_RayCollision RL_GetRayCollisionBox(RL_Ray ray, RL_BoundingBox box)
{
    RL_RayCollision collision = { 0 };

    // Note: If ray.position is inside the box, the distance is negative (as if the ray was reversed)
    // Reversing ray.direction will give use the correct result
    bool insideBox = (ray.position.x > box.min.x) && (ray.position.x < box.max.x) &&
                     (ray.position.y > box.min.y) && (ray.position.y < box.max.y) &&
                     (ray.position.z > box.min.z) && (ray.position.z < box.max.z);

    if (insideBox) ray.direction = Vector3Negate(ray.direction);

    float t[11] = { 0 };

    t[8] = 1.0f/ray.direction.x;
    t[9] = 1.0f/ray.direction.y;
    t[10] = 1.0f/ray.direction.z;

    t[0] = (box.min.x - ray.position.x)*t[8];
    t[1] = (box.max.x - ray.position.x)*t[8];
    t[2] = (box.min.y - ray.position.y)*t[9];
    t[3] = (box.max.y - ray.position.y)*t[9];
    t[4] = (box.min.z - ray.position.z)*t[10];
    t[5] = (box.max.z - ray.position.z)*t[10];
    t[6] = (float)fmax(fmax(fmin(t[0], t[1]), fmin(t[2], t[3])), fmin(t[4], t[5]));
    t[7] = (float)fmin(fmin(fmax(t[0], t[1]), fmax(t[2], t[3])), fmax(t[4], t[5]));

    collision.hit = !((t[7] < 0) || (t[6] > t[7]));
    collision.distance = t[6];
    collision.point = Vector3Add(ray.position, Vector3Scale(ray.direction, collision.distance));

    // Get box center point
    collision.normal = Vector3Lerp(box.min, box.max, 0.5f);
    // Get vector center point->hit point
    collision.normal = Vector3Subtract(collision.point, collision.normal);
    // Scale vector to unit cube
    // NOTE: We use an additional .01 to fix numerical errors
    collision.normal = Vector3Scale(collision.normal, 2.01f);
    collision.normal = Vector3Divide(collision.normal, Vector3Subtract(box.max, box.min));
    // The relevant elements of the vector are now slightly larger than 1.0f (or smaller than -1.0f)
    // and the others are somewhere between -1.0 and 1.0 casting to int is exactly our wanted normal!
    collision.normal.x = (float)((int)collision.normal.x);
    collision.normal.y = (float)((int)collision.normal.y);
    collision.normal.z = (float)((int)collision.normal.z);

    collision.normal = Vector3Normalize(collision.normal);

    if (insideBox)
    {
        // Reset ray.direction
        ray.direction = Vector3Negate(ray.direction);
        // Fix result
        collision.distance *= -1.0f;
        collision.normal = Vector3Negate(collision.normal);
    }

    return collision;
}

// Get collision info between ray and mesh
RL_RayCollision RL_GetRayCollisionMesh(RL_Ray ray, RL_Mesh mesh, RL_Matrix transform)
{
    RL_RayCollision collision = { 0 };

    // Check if mesh vertex data on CPU for testing
    if (mesh.vertices != NULL)
    {
        int triangleCount = mesh.triangleCount;

        // Test against all triangles in mesh
        for (int i = 0; i < triangleCount; i++)
        {
            RL_Vector3 a, b, c;
            RL_Vector3* vertdata = (RL_Vector3*)mesh.vertices;

            if (mesh.indices)
            {
                a = vertdata[mesh.indices[i*3 + 0]];
                b = vertdata[mesh.indices[i*3 + 1]];
                c = vertdata[mesh.indices[i*3 + 2]];
            }
            else
            {
                a = vertdata[i*3 + 0];
                b = vertdata[i*3 + 1];
                c = vertdata[i*3 + 2];
            }

            a = Vector3Transform(a, transform);
            b = Vector3Transform(b, transform);
            c = Vector3Transform(c, transform);

            RL_RayCollision triHitInfo = RL_GetRayCollisionTriangle(ray, a, b, c);

            if (triHitInfo.hit)
            {
                // Save the closest hit triangle
                if ((!collision.hit) || (collision.distance > triHitInfo.distance)) collision = triHitInfo;
            }
        }
    }

    return collision;
}

// Get collision info between ray and triangle
// NOTE: The points are expected to be in counter-clockwise winding
// NOTE: Based on https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
RL_RayCollision RL_GetRayCollisionTriangle(RL_Ray ray, RL_Vector3 p1, RL_Vector3 p2, RL_Vector3 p3)
{
    #define EPSILON 0.000001f        // A small number

    RL_RayCollision collision = { 0 };
    RL_Vector3 edge1 = { 0 };
    RL_Vector3 edge2 = { 0 };
    RL_Vector3 p, q, tv;
    float det, invDet, u, v, t;

    // Find vectors for two edges sharing V1
    edge1 = Vector3Subtract(p2, p1);
    edge2 = Vector3Subtract(p3, p1);

    // Begin calculating determinant - also used to calculate u parameter
    p = Vector3CrossProduct(ray.direction, edge2);

    // If determinant is near zero, ray lies in plane of triangle or ray is parallel to plane of triangle
    det = Vector3DotProduct(edge1, p);

    // Avoid culling!
    if ((det > -EPSILON) && (det < EPSILON)) return collision;

    invDet = 1.0f/det;

    // Calculate distance from V1 to ray origin
    tv = Vector3Subtract(ray.position, p1);

    // Calculate u parameter and test bound
    u = Vector3DotProduct(tv, p)*invDet;

    // The intersection lies outside the triangle
    if ((u < 0.0f) || (u > 1.0f)) return collision;

    // Prepare to test v parameter
    q = Vector3CrossProduct(tv, edge1);

    // Calculate V parameter and test bound
    v = Vector3DotProduct(ray.direction, q)*invDet;

    // The intersection lies outside the triangle
    if ((v < 0.0f) || ((u + v) > 1.0f)) return collision;

    t = Vector3DotProduct(edge2, q)*invDet;

    if (t > EPSILON)
    {
        // RL_Ray hit, get hit point and normal
        collision.hit = true;
        collision.distance = t;
        collision.normal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));
        collision.point = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    }

    return collision;
}

// Get collision info between ray and quad
// NOTE: The points are expected to be in counter-clockwise winding
RL_RayCollision RL_GetRayCollisionQuad(RL_Ray ray, RL_Vector3 p1, RL_Vector3 p2, RL_Vector3 p3, RL_Vector3 p4)
{
    RL_RayCollision collision = { 0 };

    collision = RL_GetRayCollisionTriangle(ray, p1, p2, p4);

    if (!collision.hit) collision = RL_GetRayCollisionTriangle(ray, p2, p3, p4);

    return collision;
}

//----------------------------------------------------------------------------------
// Module specific Functions Definition
//----------------------------------------------------------------------------------
#if defined(SUPPORT_FILEFORMAT_IQM) || defined(SUPPORT_FILEFORMAT_GLTF)
// Build pose from parent joints
// NOTE: Required for animations loading (required by IQM and GLTF)
static void BuildPoseFromParentJoints(RL_BoneInfo *bones, int boneCount, RL_Transform *transforms)
{
    for (int i = 0; i < boneCount; i++)
    {
        if (bones[i].parent >= 0)
        {
            if (bones[i].parent > i)
            {
                TRACELOG(LOG_WARNING, "Assumes bones are toplogically sorted, but bone %d has parent %d. Skipping.", i, bones[i].parent);
                continue;
            }
            transforms[i].rotation = QuaternionMultiply(transforms[bones[i].parent].rotation, transforms[i].rotation);
            transforms[i].translation = Vector3RotateByQuaternion(transforms[i].translation, transforms[bones[i].parent].rotation);
            transforms[i].translation = Vector3Add(transforms[i].translation, transforms[bones[i].parent].translation);
            transforms[i].scale = Vector3Multiply(transforms[i].scale, transforms[bones[i].parent].scale);
        }
    }
}
#endif

#if defined(SUPPORT_FILEFORMAT_OBJ)
// Load OBJ mesh data
//
// Keep the following information in mind when reading this
//  - A mesh is created for every material present in the obj file
//  - the model.meshCount is therefore the materialCount returned from tinyobj
//  - the mesh is automatically triangulated by tinyobj
static RL_Model LoadOBJ(const char *fileName)
{
    tinyobj_attrib_t objAttributes = { 0 };
    tinyobj_shape_t* objShapes = NULL;
    unsigned int objShapeCount = 0;

    tinyobj_material_t* objMaterials = NULL;
    unsigned int objMaterialCount = 0;

    RL_Model model = { 0 };
    model.transform = MatrixIdentity();

    char* fileText = RL_LoadFileText(fileName);

    if (fileText == NULL)
    {
        TRACELOG(LOG_ERROR, "MODEL Unable to read obj file %s", fileName);
        return model;
    }

    char currentDir[1024] = { 0 };
    strcpy(currentDir, RL_GetWorkingDirectory()); // Save current working directory
    const char* workingDir = RL_GetDirectoryPath(fileName); // Switch to OBJ directory for material path correctness
    if (CHDIR(workingDir) != 0)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to change working directory", workingDir);
    }

    unsigned int dataSize = (unsigned int)strlen(fileText);

    unsigned int flags = TINYOBJ_FLAG_TRIANGULATE;
    int ret = tinyobj_parse_obj(&objAttributes, &objShapes, &objShapeCount, &objMaterials, &objMaterialCount, fileText, dataSize, flags);

    if (ret != TINYOBJ_SUCCESS)
    {
        TRACELOG(LOG_ERROR, "MODEL Unable to read obj data %s", fileName);
        return model;
    }

    RL_UnloadFileText(fileText);

    unsigned int faceVertIndex = 0;
    unsigned int nextShape = 1;
    int lastMaterial = -1;
    unsigned int meshIndex = 0;

    // count meshes
    unsigned int nextShapeEnd = objAttributes.num_face_num_verts;

    // see how many verts till the next shape

    if (objShapeCount > 1) nextShapeEnd = objShapes[nextShape].face_offset;

    // walk all the faces
    for (unsigned int faceId = 0; faceId < objAttributes.num_faces; faceId++)
    {
        if (faceVertIndex >= nextShapeEnd)
        {
            // try to find the last vert in the next shape
            nextShape++;
            if (nextShape < objShapeCount) nextShapeEnd = objShapes[nextShape].face_offset;
            else nextShapeEnd = objAttributes.num_face_num_verts; // this is actually the total number of face verts in the file, not faces
            meshIndex++;
        }
        else if (lastMaterial != -1 && objAttributes.material_ids[faceId] != lastMaterial)
        {
            meshIndex++;// if this is a new material, we need to allocate a new mesh
        }

        lastMaterial = objAttributes.material_ids[faceId];
        faceVertIndex += objAttributes.face_num_verts[faceId];
    }

    // allocate the base meshes and materials
    model.meshCount = meshIndex + 1;
    model.meshes = (RL_Mesh*)RL_MemAlloc(sizeof(RL_Mesh) * model.meshCount);

    if (objMaterialCount > 0)
    {
        model.materialCount = objMaterialCount;
        model.materials = (RL_Material*)RL_MemAlloc(sizeof(RL_Material) * objMaterialCount);
    }
    else // we must allocate at least one material
    {
        model.materialCount = 1;
        model.materials = (RL_Material*)RL_MemAlloc(sizeof(RL_Material) * 1);
    }

    model.meshMaterial = (int*)RL_MemAlloc(sizeof(int) * model.meshCount);

    // see how many verts are in each mesh
    unsigned int* localMeshVertexCounts = (unsigned int*)RL_MemAlloc(sizeof(unsigned int) * model.meshCount);

    faceVertIndex = 0;
    nextShapeEnd = objAttributes.num_face_num_verts;
    lastMaterial = -1;
    meshIndex = 0;
    unsigned int localMeshVertexCount = 0;

    nextShape = 1;
    if (objShapeCount > 1)
        nextShapeEnd = objShapes[nextShape].face_offset;

    // walk all the faces
    for (unsigned int faceId = 0; faceId < objAttributes.num_faces; faceId++)
    {
        bool newMesh = false; // do we need a new mesh?
        if (faceVertIndex >= nextShapeEnd)
        {
            // try to find the last vert in the next shape
            nextShape++;
            if (nextShape < objShapeCount) nextShapeEnd = objShapes[nextShape].face_offset;
            else nextShapeEnd = objAttributes.num_face_num_verts; // this is actually the total number of face verts in the file, not faces

            newMesh = true;
        }
        else if (lastMaterial != -1 && objAttributes.material_ids[faceId] != lastMaterial)
        {
            newMesh = true;
        }

        lastMaterial = objAttributes.material_ids[faceId];

        if (newMesh)
        {
            localMeshVertexCounts[meshIndex] = localMeshVertexCount;

            localMeshVertexCount = 0;
            meshIndex++;
        }

        faceVertIndex += objAttributes.face_num_verts[faceId];
        localMeshVertexCount += objAttributes.face_num_verts[faceId];
    }
    localMeshVertexCounts[meshIndex] = localMeshVertexCount;

    for (int i = 0; i < model.meshCount; i++)
    {
        // allocate the buffers for each mesh
        unsigned int vertexCount = localMeshVertexCounts[i];

        model.meshes[i].vertexCount = vertexCount;
        model.meshes[i].triangleCount = vertexCount / 3;

        model.meshes[i].vertices = (float*)RL_MemAlloc(sizeof(float) * vertexCount * 3);
        model.meshes[i].normals = (float*)RL_MemAlloc(sizeof(float) * vertexCount * 3);
        model.meshes[i].texcoords = (float*)RL_MemAlloc(sizeof(float) * vertexCount * 2);
        model.meshes[i].colors = (unsigned char*)RL_MemAlloc(sizeof(unsigned char) * vertexCount * 4);
    }

    RL_MemFree(localMeshVertexCounts);
    localMeshVertexCounts = NULL;

    // fill meshes
    faceVertIndex = 0;

    nextShapeEnd = objAttributes.num_face_num_verts;

    // see how many verts till the next shape
    nextShape = 1;
    if (objShapeCount > 1) nextShapeEnd = objShapes[nextShape].face_offset;
    lastMaterial = -1;
    meshIndex = 0;
    localMeshVertexCount = 0;

    // walk all the faces
    for (unsigned int faceId = 0; faceId < objAttributes.num_faces; faceId++)
    {
        bool newMesh = false; // do we need a new mesh?
        if (faceVertIndex >= nextShapeEnd)
        {
            // try to find the last vert in the next shape
            nextShape++;
            if (nextShape < objShapeCount) nextShapeEnd = objShapes[nextShape].face_offset;
            else nextShapeEnd = objAttributes.num_face_num_verts; // this is actually the total number of face verts in the file, not faces
            newMesh = true;
        }
        // if this is a new material, we need to allocate a new mesh
        if (lastMaterial != -1 && objAttributes.material_ids[faceId] != lastMaterial) newMesh = true;
        lastMaterial = objAttributes.material_ids[faceId];

        if (newMesh)
        {
            localMeshVertexCount = 0;
            meshIndex++;
        }

        int matId = 0;
        if (lastMaterial >= 0 && lastMaterial < (int)objMaterialCount)
            matId = lastMaterial;

        model.meshMaterial[meshIndex] = matId;

        for (int f = 0; f < objAttributes.face_num_verts[faceId]; f++)
        {
            int vertIndex = objAttributes.faces[faceVertIndex].v_idx;
            int normalIndex = objAttributes.faces[faceVertIndex].vn_idx;
            int texcordIndex = objAttributes.faces[faceVertIndex].vt_idx;

            for (int i = 0; i < 3; i++)
                model.meshes[meshIndex].vertices[localMeshVertexCount * 3 + i] = objAttributes.vertices[vertIndex * 3 + i];

            for (int i = 0; i < 3; i++)
                model.meshes[meshIndex].normals[localMeshVertexCount * 3 + i] = objAttributes.normals[normalIndex * 3 + i];

            for (int i = 0; i < 2; i++)
                model.meshes[meshIndex].texcoords[localMeshVertexCount * 2 + i] = objAttributes.texcoords[texcordIndex * 2 + i];

            model.meshes[meshIndex].texcoords[localMeshVertexCount * 2 + 1] = 1.0f - model.meshes[meshIndex].texcoords[localMeshVertexCount * 2 + 1];

            for (int i = 0; i < 4; i++)
                model.meshes[meshIndex].colors[localMeshVertexCount * 4 + i] = 255;

            faceVertIndex++;
            localMeshVertexCount++;
        }
    }

    if (objMaterialCount > 0) ProcessMaterialsOBJ(model.materials, objMaterials, objMaterialCount);
    else model.materials[0] = RL_LoadMaterialDefault(); // Set default material for the mesh

    tinyobj_attrib_free(&objAttributes);
    tinyobj_shapes_free(objShapes, objShapeCount);
    tinyobj_materials_free(objMaterials, objMaterialCount);

    for (int i = 0; i < model.meshCount; i++)
        RL_UploadMesh(model.meshes + i, true);

    // Restore current working directory
    if (CHDIR(currentDir) != 0)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to change working directory", currentDir);
    }

    return model;
}
#endif

#if defined(SUPPORT_FILEFORMAT_IQM)
// Load IQM mesh data
static RL_Model LoadIQM(const char *fileName)
{
    #define IQM_MAGIC     "INTERQUAKEMODEL" // IQM file magic number
    #define IQM_VERSION          2          // only IQM version 2 supported

    #define BONE_NAME_LENGTH    32          // RL_BoneInfo name string length
    #define MESH_NAME_LENGTH    32          // RL_Mesh name string length
    #define MATERIAL_NAME_LENGTH 32         // RL_Material name string length

    int dataSize = 0;
    unsigned char *fileData = RL_LoadFileData(fileName, &dataSize);
    unsigned char *fileDataPtr = fileData;

    // IQM file structs
    //-----------------------------------------------------------------------------------
    typedef struct IQMHeader {
        char magic[16];
        unsigned int version;
        unsigned int dataSize;
        unsigned int flags;
        unsigned int num_text, ofs_text;
        unsigned int num_meshes, ofs_meshes;
        unsigned int num_vertexarrays, num_vertexes, ofs_vertexarrays;
        unsigned int num_triangles, ofs_triangles, ofs_adjacency;
        unsigned int num_joints, ofs_joints;
        unsigned int num_poses, ofs_poses;
        unsigned int num_anims, ofs_anims;
        unsigned int num_frames, num_framechannels, ofs_frames, ofs_bounds;
        unsigned int num_comment, ofs_comment;
        unsigned int num_extensions, ofs_extensions;
    } IQMHeader;

    typedef struct IQMMesh {
        unsigned int name;
        unsigned int material;
        unsigned int first_vertex, num_vertexes;
        unsigned int first_triangle, num_triangles;
    } IQMMesh;

    typedef struct IQMTriangle {
        unsigned int vertex[3];
    } IQMTriangle;

    typedef struct IQMJoint {
        unsigned int name;
        int parent;
        float translate[3], rotate[4], scale[3];
    } IQMJoint;

    typedef struct IQMVertexArray {
        unsigned int type;
        unsigned int flags;
        unsigned int format;
        unsigned int size;
        unsigned int offset;
    } IQMVertexArray;

    // NOTE: Below IQM structures are not used but listed for reference
    /*
    typedef struct IQMAdjacency {
        unsigned int triangle[3];
    } IQMAdjacency;

    typedef struct IQMPose {
        int parent;
        unsigned int mask;
        float channeloffset[10];
        float channelscale[10];
    } IQMPose;

    typedef struct IQMAnim {
        unsigned int name;
        unsigned int first_frame, num_frames;
        float framerate;
        unsigned int flags;
    } IQMAnim;

    typedef struct IQMBounds {
        float bbmin[3], bbmax[3];
        float xyradius, radius;
    } IQMBounds;
    */
    //-----------------------------------------------------------------------------------

    // IQM vertex data types
    enum {
        IQM_POSITION     = 0,
        IQM_TEXCOORD     = 1,
        IQM_NORMAL       = 2,
        IQM_TANGENT      = 3,       // NOTE: Tangents unused by default
        IQM_BLENDINDEXES = 4,
        IQM_BLENDWEIGHTS = 5,
        IQM_COLOR        = 6,
        IQM_CUSTOM       = 0x10     // NOTE: Custom vertex values unused by default
    };

    RL_Model model = { 0 };

    IQMMesh *imesh = NULL;
    IQMTriangle *tri = NULL;
    IQMVertexArray *va = NULL;
    IQMJoint *ijoint = NULL;

    float *vertex = NULL;
    float *normal = NULL;
    float *text = NULL;
    char *blendi = NULL;
    unsigned char *blendw = NULL;
    unsigned char *color = NULL;

    // In case file can not be read, return an empty model
    if (fileDataPtr == NULL) return model;

    const char *basePath = RL_GetDirectoryPath(fileName);

    // Read IQM header
    IQMHeader *iqmHeader = (IQMHeader *)fileDataPtr;

    if (memcmp(iqmHeader->magic, IQM_MAGIC, sizeof(IQM_MAGIC)) != 0)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] IQM file is not a valid model", fileName);
        return model;
    }

    if (iqmHeader->version != IQM_VERSION)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] IQM file version not supported (%i)", fileName, iqmHeader->version);
        return model;
    }

    //fileDataPtr += sizeof(IQMHeader);       // Move file data pointer

    // Meshes data processing
    imesh = RL_MALLOC(iqmHeader->num_meshes*sizeof(IQMMesh));
    //fseek(iqmFile, iqmHeader->ofs_meshes, SEEK_SET);
    //fread(imesh, sizeof(IQMMesh)*iqmHeader->num_meshes, 1, iqmFile);
    memcpy(imesh, fileDataPtr + iqmHeader->ofs_meshes, iqmHeader->num_meshes*sizeof(IQMMesh));

    model.meshCount = iqmHeader->num_meshes;
    model.meshes = RL_CALLOC(model.meshCount, sizeof(RL_Mesh));

    model.materialCount = model.meshCount;
    model.materials = (RL_Material *)RL_CALLOC(model.materialCount, sizeof(RL_Material));
    model.meshMaterial = (int *)RL_CALLOC(model.meshCount, sizeof(int));

    char name[MESH_NAME_LENGTH] = { 0 };
    char material[MATERIAL_NAME_LENGTH] = { 0 };

    for (int i = 0; i < model.meshCount; i++)
    {
        //fseek(iqmFile, iqmHeader->ofs_text + imesh[i].name, SEEK_SET);
        //fread(name, sizeof(char), MESH_NAME_LENGTH, iqmFile);
        memcpy(name, fileDataPtr + iqmHeader->ofs_text + imesh[i].name, MESH_NAME_LENGTH*sizeof(char));

        //fseek(iqmFile, iqmHeader->ofs_text + imesh[i].material, SEEK_SET);
        //fread(material, sizeof(char), MATERIAL_NAME_LENGTH, iqmFile);
        memcpy(material, fileDataPtr + iqmHeader->ofs_text + imesh[i].material, MATERIAL_NAME_LENGTH*sizeof(char));

        model.materials[i] = RL_LoadMaterialDefault();
        model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = RL_LoadTexture(RL_TextFormat("%s/%s", basePath, material));

        model.meshMaterial[i] = i;

        TRACELOG(LOG_DEBUG, "MODEL: [%s] mesh name (%s), material (%s)", fileName, name, material);

        model.meshes[i].vertexCount = imesh[i].num_vertexes;

        model.meshes[i].vertices = RL_CALLOC(model.meshes[i].vertexCount*3, sizeof(float));       // Default vertex positions
        model.meshes[i].normals = RL_CALLOC(model.meshes[i].vertexCount*3, sizeof(float));        // Default vertex normals
        model.meshes[i].texcoords = RL_CALLOC(model.meshes[i].vertexCount*2, sizeof(float));      // Default vertex texcoords

        model.meshes[i].boneIds = RL_CALLOC(model.meshes[i].vertexCount*4, sizeof(unsigned char));  // Up-to 4 bones supported!
        model.meshes[i].boneWeights = RL_CALLOC(model.meshes[i].vertexCount*4, sizeof(float));      // Up-to 4 bones supported!

        model.meshes[i].triangleCount = imesh[i].num_triangles;
        model.meshes[i].indices = RL_CALLOC(model.meshes[i].triangleCount*3, sizeof(unsigned short));

        // Animated vertex data, what we actually process for rendering
        // NOTE: Animated vertex should be re-uploaded to GPU (if not using GPU skinning)
        model.meshes[i].animVertices = RL_CALLOC(model.meshes[i].vertexCount*3, sizeof(float));
        model.meshes[i].animNormals = RL_CALLOC(model.meshes[i].vertexCount*3, sizeof(float));
    }

    // Triangles data processing
    tri = RL_MALLOC(iqmHeader->num_triangles*sizeof(IQMTriangle));
    //fseek(iqmFile, iqmHeader->ofs_triangles, SEEK_SET);
    //fread(tri, sizeof(IQMTriangle), iqmHeader->num_triangles, iqmFile);
    memcpy(tri, fileDataPtr + iqmHeader->ofs_triangles, iqmHeader->num_triangles*sizeof(IQMTriangle));

    for (int m = 0; m < model.meshCount; m++)
    {
        int tcounter = 0;

        for (unsigned int i = imesh[m].first_triangle; i < (imesh[m].first_triangle + imesh[m].num_triangles); i++)
        {
            // IQM triangles indexes are stored in counter-clockwise, but raylib processes the index in linear order,
            // expecting they point to the counter-clockwise vertex triangle, so we need to reverse triangle indexes
            // NOTE: raylib renders vertex data in counter-clockwise order (standard convention) by default
            model.meshes[m].indices[tcounter + 2] = tri[i].vertex[0] - imesh[m].first_vertex;
            model.meshes[m].indices[tcounter + 1] = tri[i].vertex[1] - imesh[m].first_vertex;
            model.meshes[m].indices[tcounter] = tri[i].vertex[2] - imesh[m].first_vertex;
            tcounter += 3;
        }
    }

    // Vertex arrays data processing
    va = RL_MALLOC(iqmHeader->num_vertexarrays*sizeof(IQMVertexArray));
    //fseek(iqmFile, iqmHeader->ofs_vertexarrays, SEEK_SET);
    //fread(va, sizeof(IQMVertexArray), iqmHeader->num_vertexarrays, iqmFile);
    memcpy(va, fileDataPtr + iqmHeader->ofs_vertexarrays, iqmHeader->num_vertexarrays*sizeof(IQMVertexArray));

    for (unsigned int i = 0; i < iqmHeader->num_vertexarrays; i++)
    {
        switch (va[i].type)
        {
            case IQM_POSITION:
            {
                vertex = RL_MALLOC(iqmHeader->num_vertexes*3*sizeof(float));
                //fseek(iqmFile, va[i].offset, SEEK_SET);
                //fread(vertex, iqmHeader->num_vertexes*3*sizeof(float), 1, iqmFile);
                memcpy(vertex, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*3*sizeof(float));

                for (unsigned int m = 0; m < iqmHeader->num_meshes; m++)
                {
                    int vCounter = 0;
                    for (unsigned int i = imesh[m].first_vertex*3; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*3; i++)
                    {
                        model.meshes[m].vertices[vCounter] = vertex[i];
                        model.meshes[m].animVertices[vCounter] = vertex[i];
                        vCounter++;
                    }
                }
            } break;
            case IQM_NORMAL:
            {
                normal = RL_MALLOC(iqmHeader->num_vertexes*3*sizeof(float));
                //fseek(iqmFile, va[i].offset, SEEK_SET);
                //fread(normal, iqmHeader->num_vertexes*3*sizeof(float), 1, iqmFile);
                memcpy(normal, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*3*sizeof(float));

                for (unsigned int m = 0; m < iqmHeader->num_meshes; m++)
                {
                    int vCounter = 0;
                    for (unsigned int i = imesh[m].first_vertex*3; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*3; i++)
                    {
                        model.meshes[m].normals[vCounter] = normal[i];
                        model.meshes[m].animNormals[vCounter] = normal[i];
                        vCounter++;
                    }
                }
            } break;
            case IQM_TEXCOORD:
            {
                text = RL_MALLOC(iqmHeader->num_vertexes*2*sizeof(float));
                //fseek(iqmFile, va[i].offset, SEEK_SET);
                //fread(text, iqmHeader->num_vertexes*2*sizeof(float), 1, iqmFile);
                memcpy(text, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*2*sizeof(float));

                for (unsigned int m = 0; m < iqmHeader->num_meshes; m++)
                {
                    int vCounter = 0;
                    for (unsigned int i = imesh[m].first_vertex*2; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*2; i++)
                    {
                        model.meshes[m].texcoords[vCounter] = text[i];
                        vCounter++;
                    }
                }
            } break;
            case IQM_BLENDINDEXES:
            {
                blendi = RL_MALLOC(iqmHeader->num_vertexes*4*sizeof(char));
                //fseek(iqmFile, va[i].offset, SEEK_SET);
                //fread(blendi, iqmHeader->num_vertexes*4*sizeof(char), 1, iqmFile);
                memcpy(blendi, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*4*sizeof(char));

                for (unsigned int m = 0; m < iqmHeader->num_meshes; m++)
                {
                    int boneCounter = 0;
                    for (unsigned int i = imesh[m].first_vertex*4; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*4; i++)
                    {
                        model.meshes[m].boneIds[boneCounter] = blendi[i];
                        boneCounter++;
                    }
                }
            } break;
            case IQM_BLENDWEIGHTS:
            {
                blendw = RL_MALLOC(iqmHeader->num_vertexes*4*sizeof(unsigned char));
                //fseek(iqmFile, va[i].offset, SEEK_SET);
                //fread(blendw, iqmHeader->num_vertexes*4*sizeof(unsigned char), 1, iqmFile);
                memcpy(blendw, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*4*sizeof(unsigned char));

                for (unsigned int m = 0; m < iqmHeader->num_meshes; m++)
                {
                    int boneCounter = 0;
                    for (unsigned int i = imesh[m].first_vertex*4; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*4; i++)
                    {
                        model.meshes[m].boneWeights[boneCounter] = blendw[i]/255.0f;
                        boneCounter++;
                    }
                }
            } break;
            case IQM_COLOR:
            {
                color = RL_MALLOC(iqmHeader->num_vertexes*4*sizeof(unsigned char));
                //fseek(iqmFile, va[i].offset, SEEK_SET);
                //fread(blendw, iqmHeader->num_vertexes*4*sizeof(unsigned char), 1, iqmFile);
                memcpy(color, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*4*sizeof(unsigned char));

                for (unsigned int m = 0; m < iqmHeader->num_meshes; m++)
                {
                    model.meshes[m].colors = RL_CALLOC(model.meshes[m].vertexCount*4, sizeof(unsigned char));

                    int vCounter = 0;
                    for (unsigned int i = imesh[m].first_vertex*4; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*4; i++)
                    {
                        model.meshes[m].colors[vCounter] = color[i];
                        vCounter++;
                    }
                }
            } break;
        }
    }

    // Bones (joints) data processing
    ijoint = RL_MALLOC(iqmHeader->num_joints*sizeof(IQMJoint));
    //fseek(iqmFile, iqmHeader->ofs_joints, SEEK_SET);
    //fread(ijoint, sizeof(IQMJoint), iqmHeader->num_joints, iqmFile);
    memcpy(ijoint, fileDataPtr + iqmHeader->ofs_joints, iqmHeader->num_joints*sizeof(IQMJoint));

    model.boneCount = iqmHeader->num_joints;
    model.bones = RL_MALLOC(iqmHeader->num_joints*sizeof(RL_BoneInfo));
    model.bindPose = RL_MALLOC(iqmHeader->num_joints*sizeof(RL_Transform));

    for (unsigned int i = 0; i < iqmHeader->num_joints; i++)
    {
        // Bones
        model.bones[i].parent = ijoint[i].parent;
        //fseek(iqmFile, iqmHeader->ofs_text + ijoint[i].name, SEEK_SET);
        //fread(model.bones[i].name, sizeof(char), BONE_NAME_LENGTH, iqmFile);
        memcpy(model.bones[i].name, fileDataPtr + iqmHeader->ofs_text + ijoint[i].name, BONE_NAME_LENGTH*sizeof(char));

        // Bind pose (base pose)
        model.bindPose[i].translation.x = ijoint[i].translate[0];
        model.bindPose[i].translation.y = ijoint[i].translate[1];
        model.bindPose[i].translation.z = ijoint[i].translate[2];

        model.bindPose[i].rotation.x = ijoint[i].rotate[0];
        model.bindPose[i].rotation.y = ijoint[i].rotate[1];
        model.bindPose[i].rotation.z = ijoint[i].rotate[2];
        model.bindPose[i].rotation.w = ijoint[i].rotate[3];

        model.bindPose[i].scale.x = ijoint[i].scale[0];
        model.bindPose[i].scale.y = ijoint[i].scale[1];
        model.bindPose[i].scale.z = ijoint[i].scale[2];
    }

    BuildPoseFromParentJoints(model.bones, model.boneCount, model.bindPose);
    
    for (int i = 0; i < model.meshCount; i++)
    {
        model.meshes[i].boneCount = model.boneCount;
        model.meshes[i].boneMatrices = RL_CALLOC(model.meshes[i].boneCount, sizeof(RL_Matrix));
        
        for (int j = 0; j < model.meshes[i].boneCount; j++)
        {
            model.meshes[i].boneMatrices[j] = MatrixIdentity();
        }
    }

    RL_UnloadFileData(fileData);

    RL_FREE(imesh);
    RL_FREE(tri);
    RL_FREE(va);
    RL_FREE(vertex);
    RL_FREE(normal);
    RL_FREE(text);
    RL_FREE(blendi);
    RL_FREE(blendw);
    RL_FREE(ijoint);
    RL_FREE(color);

    return model;
}

// Load IQM animation data
static RL_ModelAnimation *LoadModelAnimationsIQM(const char *fileName, int *animCount)
{
    #define IQM_MAGIC       "INTERQUAKEMODEL"   // IQM file magic number
    #define IQM_VERSION     2                   // only IQM version 2 supported

    int dataSize = 0;
    unsigned char *fileData = RL_LoadFileData(fileName, &dataSize);
    unsigned char *fileDataPtr = fileData;

    typedef struct IQMHeader {
        char magic[16];
        unsigned int version;
        unsigned int dataSize;
        unsigned int flags;
        unsigned int num_text, ofs_text;
        unsigned int num_meshes, ofs_meshes;
        unsigned int num_vertexarrays, num_vertexes, ofs_vertexarrays;
        unsigned int num_triangles, ofs_triangles, ofs_adjacency;
        unsigned int num_joints, ofs_joints;
        unsigned int num_poses, ofs_poses;
        unsigned int num_anims, ofs_anims;
        unsigned int num_frames, num_framechannels, ofs_frames, ofs_bounds;
        unsigned int num_comment, ofs_comment;
        unsigned int num_extensions, ofs_extensions;
    } IQMHeader;

    typedef struct IQMJoint {
        unsigned int name;
        int parent;
        float translate[3], rotate[4], scale[3];
    } IQMJoint;

    typedef struct IQMPose {
        int parent;
        unsigned int mask;
        float channeloffset[10];
        float channelscale[10];
    } IQMPose;

    typedef struct IQMAnim {
        unsigned int name;
        unsigned int first_frame, num_frames;
        float framerate;
        unsigned int flags;
    } IQMAnim;

    // In case file can not be read, return an empty model
    if (fileDataPtr == NULL) return NULL;

    // Read IQM header
    IQMHeader *iqmHeader = (IQMHeader *)fileDataPtr;

    if (memcmp(iqmHeader->magic, IQM_MAGIC, sizeof(IQM_MAGIC)) != 0)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] IQM file is not a valid model", fileName);
        return NULL;
    }

    if (iqmHeader->version != IQM_VERSION)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] IQM file version not supported (%i)", fileName, iqmHeader->version);
        return NULL;
    }

    // Get bones data
    IQMPose *poses = RL_MALLOC(iqmHeader->num_poses*sizeof(IQMPose));
    //fseek(iqmFile, iqmHeader->ofs_poses, SEEK_SET);
    //fread(poses, sizeof(IQMPose), iqmHeader->num_poses, iqmFile);
    memcpy(poses, fileDataPtr + iqmHeader->ofs_poses, iqmHeader->num_poses*sizeof(IQMPose));

    // Get animations data
    *animCount = iqmHeader->num_anims;
    IQMAnim *anim = RL_MALLOC(iqmHeader->num_anims*sizeof(IQMAnim));
    //fseek(iqmFile, iqmHeader->ofs_anims, SEEK_SET);
    //fread(anim, sizeof(IQMAnim), iqmHeader->num_anims, iqmFile);
    memcpy(anim, fileDataPtr + iqmHeader->ofs_anims, iqmHeader->num_anims*sizeof(IQMAnim));

    RL_ModelAnimation *animations = RL_MALLOC(iqmHeader->num_anims*sizeof(RL_ModelAnimation));

    // frameposes
    unsigned short *framedata = RL_MALLOC(iqmHeader->num_frames*iqmHeader->num_framechannels*sizeof(unsigned short));
    //fseek(iqmFile, iqmHeader->ofs_frames, SEEK_SET);
    //fread(framedata, sizeof(unsigned short), iqmHeader->num_frames*iqmHeader->num_framechannels, iqmFile);
    memcpy(framedata, fileDataPtr + iqmHeader->ofs_frames, iqmHeader->num_frames*iqmHeader->num_framechannels*sizeof(unsigned short));

    // joints
    IQMJoint *joints = RL_MALLOC(iqmHeader->num_joints*sizeof(IQMJoint));
    memcpy(joints, fileDataPtr + iqmHeader->ofs_joints, iqmHeader->num_joints*sizeof(IQMJoint));

    for (unsigned int a = 0; a < iqmHeader->num_anims; a++)
    {
        animations[a].frameCount = anim[a].num_frames;
        animations[a].boneCount = iqmHeader->num_poses;
        animations[a].bones = RL_MALLOC(iqmHeader->num_poses*sizeof(RL_BoneInfo));
        animations[a].framePoses = RL_MALLOC(anim[a].num_frames*sizeof(RL_Transform *));
        memcpy(animations[a].name, fileDataPtr + iqmHeader->ofs_text + anim[a].name, 32);   //  I don't like this 32 here
        RL_TraceLog(LOG_INFO, "IQM Anim %s", animations[a].name);
        // animations[a].framerate = anim.framerate;     // TODO: Use animation framerate data?

        for (unsigned int j = 0; j < iqmHeader->num_poses; j++)
        {
            // If animations and skeleton are in the same file, copy bone names to anim
            if (iqmHeader->num_joints > 0)
                memcpy(animations[a].bones[j].name, fileDataPtr + iqmHeader->ofs_text + joints[j].name, BONE_NAME_LENGTH*sizeof(char));
            else
                strcpy(animations[a].bones[j].name, "ANIMJOINTNAME"); // default bone name otherwise
            animations[a].bones[j].parent = poses[j].parent;
        }

        for (unsigned int j = 0; j < anim[a].num_frames; j++) animations[a].framePoses[j] = RL_MALLOC(iqmHeader->num_poses*sizeof(RL_Transform));

        int dcounter = anim[a].first_frame*iqmHeader->num_framechannels;

        for (unsigned int frame = 0; frame < anim[a].num_frames; frame++)
        {
            for (unsigned int i = 0; i < iqmHeader->num_poses; i++)
            {
                animations[a].framePoses[frame][i].translation.x = poses[i].channeloffset[0];

                if (poses[i].mask & 0x01)
                {
                    animations[a].framePoses[frame][i].translation.x += framedata[dcounter]*poses[i].channelscale[0];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].translation.y = poses[i].channeloffset[1];

                if (poses[i].mask & 0x02)
                {
                    animations[a].framePoses[frame][i].translation.y += framedata[dcounter]*poses[i].channelscale[1];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].translation.z = poses[i].channeloffset[2];

                if (poses[i].mask & 0x04)
                {
                    animations[a].framePoses[frame][i].translation.z += framedata[dcounter]*poses[i].channelscale[2];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].rotation.x = poses[i].channeloffset[3];

                if (poses[i].mask & 0x08)
                {
                    animations[a].framePoses[frame][i].rotation.x += framedata[dcounter]*poses[i].channelscale[3];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].rotation.y = poses[i].channeloffset[4];

                if (poses[i].mask & 0x10)
                {
                    animations[a].framePoses[frame][i].rotation.y += framedata[dcounter]*poses[i].channelscale[4];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].rotation.z = poses[i].channeloffset[5];

                if (poses[i].mask & 0x20)
                {
                    animations[a].framePoses[frame][i].rotation.z += framedata[dcounter]*poses[i].channelscale[5];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].rotation.w = poses[i].channeloffset[6];

                if (poses[i].mask & 0x40)
                {
                    animations[a].framePoses[frame][i].rotation.w += framedata[dcounter]*poses[i].channelscale[6];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].scale.x = poses[i].channeloffset[7];

                if (poses[i].mask & 0x80)
                {
                    animations[a].framePoses[frame][i].scale.x += framedata[dcounter]*poses[i].channelscale[7];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].scale.y = poses[i].channeloffset[8];

                if (poses[i].mask & 0x100)
                {
                    animations[a].framePoses[frame][i].scale.y += framedata[dcounter]*poses[i].channelscale[8];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].scale.z = poses[i].channeloffset[9];

                if (poses[i].mask & 0x200)
                {
                    animations[a].framePoses[frame][i].scale.z += framedata[dcounter]*poses[i].channelscale[9];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].rotation = QuaternionNormalize(animations[a].framePoses[frame][i].rotation);
            }
        }

        // Build frameposes
        for (unsigned int frame = 0; frame < anim[a].num_frames; frame++)
        {
            for (int i = 0; i < animations[a].boneCount; i++)
            {
                if (animations[a].bones[i].parent >= 0)
                {
                    animations[a].framePoses[frame][i].rotation = QuaternionMultiply(animations[a].framePoses[frame][animations[a].bones[i].parent].rotation, animations[a].framePoses[frame][i].rotation);
                    animations[a].framePoses[frame][i].translation = Vector3RotateByQuaternion(animations[a].framePoses[frame][i].translation, animations[a].framePoses[frame][animations[a].bones[i].parent].rotation);
                    animations[a].framePoses[frame][i].translation = Vector3Add(animations[a].framePoses[frame][i].translation, animations[a].framePoses[frame][animations[a].bones[i].parent].translation);
                    animations[a].framePoses[frame][i].scale = Vector3Multiply(animations[a].framePoses[frame][i].scale, animations[a].framePoses[frame][animations[a].bones[i].parent].scale);
                }
            }
        }
    }

    RL_UnloadFileData(fileData);

    RL_FREE(joints);
    RL_FREE(framedata);
    RL_FREE(poses);
    RL_FREE(anim);

    return animations;
}

#endif

#if defined(SUPPORT_FILEFORMAT_GLTF)
// Load file data callback for cgltf
static cgltf_result LoadFileGLTFCallback(const struct cgltf_memory_options *memoryOptions, const struct cgltf_file_options *fileOptions, const char *path, cgltf_size *size, void **data)
{
    int filesize;
    unsigned char *filedata = RL_LoadFileData(path, &filesize);

    if (filedata == NULL) return cgltf_result_io_error;

    *size = filesize;
    *data = filedata;

    return cgltf_result_success;
}

// Release file data callback for cgltf
static void ReleaseFileGLTFCallback(const struct cgltf_memory_options *memoryOptions, const struct cgltf_file_options *fileOptions, void *data)
{
    RL_UnloadFileData(data);
}

// Load image from different glTF provided methods (uri, path, buffer_view)
static RL_Image LoadImageFromCgltfImage(cgltf_image *cgltfImage, const char *texPath)
{
    RL_Image image = { 0 };

    if (cgltfImage->uri != NULL)     // Check if image data is provided as an uri (base64 or path)
    {
        if ((strlen(cgltfImage->uri) > 5) &&
            (cgltfImage->uri[0] == 'd') &&
            (cgltfImage->uri[1] == 'a') &&
            (cgltfImage->uri[2] == 't') &&
            (cgltfImage->uri[3] == 'a') &&
            (cgltfImage->uri[4] == ':'))     // Check if image is provided as base64 text data
        {
            // Data URI Format: data:<mediatype>;base64,<data>

            // Find the comma
            int i = 0;
            while ((cgltfImage->uri[i] != ',') && (cgltfImage->uri[i] != 0)) i++;

            if (cgltfImage->uri[i] == 0) TRACELOG(LOG_WARNING, "IMAGE: glTF data URI is not a valid image");
            else
            {
                int base64Size = (int)strlen(cgltfImage->uri + i + 1);
                while (cgltfImage->uri[i + base64Size] == '=') base64Size--;    // Ignore optional paddings
                int numberOfEncodedBits = base64Size*6 - (base64Size*6) % 8 ;   // Encoded bits minus extra bits, so it becomes a multiple of 8 bits
                int outSize = numberOfEncodedBits/8 ;                           // Actual encoded bytes
                void *data = NULL;

                cgltf_options options = { 0 };
                options.file.read = LoadFileGLTFCallback;
                options.file.release = ReleaseFileGLTFCallback;
                cgltf_result result = cgltf_load_buffer_base64(&options, outSize, cgltfImage->uri + i + 1, &data);

                if (result == cgltf_result_success)
                {
                    image = RL_LoadImageFromMemory(".png", (unsigned char *)data, outSize);
                    RL_FREE(data);
                }
            }
        }
        else     // Check if image is provided as image path
        {
            image = RL_LoadImage(RL_TextFormat("%s/%s", texPath, cgltfImage->uri));
        }
    }
    else if (cgltfImage->buffer_view->buffer->data != NULL)    // Check if image is provided as data buffer
    {
        unsigned char *data = RL_MALLOC(cgltfImage->buffer_view->size);
        int offset = (int)cgltfImage->buffer_view->offset;
        int stride = (int)cgltfImage->buffer_view->stride? (int)cgltfImage->buffer_view->stride : 1;

        // Copy buffer data to memory for loading
        for (unsigned int i = 0; i < cgltfImage->buffer_view->size; i++)
        {
            data[i] = ((unsigned char *)cgltfImage->buffer_view->buffer->data)[offset];
            offset += stride;
        }

        // Check mime_type for image: (cgltfImage->mime_type == "image/png")
        // NOTE: Detected that some models define mime_type as "image\\/png"
        if ((strcmp(cgltfImage->mime_type, "image\\/png") == 0) ||
            (strcmp(cgltfImage->mime_type, "image/png") == 0)) image = RL_LoadImageFromMemory(".png", data, (int)cgltfImage->buffer_view->size);
        else if ((strcmp(cgltfImage->mime_type, "image\\/jpeg") == 0) ||
                 (strcmp(cgltfImage->mime_type, "image/jpeg") == 0)) image = RL_LoadImageFromMemory(".jpg", data, (int)cgltfImage->buffer_view->size);
        else TRACELOG(LOG_WARNING, "MODEL: glTF image data MIME type not recognized", RL_TextFormat("%s/%s", texPath, cgltfImage->uri));

        RL_FREE(data);
    }

    return image;
}

// Load bone info from GLTF skin data
static RL_BoneInfo *LoadBoneInfoGLTF(cgltf_skin skin, int *boneCount)
{
    *boneCount = (int)skin.joints_count;
    RL_BoneInfo *bones = RL_MALLOC(skin.joints_count*sizeof(RL_BoneInfo));

    for (unsigned int i = 0; i < skin.joints_count; i++)
    {
        cgltf_node node = *skin.joints[i];
        if (node.name != NULL)
        {
            strncpy(bones[i].name, node.name, sizeof(bones[i].name));
            bones[i].name[sizeof(bones[i].name) - 1] = '\0';
        }

        // Find parent bone index
        int parentIndex = -1;

        for (unsigned int j = 0; j < skin.joints_count; j++)
        {
            if (skin.joints[j] == node.parent)
            {
                parentIndex = (int)j;
                break;
            }
        }

        bones[i].parent = parentIndex;
    }

    return bones;
}

// Load glTF file into model struct, .gltf and .glb supported
static RL_Model LoadGLTF(const char *fileName)
{
    /*********************************************************************************************

        Function implemented by Wilhem Barbier(@wbrbr), with modifications by Tyler Bezera(@gamerfiend)
        RL_Transform handling implemented by Paul Melis (@paulmelis).
        Reviewed by Ramon Santamaria (@raysan5)

        FEATURES:
          - Supports .gltf and .glb files
          - Supports embedded (base64) or external textures
          - Supports PBR metallic/roughness flow, loads material textures, values and colors
                     PBR specular/glossiness flow and extended texture flows not supported
          - Supports multiple meshes per model (every primitives is loaded as a separate mesh)
          - Supports basic animations
          - Transforms, including parent-child relations, are applied on the mesh data, but the
            hierarchy is not kept (as it can't be represented).
          - RL_Mesh instances in the glTF file (i.e. same mesh linked from multiple nodes)
            are turned into separate raylib Meshes.

        RESTRICTIONS:
          - Only triangle meshes supported
          - Vertex attribute types and formats supported:
              > Vertices (position): vec3: float
              > Normals: vec3: float
              > Texcoords: vec2: float
              > Colors: vec4: u8, u16, f32 (normalized)
              > Indices: u16, u32 (truncated to u16)
          - Scenes defined in the glTF file are ignored. All nodes in the file
            are used.

    ***********************************************************************************************/

    // Macro to simplify attributes loading code
    #define LOAD_ATTRIBUTE(accesor, numComp, dataType, dstPtr) \
    { \
        int n = 0; \
        dataType *buffer = (dataType *)accesor->buffer_view->buffer->data + accesor->buffer_view->offset/sizeof(dataType) + accesor->offset/sizeof(dataType); \
        for (unsigned int k = 0; k < accesor->count; k++) \
        {\
            for (int l = 0; l < numComp; l++) \
            {\
                dstPtr[numComp*k + l] = buffer[n + l];\
            }\
            n += (int)(accesor->stride/sizeof(dataType));\
        }\
    }

    RL_Model model = { 0 };

    // glTF file loading
    int dataSize = 0;
    unsigned char *fileData = RL_LoadFileData(fileName, &dataSize);

    if (fileData == NULL) return model;

    // glTF data loading
    cgltf_options options = { 0 };
    options.file.read = LoadFileGLTFCallback;
    options.file.release = ReleaseFileGLTFCallback;
    cgltf_data *data = NULL;
    cgltf_result result = cgltf_parse(&options, fileData, dataSize, &data);

    if (result == cgltf_result_success)
    {
        if (data->file_type == cgltf_file_type_glb) TRACELOG(LOG_INFO, "MODEL: [%s] RL_Model basic data (glb) loaded successfully", fileName);
        else if (data->file_type == cgltf_file_type_gltf) TRACELOG(LOG_INFO, "MODEL: [%s] RL_Model basic data (glTF) loaded successfully", fileName);
        else TRACELOG(LOG_WARNING, "MODEL: [%s] RL_Model format not recognized", fileName);

        TRACELOG(LOG_INFO, "    > Meshes count: %i", data->meshes_count);
        TRACELOG(LOG_INFO, "    > Materials count: %i (+1 default)", data->materials_count);
        TRACELOG(LOG_DEBUG, "    > Buffers count: %i", data->buffers_count);
        TRACELOG(LOG_DEBUG, "    > Images count: %i", data->images_count);
        TRACELOG(LOG_DEBUG, "    > Textures count: %i", data->textures_count);

        // Force reading data buffers (fills buffer_view->buffer->data)
        // NOTE: If an uri is defined to base64 data or external path, it's automatically loaded
        result = cgltf_load_buffers(&options, data, fileName);
        if (result != cgltf_result_success) TRACELOG(LOG_INFO, "MODEL: [%s] Failed to load mesh/material buffers", fileName);

        int primitivesCount = 0;
        // NOTE: We will load every primitive in the glTF as a separate raylib RL_Mesh.
        // Determine total number of meshes needed from the node hierarchy.
        for (unsigned int i = 0; i < data->nodes_count; i++)
        {
            cgltf_node *node = &(data->nodes[i]);
            cgltf_mesh *mesh = node->mesh;
            if (!mesh)
                continue;

            for (unsigned int p = 0; p < mesh->primitives_count; p++)
            {
                if (mesh->primitives[p].type == cgltf_primitive_type_triangles)
                    primitivesCount++;
            }
        }
        TRACELOG(LOG_DEBUG, "    > Primitives (triangles only) count based on hierarchy : %i", primitivesCount);

        // Load our model data: meshes and materials
        model.meshCount = primitivesCount;
        model.meshes = RL_CALLOC(model.meshCount, sizeof(RL_Mesh));

        // NOTE: We keep an extra slot for default material, in case some mesh requires it
        model.materialCount = (int)data->materials_count + 1;
        model.materials = RL_CALLOC(model.materialCount, sizeof(RL_Material));
        model.materials[0] = RL_LoadMaterialDefault();     // Load default material (index: 0)

        // Load mesh-material indices, by default all meshes are mapped to material index: 0
        model.meshMaterial = RL_CALLOC(model.meshCount, sizeof(int));

        // Load materials data
        //----------------------------------------------------------------------------------------------------
        for (unsigned int i = 0, j = 1; i < data->materials_count; i++, j++)
        {
            model.materials[j] = RL_LoadMaterialDefault();
            const char *texPath = RL_GetDirectoryPath(fileName);

            // Check glTF material flow: PBR metallic/roughness flow
            // NOTE: Alternatively, materials can follow PBR specular/glossiness flow
            if (data->materials[i].has_pbr_metallic_roughness)
            {
                // Load base color texture (albedo)
                if (data->materials[i].pbr_metallic_roughness.base_color_texture.texture)
                {
                    RL_Image imAlbedo = LoadImageFromCgltfImage(data->materials[i].pbr_metallic_roughness.base_color_texture.texture->image, texPath);
                    if (imAlbedo.data != NULL)
                    {
                        model.materials[j].maps[MATERIAL_MAP_ALBEDO].texture = RL_LoadTextureFromImage(imAlbedo);
                        RL_UnloadImage(imAlbedo);
                    }
                }
                // Load base color factor (tint)
                model.materials[j].maps[MATERIAL_MAP_ALBEDO].color.r = (unsigned char)(data->materials[i].pbr_metallic_roughness.base_color_factor[0]*255);
                model.materials[j].maps[MATERIAL_MAP_ALBEDO].color.g = (unsigned char)(data->materials[i].pbr_metallic_roughness.base_color_factor[1]*255);
                model.materials[j].maps[MATERIAL_MAP_ALBEDO].color.b = (unsigned char)(data->materials[i].pbr_metallic_roughness.base_color_factor[2]*255);
                model.materials[j].maps[MATERIAL_MAP_ALBEDO].color.a = (unsigned char)(data->materials[i].pbr_metallic_roughness.base_color_factor[3]*255);

                // Load metallic/roughness texture
                if (data->materials[i].pbr_metallic_roughness.metallic_roughness_texture.texture)
                {
                    RL_Image imMetallicRoughness = LoadImageFromCgltfImage(data->materials[i].pbr_metallic_roughness.metallic_roughness_texture.texture->image, texPath);
                    if (imMetallicRoughness.data != NULL)
                    {
                        model.materials[j].maps[MATERIAL_MAP_ROUGHNESS].texture = RL_LoadTextureFromImage(imMetallicRoughness);
                        RL_UnloadImage(imMetallicRoughness);
                    }

                    // Load metallic/roughness material properties
                    float roughness = data->materials[i].pbr_metallic_roughness.roughness_factor;
                    model.materials[j].maps[MATERIAL_MAP_ROUGHNESS].value = roughness;

                    float metallic = data->materials[i].pbr_metallic_roughness.metallic_factor;
                    model.materials[j].maps[MATERIAL_MAP_METALNESS].value = metallic;
                }

                // Load normal texture
                if (data->materials[i].normal_texture.texture)
                {
                    RL_Image imNormal = LoadImageFromCgltfImage(data->materials[i].normal_texture.texture->image, texPath);
                    if (imNormal.data != NULL)
                    {
                        model.materials[j].maps[MATERIAL_MAP_NORMAL].texture = RL_LoadTextureFromImage(imNormal);
                        RL_UnloadImage(imNormal);
                    }
                }

                // Load ambient occlusion texture
                if (data->materials[i].occlusion_texture.texture)
                {
                    RL_Image imOcclusion = LoadImageFromCgltfImage(data->materials[i].occlusion_texture.texture->image, texPath);
                    if (imOcclusion.data != NULL)
                    {
                        model.materials[j].maps[MATERIAL_MAP_OCCLUSION].texture = RL_LoadTextureFromImage(imOcclusion);
                        RL_UnloadImage(imOcclusion);
                    }
                }

                // Load emissive texture
                if (data->materials[i].emissive_texture.texture)
                {
                    RL_Image imEmissive = LoadImageFromCgltfImage(data->materials[i].emissive_texture.texture->image, texPath);
                    if (imEmissive.data != NULL)
                    {
                        model.materials[j].maps[MATERIAL_MAP_EMISSION].texture = RL_LoadTextureFromImage(imEmissive);
                        RL_UnloadImage(imEmissive);
                    }

                    // Load emissive color factor
                    model.materials[j].maps[MATERIAL_MAP_EMISSION].color.r = (unsigned char)(data->materials[i].emissive_factor[0]*255);
                    model.materials[j].maps[MATERIAL_MAP_EMISSION].color.g = (unsigned char)(data->materials[i].emissive_factor[1]*255);
                    model.materials[j].maps[MATERIAL_MAP_EMISSION].color.b = (unsigned char)(data->materials[i].emissive_factor[2]*255);
                    model.materials[j].maps[MATERIAL_MAP_EMISSION].color.a = 255;
                }
            }

            // Other possible materials not supported by raylib pipeline:
            // has_clearcoat, has_transmission, has_volume, has_ior, has specular, has_sheen
        }

        // Visit each node in the hierarchy and process any mesh linked from it.
        // Each primitive within a glTF node becomes a Raylib RL_Mesh.
        // The local-to-world transform of each node is used to transform the
        // points/normals/tangents of the created RL_Mesh(es).
        // Any glTF mesh linked from more than one Node (i.e. instancing)
        // is turned into multiple RL_Mesh's, as each Node will have its own
        // transform applied.
        // Note: the code below disregards the scenes defined in the file, all nodes are used.
        //----------------------------------------------------------------------------------------------------
        int meshIndex = 0;
        for (unsigned int i = 0; i < data->nodes_count; i++)
        {
            cgltf_node *node = &(data->nodes[i]);

            cgltf_mesh *mesh = node->mesh;
            if (!mesh)
                continue;

            cgltf_float worldTransform[16];
            cgltf_node_transform_world(node, worldTransform);

            RL_Matrix worldMatrix = {
                worldTransform[0], worldTransform[4], worldTransform[8], worldTransform[12],
                worldTransform[1], worldTransform[5], worldTransform[9], worldTransform[13],
                worldTransform[2], worldTransform[6], worldTransform[10], worldTransform[14],
                worldTransform[3], worldTransform[7], worldTransform[11], worldTransform[15]
            };

            RL_Matrix worldMatrixNormals = MatrixTranspose(MatrixInvert(worldMatrix));

            for (unsigned int p = 0; p < mesh->primitives_count; p++)
            {
                // NOTE: We only support primitives defined by triangles
                // Other alternatives: points, lines, line_strip, triangle_strip
                if (mesh->primitives[p].type != cgltf_primitive_type_triangles) continue;

                // NOTE: Attributes data could be provided in several data formats (8, 8u, 16u, 32...),
                // Only some formats for each attribute type are supported, read info at the top of this function!

                for (unsigned int j = 0; j < mesh->primitives[p].attributes_count; j++)
                {
                    // Check the different attributes for every primitive
                    if (mesh->primitives[p].attributes[j].type == cgltf_attribute_type_position)      // POSITION, vec3, float
                    {
                        cgltf_accessor *attribute = mesh->primitives[p].attributes[j].data;

                        // WARNING: SPECS: POSITION accessor MUST have its min and max properties defined

                        if ((attribute->type == cgltf_type_vec3) && (attribute->component_type == cgltf_component_type_r_32f))
                        {
                            // Init raylib mesh vertices to copy glTF attribute data
                            model.meshes[meshIndex].vertexCount = (int)attribute->count;
                            model.meshes[meshIndex].vertices = RL_MALLOC(attribute->count*3*sizeof(float));

                            // Load 3 components of float data type into mesh.vertices
                            LOAD_ATTRIBUTE(attribute, 3, float, model.meshes[meshIndex].vertices)

                            // RL_Transform the vertices
                            float *vertices = model.meshes[meshIndex].vertices;
                            for (unsigned int k = 0; k < attribute->count; k++)
                            {
                                RL_Vector3 vt = Vector3Transform((RL_Vector3){ vertices[3*k], vertices[3*k+1], vertices[3*k+2] }, worldMatrix);
                                vertices[3*k] = vt.x;
                                vertices[3*k+1] = vt.y;
                                vertices[3*k+2] = vt.z;
                            }
                        }
                        else TRACELOG(LOG_WARNING, "MODEL: [%s] Vertices attribute data format not supported, use vec3 float", fileName);
                    }
                    else if (mesh->primitives[p].attributes[j].type == cgltf_attribute_type_normal)   // NORMAL, vec3, float
                    {
                        cgltf_accessor *attribute = mesh->primitives[p].attributes[j].data;

                        if ((attribute->type == cgltf_type_vec3) && (attribute->component_type == cgltf_component_type_r_32f))
                        {
                            // Init raylib mesh normals to copy glTF attribute data
                            model.meshes[meshIndex].normals = RL_MALLOC(attribute->count*3*sizeof(float));

                            // Load 3 components of float data type into mesh.normals
                            LOAD_ATTRIBUTE(attribute, 3, float, model.meshes[meshIndex].normals)

                            // RL_Transform the normals
                            float *normals = model.meshes[meshIndex].normals;
                            for (unsigned int k = 0; k < attribute->count; k++)
                            {
                                RL_Vector3 nt = Vector3Transform((RL_Vector3){ normals[3*k], normals[3*k+1], normals[3*k+2] }, worldMatrixNormals);
                                normals[3*k] = nt.x;
                                normals[3*k+1] = nt.y;
                                normals[3*k+2] = nt.z;
                            }
                        }
                        else TRACELOG(LOG_WARNING, "MODEL: [%s] Normal attribute data format not supported, use vec3 float", fileName);
                    }
                    else if (mesh->primitives[p].attributes[j].type == cgltf_attribute_type_tangent)   // TANGENT, vec3, float
                    {
                        cgltf_accessor *attribute = mesh->primitives[p].attributes[j].data;

                        if ((attribute->type == cgltf_type_vec4) && (attribute->component_type == cgltf_component_type_r_32f))
                        {
                            // Init raylib mesh tangent to copy glTF attribute data
                            model.meshes[meshIndex].tangents = RL_MALLOC(attribute->count*4*sizeof(float));

                            // Load 4 components of float data type into mesh.tangents
                            LOAD_ATTRIBUTE(attribute, 4, float, model.meshes[meshIndex].tangents)

                            // RL_Transform the tangents
                            float *tangents = model.meshes[meshIndex].tangents;
                            for (unsigned int k = 0; k < attribute->count; k++)
                            {
                                RL_Vector3 tt = Vector3Transform((RL_Vector3){ tangents[3*k], tangents[3*k+1], tangents[3*k+2] }, worldMatrix);
                                tangents[3*k] = tt.x;
                                tangents[3*k+1] = tt.y;
                                tangents[3*k+2] = tt.z;
                            }
                        }
                        else TRACELOG(LOG_WARNING, "MODEL: [%s] Tangent attribute data format not supported, use vec4 float", fileName);
                    }
                    else if (mesh->primitives[p].attributes[j].type == cgltf_attribute_type_texcoord) // TEXCOORD_n, vec2, float/u8n/u16n
                    {
                        // Support up to 2 texture coordinates attributes
                        float *texcoordPtr = NULL;

                        cgltf_accessor *attribute = mesh->primitives[p].attributes[j].data;

                        if (attribute->type == cgltf_type_vec2)
                        {
                            if (attribute->component_type == cgltf_component_type_r_32f)  // vec2, float
                            {
                                // Init raylib mesh texcoords to copy glTF attribute data
                                texcoordPtr = (float *)RL_MALLOC(attribute->count*2*sizeof(float));

                                // Load 3 components of float data type into mesh.texcoords
                                LOAD_ATTRIBUTE(attribute, 2, float, texcoordPtr)
                            }
                            else if (attribute->component_type == cgltf_component_type_r_8u) // vec2, u8n
                            {
                                // Init raylib mesh texcoords to copy glTF attribute data
                                texcoordPtr = (float *)RL_MALLOC(attribute->count*2*sizeof(float));

                                // Load data into a temp buffer to be converted to raylib data type
                                unsigned char *temp = (unsigned char *)RL_MALLOC(attribute->count*2*sizeof(unsigned char));
                                LOAD_ATTRIBUTE(attribute, 2, unsigned char, temp);

                                // Convert data to raylib texcoord data type (float)
                                for (unsigned int t = 0; t < attribute->count*2; t++) texcoordPtr[t] = (float)temp[t]/255.0f;

                                RL_FREE(temp);
                            }
                            else if (attribute->component_type == cgltf_component_type_r_16u) // vec2, u16n
                            {
                                // Init raylib mesh texcoords to copy glTF attribute data
                                texcoordPtr = (float *)RL_MALLOC(attribute->count*2*sizeof(float));

                                // Load data into a temp buffer to be converted to raylib data type
                                unsigned short *temp = (unsigned short *)RL_MALLOC(attribute->count*2*sizeof(unsigned short));
                                LOAD_ATTRIBUTE(attribute, 2, unsigned short, temp);

                                // Convert data to raylib texcoord data type (float)
                                for (unsigned int t = 0; t < attribute->count*2; t++) texcoordPtr[t] = (float)temp[t]/65535.0f;

                                RL_FREE(temp);
                            }
                            else TRACELOG(LOG_WARNING, "MODEL: [%s] Texcoords attribute data format not supported", fileName);
                        }
                        else TRACELOG(LOG_WARNING, "MODEL: [%s] Texcoords attribute data format not supported, use vec2 float", fileName);

                        int index = mesh->primitives[p].attributes[j].index;
                        if (index == 0) model.meshes[meshIndex].texcoords = texcoordPtr;
                        else if (index == 1) model.meshes[meshIndex].texcoords2 = texcoordPtr;
                        else
                        {
                            TRACELOG(LOG_WARNING, "MODEL: [%s] No more than 2 texture coordinates attributes supported", fileName);
                            if (texcoordPtr != NULL) RL_FREE(texcoordPtr);
                        }
                    }
                    else if (mesh->primitives[p].attributes[j].type == cgltf_attribute_type_color)    // COLOR_n, vec3/vec4, float/u8n/u16n
                    {
                        cgltf_accessor *attribute = mesh->primitives[p].attributes[j].data;

                        // WARNING: SPECS: All components of each COLOR_n accessor element MUST be clamped to [0.0, 1.0] range

                        if (attribute->type == cgltf_type_vec3)  // RGB
                        {
                            if (attribute->component_type == cgltf_component_type_r_8u)
                            {
                                // Init raylib mesh color to copy glTF attribute data
                                model.meshes[meshIndex].colors = RL_MALLOC(attribute->count*4*sizeof(unsigned char));

                                // Load data into a temp buffer to be converted to raylib data type
                                unsigned char *temp = RL_MALLOC(attribute->count*3*sizeof(unsigned char));
                                LOAD_ATTRIBUTE(attribute, 3, unsigned char, temp);

                                // Convert data to raylib color data type (4 bytes)
                                for (unsigned int c = 0, k = 0; c < (attribute->count*4 - 3); c += 4, k += 3)
                                {
                                    model.meshes[meshIndex].colors[c] = temp[k];
                                    model.meshes[meshIndex].colors[c + 1] = temp[k + 1];
                                    model.meshes[meshIndex].colors[c + 2] = temp[k + 2];
                                    model.meshes[meshIndex].colors[c + 3] = 255;
                                }

                                RL_FREE(temp);
                            }
                            else if (attribute->component_type == cgltf_component_type_r_16u)
                            {
                                // Init raylib mesh color to copy glTF attribute data
                                model.meshes[meshIndex].colors = RL_MALLOC(attribute->count*4*sizeof(unsigned char));

                                // Load data into a temp buffer to be converted to raylib data type
                                unsigned short *temp = RL_MALLOC(attribute->count*3*sizeof(unsigned short));
                                LOAD_ATTRIBUTE(attribute, 3, unsigned short, temp);

                                // Convert data to raylib color data type (4 bytes)
                                for (unsigned int c = 0, k = 0; c < (attribute->count*4 - 3); c += 4, k += 3)
                                {
                                    model.meshes[meshIndex].colors[c] = (unsigned char)(((float)temp[k]/65535.0f)*255.0f);
                                    model.meshes[meshIndex].colors[c + 1] = (unsigned char)(((float)temp[k + 1]/65535.0f)*255.0f);
                                    model.meshes[meshIndex].colors[c + 2] = (unsigned char)(((float)temp[k + 2]/65535.0f)*255.0f);
                                    model.meshes[meshIndex].colors[c + 3] = 255;
                                }

                                RL_FREE(temp);
                            }
                            else if (attribute->component_type == cgltf_component_type_r_32f)
                            {
                                // Init raylib mesh color to copy glTF attribute data
                                model.meshes[meshIndex].colors = RL_MALLOC(attribute->count*4*sizeof(unsigned char));

                                // Load data into a temp buffer to be converted to raylib data type
                                float *temp = RL_MALLOC(attribute->count*3*sizeof(float));
                                LOAD_ATTRIBUTE(attribute, 3, float, temp);

                                // Convert data to raylib color data type (4 bytes)
                                for (unsigned int c = 0, k = 0; c < (attribute->count*4 - 3); c += 4, k += 3)
                                {
                                    model.meshes[meshIndex].colors[c] = (unsigned char)(temp[k]*255.0f);
                                    model.meshes[meshIndex].colors[c + 1] = (unsigned char)(temp[k + 1]*255.0f);
                                    model.meshes[meshIndex].colors[c + 2] = (unsigned char)(temp[k + 2]*255.0f);
                                    model.meshes[meshIndex].colors[c + 3] = 255;
                                }

                                RL_FREE(temp);
                            }
                            else TRACELOG(LOG_WARNING, "MODEL: [%s] RL_Color attribute data format not supported", fileName);
                        }
                        else if (attribute->type == cgltf_type_vec4) // RGBA
                        {
                            if (attribute->component_type == cgltf_component_type_r_8u)
                            {
                                // Init raylib mesh color to copy glTF attribute data
                                model.meshes[meshIndex].colors = RL_MALLOC(attribute->count*4*sizeof(unsigned char));

                                // Load 4 components of unsigned char data type into mesh.colors
                                LOAD_ATTRIBUTE(attribute, 4, unsigned char, model.meshes[meshIndex].colors)
                            }
                            else if (attribute->component_type == cgltf_component_type_r_16u)
                            {
                                // Init raylib mesh color to copy glTF attribute data
                                model.meshes[meshIndex].colors = RL_MALLOC(attribute->count*4*sizeof(unsigned char));

                                // Load data into a temp buffer to be converted to raylib data type
                                unsigned short *temp = RL_MALLOC(attribute->count*4*sizeof(unsigned short));
                                LOAD_ATTRIBUTE(attribute, 4, unsigned short, temp);

                                // Convert data to raylib color data type (4 bytes)
                                for (unsigned int c = 0; c < attribute->count*4; c++) model.meshes[meshIndex].colors[c] = (unsigned char)(((float)temp[c]/65535.0f)*255.0f);

                                RL_FREE(temp);
                            }
                            else if (attribute->component_type == cgltf_component_type_r_32f)
                            {
                                // Init raylib mesh color to copy glTF attribute data
                                model.meshes[meshIndex].colors = RL_MALLOC(attribute->count*4*sizeof(unsigned char));

                                // Load data into a temp buffer to be converted to raylib data type
                                float *temp = RL_MALLOC(attribute->count*4*sizeof(float));
                                LOAD_ATTRIBUTE(attribute, 4, float, temp);

                                // Convert data to raylib color data type (4 bytes), we expect the color data normalized
                                for (unsigned int c = 0; c < attribute->count*4; c++) model.meshes[meshIndex].colors[c] = (unsigned char)(temp[c]*255.0f);

                                RL_FREE(temp);
                            }
                            else TRACELOG(LOG_WARNING, "MODEL: [%s] RL_Color attribute data format not supported", fileName);
                        }
                        else TRACELOG(LOG_WARNING, "MODEL: [%s] RL_Color attribute data format not supported", fileName);


                    }

                    // NOTE: Attributes related to animations are processed separately
                }

                // Load primitive indices data (if provided)
                if (mesh->primitives[p].indices != NULL)
                {
                    cgltf_accessor *attribute = mesh->primitives[p].indices;

                    model.meshes[meshIndex].triangleCount = (int)attribute->count/3;

                    if (attribute->component_type == cgltf_component_type_r_16u)
                    {
                        // Init raylib mesh indices to copy glTF attribute data
                        model.meshes[meshIndex].indices = RL_MALLOC(attribute->count*sizeof(unsigned short));

                        // Load unsigned short data type into mesh.indices
                        LOAD_ATTRIBUTE(attribute, 1, unsigned short, model.meshes[meshIndex].indices)
                    }
                    else if (attribute->component_type == cgltf_component_type_r_32u)
                    {
                        // Init raylib mesh indices to copy glTF attribute data
                        model.meshes[meshIndex].indices = RL_MALLOC(attribute->count*sizeof(unsigned short));

                        // Load data into a temp buffer to be converted to raylib data type
                        unsigned int *temp = RL_MALLOC(attribute->count*sizeof(unsigned int));
                        LOAD_ATTRIBUTE(attribute, 1, unsigned int, temp);

                        // Convert data to raylib indices data type (unsigned short)
                        for (unsigned int d = 0; d < attribute->count; d++) model.meshes[meshIndex].indices[d] = (unsigned short)temp[d];

                        TRACELOG(LOG_WARNING, "MODEL: [%s] Indices data converted from u32 to u16, possible loss of data", fileName);

                        RL_FREE(temp);
                    }
                    else TRACELOG(LOG_WARNING, "MODEL: [%s] Indices data format not supported, use u16", fileName);
                }
                else model.meshes[meshIndex].triangleCount = model.meshes[meshIndex].vertexCount/3;    // Unindexed mesh

                // Assign to the primitive mesh the corresponding material index
                // NOTE: If no material defined, mesh uses the already assigned default material (index: 0)
                for (unsigned int m = 0; m < data->materials_count; m++)
                {
                    // The primitive actually keeps the pointer to the corresponding material,
                    // raylib instead assigns to the mesh the by its index, as loaded in model.materials array
                    // To get the index, we check if material pointers match, and we assign the corresponding index,
                    // skipping index 0, the default material
                    if (&data->materials[m] == mesh->primitives[p].material)
                    {
                        model.meshMaterial[meshIndex] = m + 1;
                        break;
                    }
                }

                meshIndex++;       // Move to next mesh
            }
        }

        // Load glTF meshes animation data
        // REF: https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#skins
        // REF: https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#skinned-mesh-attributes
        //
        // LIMITATIONS:
        //  - Only supports 1 armature per file, and skips loading it if there are multiple armatures
        //  - Only supports linear interpolation (default method in Blender when checked "Always Sample Animations" when exporting a GLTF file)
        //  - Only supports translation/rotation/scale animation channel.path, weights not considered (i.e. morph targets)
        //----------------------------------------------------------------------------------------------------
        if (data->skins_count == 1)
        {
            cgltf_skin skin = data->skins[0];
            model.bones = LoadBoneInfoGLTF(skin, &model.boneCount);
            model.bindPose = RL_MALLOC(model.boneCount*sizeof(RL_Transform));

            for (int i = 0; i < model.boneCount; i++)
            {
                cgltf_node* node = skin.joints[i];
                cgltf_float worldTransform[16];
                cgltf_node_transform_world(node, worldTransform);
                RL_Matrix worldMatrix = {
                    worldTransform[0], worldTransform[4], worldTransform[8], worldTransform[12],
                    worldTransform[1], worldTransform[5], worldTransform[9], worldTransform[13],
                    worldTransform[2], worldTransform[6], worldTransform[10], worldTransform[14],
                    worldTransform[3], worldTransform[7], worldTransform[11], worldTransform[15]
                };
                MatrixDecompose(worldMatrix, &(model.bindPose[i].translation), &(model.bindPose[i].rotation), &(model.bindPose[i].scale));
            }
        }
        else if (data->skins_count > 1)
        {
            TRACELOG(LOG_ERROR, "MODEL: [%s] can only load one skin (armature) per model, but gltf skins_count == %i", fileName, data->skins_count);
        }

        meshIndex = 0;
        for (unsigned int i = 0; i < data->nodes_count; i++)
        {
            cgltf_node *node = &(data->nodes[i]);

            cgltf_mesh *mesh = node->mesh;
            if (!mesh)
                continue;

            for (unsigned int p = 0; p < mesh->primitives_count; p++)
            {
                // NOTE: We only support primitives defined by triangles
                if (mesh->primitives[p].type != cgltf_primitive_type_triangles) continue;

                for (unsigned int j = 0; j < mesh->primitives[p].attributes_count; j++)
                {
                    // NOTE: JOINTS_1 + WEIGHT_1 will be used for +4 joints influencing a vertex -> Not supported by raylib

                    if (mesh->primitives[p].attributes[j].type == cgltf_attribute_type_joints) // JOINTS_n (vec4: 4 bones max per vertex / u8, u16)
                    {
                        cgltf_accessor *attribute = mesh->primitives[p].attributes[j].data;

                        // NOTE: JOINTS_n can only be vec4 and u8/u16
                        // SPECS: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview

                        // WARNING: raylib only supports model.meshes[].boneIds as u8 (unsigned char),
                        // if data is provided in any other format, it is converted to supported format but
                        // it could imply data loss (a warning message is issued in that case)

                        if (attribute->type == cgltf_type_vec4)
                        {
                            if (attribute->component_type == cgltf_component_type_r_8u)
                            {
                                // Init raylib mesh boneIds to copy glTF attribute data
                                model.meshes[meshIndex].boneIds = RL_CALLOC(model.meshes[meshIndex].vertexCount*4, sizeof(unsigned char));

                                // Load attribute: vec4, u8 (unsigned char)
                                LOAD_ATTRIBUTE(attribute, 4, unsigned char, model.meshes[meshIndex].boneIds)
                            }
                            else if (attribute->component_type == cgltf_component_type_r_16u)
                            {
                                // Init raylib mesh boneIds to copy glTF attribute data
                                model.meshes[meshIndex].boneIds = RL_CALLOC(model.meshes[meshIndex].vertexCount*4, sizeof(unsigned char));

                                // Load data into a temp buffer to be converted to raylib data type
                                unsigned short *temp = RL_CALLOC(model.meshes[meshIndex].vertexCount*4, sizeof(unsigned short));
                                LOAD_ATTRIBUTE(attribute, 4, unsigned short, temp);

                                // Convert data to raylib color data type (4 bytes)
                                bool boneIdOverflowWarning = false;
                                for (int b = 0; b < model.meshes[meshIndex].vertexCount*4; b++)
                                {
                                    if ((temp[b] > 255) && !boneIdOverflowWarning)
                                    {
                                        TRACELOG(LOG_WARNING, "MODEL: [%s] Joint attribute data format (u16) overflow", fileName);
                                        boneIdOverflowWarning = true;
                                    }

                                    // Despite the possible overflow, we convert data to unsigned char
                                    model.meshes[meshIndex].boneIds[b] = (unsigned char)temp[b];
                                }

                                RL_FREE(temp);
                            }
                            else TRACELOG(LOG_WARNING, "MODEL: [%s] Joint attribute data format not supported", fileName);
                        }
                        else TRACELOG(LOG_WARNING, "MODEL: [%s] Joint attribute data format not supported", fileName);
                    }
                    else if (mesh->primitives[p].attributes[j].type == cgltf_attribute_type_weights)  // WEIGHTS_n (vec4, u8n/u16n/f32)
                    {
                        cgltf_accessor *attribute = mesh->primitives[p].attributes[j].data;

                        if (attribute->type == cgltf_type_vec4)
                        {
                            // TODO: Support component types: u8, u16?
                            if (attribute->component_type == cgltf_component_type_r_8u)
                            {
                                // Init raylib mesh bone weight to copy glTF attribute data
                                model.meshes[meshIndex].boneWeights = RL_CALLOC(model.meshes[meshIndex].vertexCount*4, sizeof(float));

                                // Load data into a temp buffer to be converted to raylib data type
                                unsigned char *temp = RL_MALLOC(attribute->count*4*sizeof(unsigned char));
                                LOAD_ATTRIBUTE(attribute, 4, unsigned char, temp);

                                // Convert data to raylib bone weight data type (4 bytes)
                                for (unsigned int b = 0; b < attribute->count*4; b++) model.meshes[meshIndex].boneWeights[b] = (float)temp[b]/255.0f;

                                RL_FREE(temp);
                            }
                            else if (attribute->component_type == cgltf_component_type_r_16u)
                            {
                                // Init raylib mesh bone weight to copy glTF attribute data
                                model.meshes[meshIndex].boneWeights = RL_CALLOC(model.meshes[meshIndex].vertexCount*4, sizeof(float));

                                // Load data into a temp buffer to be converted to raylib data type
                                unsigned short *temp = RL_MALLOC(attribute->count*4*sizeof(unsigned short));
                                LOAD_ATTRIBUTE(attribute, 4, unsigned short, temp);

                                // Convert data to raylib bone weight data type
                                for (unsigned int b = 0; b < attribute->count*4; b++) model.meshes[meshIndex].boneWeights[b] = (float)temp[b]/65535.0f;

                                RL_FREE(temp);
                            }
                            else if (attribute->component_type == cgltf_component_type_r_32f)
                            {
                                // Init raylib mesh bone weight to copy glTF attribute data
                                model.meshes[meshIndex].boneWeights = RL_CALLOC(model.meshes[meshIndex].vertexCount*4, sizeof(float));

                                // Load 4 components of float data type into mesh.boneWeights
                                // for cgltf_attribute_type_weights we have:
                                //   - data.meshes[0] (256 vertices)
                                //   - 256 values, provided as cgltf_type_vec4 of float (4 byte per joint, stride 16)
                                LOAD_ATTRIBUTE(attribute, 4, float, model.meshes[meshIndex].boneWeights)
                            }
                            else TRACELOG(LOG_WARNING, "MODEL: [%s] Joint weight attribute data format not supported, use vec4 float", fileName);
                        }
                        else TRACELOG(LOG_WARNING, "MODEL: [%s] Joint weight attribute data format not supported, use vec4 float", fileName);
                    }
                }

                // Animated vertex data
                model.meshes[meshIndex].animVertices = RL_CALLOC(model.meshes[meshIndex].vertexCount*3, sizeof(float));
                memcpy(model.meshes[meshIndex].animVertices, model.meshes[meshIndex].vertices, model.meshes[meshIndex].vertexCount*3*sizeof(float));
                model.meshes[meshIndex].animNormals = RL_CALLOC(model.meshes[meshIndex].vertexCount*3, sizeof(float));
                if (model.meshes[meshIndex].normals != NULL)
                {
                    memcpy(model.meshes[meshIndex].animNormals, model.meshes[meshIndex].normals, model.meshes[meshIndex].vertexCount*3*sizeof(float));
                }
                
                // Bone RL_Transform Matrices
                model.meshes[meshIndex].boneCount = model.boneCount;
                model.meshes[meshIndex].boneMatrices = RL_CALLOC(model.meshes[meshIndex].boneCount, sizeof(RL_Matrix));
                
                for (int j = 0; j < model.meshes[meshIndex].boneCount; j++)
                {
                    model.meshes[meshIndex].boneMatrices[j] = MatrixIdentity();
                }

                meshIndex++;       // Move to next mesh
            }

        }

        // Free all cgltf loaded data
        cgltf_free(data);
    }
    else TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to load glTF data", fileName);

    // WARNING: cgltf requires the file pointer available while reading data
    RL_UnloadFileData(fileData);

    return model;
}

// Get interpolated pose for bone sampler at a specific time. Returns true on success
static bool GetPoseAtTimeGLTF(cgltf_interpolation_type interpolationType, cgltf_accessor *input, cgltf_accessor *output, float time, void *data)
{
    if (interpolationType >= cgltf_interpolation_type_max_enum) return false;

    // Input and output should have the same count
    float tstart = 0.0f;
    float tend = 0.0f;
    int keyframe = 0;       // Defaults to first pose

    for (int i = 0; i < (int)input->count - 1; i++)
    {
        cgltf_bool r1 = cgltf_accessor_read_float(input, i, &tstart, 1);
        if (!r1) return false;

        cgltf_bool r2 = cgltf_accessor_read_float(input, i + 1, &tend, 1);
        if (!r2) return false;

        if ((tstart <= time) && (time < tend))
        {
            keyframe = i;
            break;
        }
    }

    // Constant animation, no need to interpolate
    if (FloatEquals(tend, tstart)) return true;

    float duration = fmaxf((tend - tstart), EPSILON);
    float t = (time - tstart)/duration;
    t = (t < 0.0f)? 0.0f : t;
    t = (t > 1.0f)? 1.0f : t;

    if (output->component_type != cgltf_component_type_r_32f) return false;

    if (output->type == cgltf_type_vec3)
    {
        switch (interpolationType)
        {
            case cgltf_interpolation_type_step:
            {
                float tmp[3] = { 0.0f };
                cgltf_accessor_read_float(output, keyframe, tmp, 3);
                RL_Vector3 v1 = {tmp[0], tmp[1], tmp[2]};
                RL_Vector3 *r = data;

                *r = v1;
            } break;
            case cgltf_interpolation_type_linear:
            {
                float tmp[3] = { 0.0f };
                cgltf_accessor_read_float(output, keyframe, tmp, 3);
                RL_Vector3 v1 = {tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, keyframe+1, tmp, 3);
                RL_Vector3 v2 = {tmp[0], tmp[1], tmp[2]};
                RL_Vector3 *r = data;

                *r = Vector3Lerp(v1, v2, t);
            } break;
            case cgltf_interpolation_type_cubic_spline:
            {
                float tmp[3] = { 0.0f };
                cgltf_accessor_read_float(output, 3*keyframe+1, tmp, 3);
                RL_Vector3 v1 = {tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, 3*keyframe+2, tmp, 3);
                RL_Vector3 tangent1 = {tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, 3*(keyframe+1)+1, tmp, 3);
                RL_Vector3 v2 = {tmp[0], tmp[1], tmp[2]};
                cgltf_accessor_read_float(output, 3*(keyframe+1), tmp, 3);
                RL_Vector3 tangent2 = {tmp[0], tmp[1], tmp[2]};
                RL_Vector3 *r = data;

                *r = Vector3CubicHermite(v1, tangent1, v2, tangent2, t);
            } break;
            default: break;
        }
    }
    else if (output->type == cgltf_type_vec4)
    {
        // Only v4 is for rotations, so we know it's a quaternion
        switch (interpolationType)
        {
            case cgltf_interpolation_type_step:
            {
                float tmp[4] = { 0.0f };
                cgltf_accessor_read_float(output, keyframe, tmp, 4);
                RL_Vector4 v1 = {tmp[0], tmp[1], tmp[2], tmp[3]};
                RL_Vector4 *r = data;

                *r = v1;
            } break;
            case cgltf_interpolation_type_linear:
            {
                float tmp[4] = { 0.0f };
                cgltf_accessor_read_float(output, keyframe, tmp, 4);
                RL_Vector4 v1 = {tmp[0], tmp[1], tmp[2], tmp[3]};
                cgltf_accessor_read_float(output, keyframe+1, tmp, 4);
                RL_Vector4 v2 = {tmp[0], tmp[1], tmp[2], tmp[3]};
                RL_Vector4 *r = data;

                *r = QuaternionSlerp(v1, v2, t);
            } break;
            case cgltf_interpolation_type_cubic_spline:
            {
                float tmp[4] = { 0.0f };
                cgltf_accessor_read_float(output, 3*keyframe+1, tmp, 4);
                RL_Vector4 v1 = {tmp[0], tmp[1], tmp[2], tmp[3]};
                cgltf_accessor_read_float(output, 3*keyframe+2, tmp, 4);
                RL_Vector4 outTangent1 = {tmp[0], tmp[1], tmp[2], 0.0f};
                cgltf_accessor_read_float(output, 3*(keyframe+1)+1, tmp, 4);
                RL_Vector4 v2 = {tmp[0], tmp[1], tmp[2], tmp[3]};
                cgltf_accessor_read_float(output, 3*(keyframe+1), tmp, 4);
                RL_Vector4 inTangent2 = {tmp[0], tmp[1], tmp[2], 0.0f};
                RL_Vector4 *r = data;

                v1 = QuaternionNormalize(v1);
                v2 = QuaternionNormalize(v2);

                if (Vector4DotProduct(v1, v2) < 0.0f)
                {
                    v2 = Vector4Negate(v2);
                }

                outTangent1 = Vector4Scale(outTangent1, duration);
                inTangent2 = Vector4Scale(inTangent2, duration);

                *r = QuaternionCubicHermiteSpline(v1, outTangent1, v2, inTangent2, t);
            } break;
            default: break;
        }
    }

    return true;
}

#define GLTF_ANIMDELAY 17    // Animation frames delay, (~1000 ms/60 FPS = 16.666666* ms)

static RL_ModelAnimation *LoadModelAnimationsGLTF(const char *fileName, int *animCount)
{
    // glTF file loading
    int dataSize = 0;
    unsigned char *fileData = RL_LoadFileData(fileName, &dataSize);

    RL_ModelAnimation *animations = NULL;

    // glTF data loading
    cgltf_options options = { 0 };
    options.file.read = LoadFileGLTFCallback;
    options.file.release = ReleaseFileGLTFCallback;
    cgltf_data *data = NULL;
    cgltf_result result = cgltf_parse(&options, fileData, dataSize, &data);

    if (result != cgltf_result_success)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to load glTF data", fileName);
        *animCount = 0;
        return NULL;
    }

    result = cgltf_load_buffers(&options, data, fileName);
    if (result != cgltf_result_success) TRACELOG(LOG_INFO, "MODEL: [%s] Failed to load animation buffers", fileName);

    if (result == cgltf_result_success)
    {
        if (data->skins_count == 1)
        {
            cgltf_skin skin = data->skins[0];
            *animCount = (int)data->animations_count;
            animations = RL_MALLOC(data->animations_count*sizeof(RL_ModelAnimation));

            for (unsigned int i = 0; i < data->animations_count; i++)
            {
                animations[i].bones = LoadBoneInfoGLTF(skin, &animations[i].boneCount);

                cgltf_animation animData = data->animations[i];

                struct Channels {
                    cgltf_animation_channel *translate;
                    cgltf_animation_channel *rotate;
                    cgltf_animation_channel *scale;
                    cgltf_interpolation_type interpolationType;
                };

                struct Channels *boneChannels = RL_CALLOC(animations[i].boneCount, sizeof(struct Channels));
                float animDuration = 0.0f;

                for (unsigned int j = 0; j < animData.channels_count; j++)
                {
                    cgltf_animation_channel channel = animData.channels[j];
                    int boneIndex = -1;

                    for (unsigned int k = 0; k < skin.joints_count; k++)
                    {
                        if (animData.channels[j].target_node == skin.joints[k])
                        {
                            boneIndex = k;
                            break;
                        }
                    }

                    if (boneIndex == -1)
                    {
                        // Animation channel for a node not in the armature
                        continue;
                    }

                    boneChannels[boneIndex].interpolationType = animData.channels[j].sampler->interpolation;

                    if (animData.channels[j].sampler->interpolation != cgltf_interpolation_type_max_enum)
                    {
                        if (channel.target_path == cgltf_animation_path_type_translation)
                        {
                            boneChannels[boneIndex].translate = &animData.channels[j];
                        }
                        else if (channel.target_path == cgltf_animation_path_type_rotation)
                        {
                            boneChannels[boneIndex].rotate = &animData.channels[j];
                        }
                        else if (channel.target_path == cgltf_animation_path_type_scale)
                        {
                            boneChannels[boneIndex].scale = &animData.channels[j];
                        }
                        else
                        {
                            TRACELOG(LOG_WARNING, "MODEL: [%s] Unsupported target_path on channel %d's sampler for animation %d. Skipping.", fileName, j, i);
                        }
                    }
                    else TRACELOG(LOG_WARNING, "MODEL: [%s] Invalid interpolation curve encountered for GLTF animation.", fileName);

                    float t = 0.0f;
                    cgltf_bool r = cgltf_accessor_read_float(channel.sampler->input, channel.sampler->input->count - 1, &t, 1);

                    if (!r)
                    {
                        TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to load input time", fileName);
                        continue;
                    }

                    animDuration = (t > animDuration)? t : animDuration;
                }

                if (animData.name != NULL)
                {
                    strncpy(animations[i].name, animData.name, sizeof(animations[i].name));
                    animations[i].name[sizeof(animations[i].name) - 1] = '\0';
                }

                animations[i].frameCount = (int)(animDuration*1000.0f/GLTF_ANIMDELAY) + 1;
                animations[i].framePoses = RL_MALLOC(animations[i].frameCount*sizeof(RL_Transform *));

                for (int j = 0; j < animations[i].frameCount; j++)
                {
                    animations[i].framePoses[j] = RL_MALLOC(animations[i].boneCount*sizeof(RL_Transform));
                    float time = ((float) j*GLTF_ANIMDELAY)/1000.0f;

                    for (int k = 0; k < animations[i].boneCount; k++)
                    {
                        RL_Vector3 translation = {skin.joints[k]->translation[0], skin.joints[k]->translation[1], skin.joints[k]->translation[2]};
                        RL_Quaternion rotation = {skin.joints[k]->rotation[0], skin.joints[k]->rotation[1], skin.joints[k]->rotation[2], skin.joints[k]->rotation[3]};
                        RL_Vector3 scale = {skin.joints[k]->scale[0], skin.joints[k]->scale[1], skin.joints[k]->scale[2]};

                        if (boneChannels[k].translate)
                        {
                            if (!GetPoseAtTimeGLTF(boneChannels[k].interpolationType, boneChannels[k].translate->sampler->input, boneChannels[k].translate->sampler->output, time, &translation))
                            {
                                TRACELOG(LOG_INFO, "MODEL: [%s] Failed to load translate pose data for bone %s", fileName, animations[i].bones[k].name);
                            }
                        }

                        if (boneChannels[k].rotate)
                        {
                            if (!GetPoseAtTimeGLTF(boneChannels[k].interpolationType, boneChannels[k].rotate->sampler->input, boneChannels[k].rotate->sampler->output, time, &rotation))
                            {
                                TRACELOG(LOG_INFO, "MODEL: [%s] Failed to load rotate pose data for bone %s", fileName, animations[i].bones[k].name);
                            }
                        }

                        if (boneChannels[k].scale)
                        {
                            if (!GetPoseAtTimeGLTF(boneChannels[k].interpolationType, boneChannels[k].scale->sampler->input, boneChannels[k].scale->sampler->output, time, &scale))
                            {
                                TRACELOG(LOG_INFO, "MODEL: [%s] Failed to load scale pose data for bone %s", fileName, animations[i].bones[k].name);
                            }
                        }

                        animations[i].framePoses[j][k] = (RL_Transform){
                            .translation = translation,
                            .rotation = rotation,
                            .scale = scale
                        };
                    }

                    BuildPoseFromParentJoints(animations[i].bones, animations[i].boneCount, animations[i].framePoses[j]);
                }

                TRACELOG(LOG_INFO, "MODEL: [%s] Loaded animation: %s (%d frames, %fs)", fileName, (animData.name != NULL)? animData.name : "NULL", animations[i].frameCount, animDuration);
                RL_FREE(boneChannels);
            }
        }
        else TRACELOG(LOG_ERROR, "MODEL: [%s] expected exactly one skin to load animation data from, but found %i", fileName, data->skins_count);

        cgltf_free(data);
    }
    RL_UnloadFileData(fileData);
    return animations;
}
#endif

#if defined(SUPPORT_FILEFORMAT_VOX)
// Load VOX (MagicaVoxel) mesh data
static RL_Model LoadVOX(const char *fileName)
{
    RL_Model model = { 0 };

    int nbvertices = 0;
    int meshescount = 0;

    // Read vox file into buffer
    int dataSize = 0;
    unsigned char *fileData = RL_LoadFileData(fileName, &dataSize);

    if (fileData == 0)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to load VOX file", fileName);
        return model;
    }

    // Read and build voxarray description
    VoxArray3D voxarray = { 0 };
    int ret = Vox_LoadFromMemory(fileData, dataSize, &voxarray);

    if (ret != VOX_SUCCESS)
    {
        // Error
        RL_UnloadFileData(fileData);

        TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to load VOX data", fileName);
        return model;
    }
    else
    {
        // Success: Compute meshes count
        nbvertices = voxarray.vertices.used;
        meshescount = 1 + (nbvertices/65536);

        TRACELOG(LOG_INFO, "MODEL: [%s] VOX data loaded successfully : %i vertices/%i meshes", fileName, nbvertices, meshescount);
    }

    // Build models from meshes
    model.transform = MatrixIdentity();

    model.meshCount = meshescount;
    model.meshes = (RL_Mesh *)RL_CALLOC(model.meshCount, sizeof(RL_Mesh));

    model.meshMaterial = (int *)RL_CALLOC(model.meshCount, sizeof(int));

    model.materialCount = 1;
    model.materials = (RL_Material *)RL_CALLOC(model.materialCount, sizeof(RL_Material));
    model.materials[0] = RL_LoadMaterialDefault();

    // Init model meshes
    int verticesRemain = voxarray.vertices.used;
    int verticesMax = 65532; // 5461 voxels x 12 vertices per voxel -> 65532 (must be inf 65536)

    // 6*4 = 12 vertices per voxel
    RL_Vector3 *pvertices = (RL_Vector3 *)voxarray.vertices.array;
    RL_Vector3 *pnormals = (RL_Vector3 *)voxarray.normals.array;
    RL_Color *pcolors = (RL_Color *)voxarray.colors.array;

    unsigned short *pindices = voxarray.indices.array;    // 5461*6*6 = 196596 indices max per mesh

    int size = 0;

    for (int i = 0; i < meshescount; i++)
    {
        RL_Mesh *pmesh = &model.meshes[i];
        memset(pmesh, 0, sizeof(RL_Mesh));

        // Copy vertices
        pmesh->vertexCount = (int)fmin(verticesMax, verticesRemain);

        size = pmesh->vertexCount*sizeof(float)*3;
        pmesh->vertices = (float *)RL_MALLOC(size);
        memcpy(pmesh->vertices, pvertices, size);

        // Copy normals
        pmesh->normals = (float *)RL_MALLOC(size);
        memcpy(pmesh->normals, pnormals, size);

        // Copy indices
        size = voxarray.indices.used*sizeof(unsigned short);
        pmesh->indices = (unsigned short *)RL_MALLOC(size);
        memcpy(pmesh->indices, pindices, size);

        pmesh->triangleCount = (pmesh->vertexCount/4)*2;

        // Copy colors
        size = pmesh->vertexCount*sizeof(RL_Color);
        pmesh->colors = RL_MALLOC(size);
        memcpy(pmesh->colors, pcolors, size);

        // First material index
        model.meshMaterial[i] = 0;

        verticesRemain -= verticesMax;
        pvertices += verticesMax;
        pnormals += verticesMax;
        pcolors += verticesMax;
    }

    // Free buffers
    Vox_FreeArrays(&voxarray);
    RL_UnloadFileData(fileData);

    return model;
}
#endif

#if defined(SUPPORT_FILEFORMAT_M3D)
// Hook RL_LoadFileData()/RL_UnloadFileData() calls to M3D loaders
unsigned char *m3d_loaderhook(char *fn, unsigned int *len) { return RL_LoadFileData((const char *)fn, (int *)len); }
void m3d_freehook(void *data) { RL_UnloadFileData((unsigned char *)data); }

// Load M3D mesh data
static RL_Model LoadM3D(const char *fileName)
{
    RL_Model model = { 0 };

    m3d_t *m3d = NULL;
    m3dp_t *prop = NULL;
    int i, j, k, l, n, mi = -2, vcolor = 0;

    int dataSize = 0;
    unsigned char *fileData = RL_LoadFileData(fileName, &dataSize);

    if (fileData != NULL)
    {
        m3d = m3d_load(fileData, m3d_loaderhook, m3d_freehook, NULL);

        if (!m3d || M3D_ERR_ISFATAL(m3d->errcode))
        {
            TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to load M3D data, error code %d", fileName, m3d? m3d->errcode : -2);
            if (m3d) m3d_free(m3d);
            RL_UnloadFileData(fileData);
            return model;
        }
        else TRACELOG(LOG_INFO, "MODEL: [%s] M3D data loaded successfully: %i faces/%i materials", fileName, m3d->numface, m3d->nummaterial);

        // no face? this is probably just a material library
        if (!m3d->numface)
        {
            m3d_free(m3d);
            RL_UnloadFileData(fileData);
            return model;
        }

        if (m3d->nummaterial > 0)
        {
            model.meshCount = model.materialCount = m3d->nummaterial;
            TRACELOG(LOG_INFO, "MODEL: model has %i material meshes", model.materialCount);
        }
        else
        {
            model.meshCount = 1; model.materialCount = 0;
            TRACELOG(LOG_INFO, "MODEL: No materials, putting all meshes in a default material");
        }

        // We always need a default material, so we add +1
        model.materialCount++;

        // Faces must be in non-decreasing materialid order. Verify that quickly, sorting them otherwise
        // WARNING: Sorting is not needed, valid M3D model files should already be sorted
        // Just keeping the sorting function for reference (Check PR #3363 #3385)
        /*
        for (i = 1; i < m3d->numface; i++)
        {
            if (m3d->face[i-1].materialid <= m3d->face[i].materialid) continue;

            // face[i-1] > face[i].  slide face[i] lower
            m3df_t slider = m3d->face[i];
            j = i-1;

            do
            {   // face[j] > slider, face[j+1] is svailable vacant gap
                m3d->face[j+1] = m3d->face[j];
                j = j-1;
            }
            while (j >= 0 && m3d->face[j].materialid > slider.materialid);

            m3d->face[j+1] = slider;
        }
        */

        model.meshes = (RL_Mesh *)RL_CALLOC(model.meshCount, sizeof(RL_Mesh));
        model.meshMaterial = (int *)RL_CALLOC(model.meshCount, sizeof(int));
        model.materials = (RL_Material *)RL_CALLOC(model.materialCount + 1, sizeof(RL_Material));

        // Map no material to index 0 with default shader, everything else materialid + 1
        model.materials[0] = RL_LoadMaterialDefault();

        for (i = l = 0, k = -1; i < (int)m3d->numface; i++, l++)
        {
            // Materials are grouped together
            if (mi != m3d->face[i].materialid)
            {
                // there should be only one material switch per material kind, but be bulletproof for non-optimal model files
                if (k + 1 >= model.meshCount)
                {
                    model.meshCount++;
                    model.meshes = (RL_Mesh *)RL_REALLOC(model.meshes, model.meshCount*sizeof(RL_Mesh));
                    memset(&model.meshes[model.meshCount - 1], 0, sizeof(RL_Mesh));
                    model.meshMaterial = (int *)RL_REALLOC(model.meshMaterial, model.meshCount*sizeof(int));
                }

                k++;
                mi = m3d->face[i].materialid;

                // Only allocate colors VertexBuffer if there's a color vertex in the model for this material batch
                // if all colors are fully transparent black for all verteces of this materal, then we assume no vertex colors
                for (j = i, l = vcolor = 0; (j < (int)m3d->numface) && (mi == m3d->face[j].materialid); j++, l++)
                {
                    if (!m3d->vertex[m3d->face[j].vertex[0]].color ||
                        !m3d->vertex[m3d->face[j].vertex[1]].color ||
                        !m3d->vertex[m3d->face[j].vertex[2]].color) vcolor = 1;
                }

                model.meshes[k].vertexCount = l*3;
                model.meshes[k].triangleCount = l;
                model.meshes[k].vertices = (float *)RL_CALLOC(model.meshes[k].vertexCount*3, sizeof(float));
                model.meshes[k].texcoords = (float *)RL_CALLOC(model.meshes[k].vertexCount*2, sizeof(float));
                model.meshes[k].normals = (float *)RL_CALLOC(model.meshes[k].vertexCount*3, sizeof(float));

                // If no map is provided, or we have colors defined, we allocate storage for vertex colors
                // M3D specs only consider vertex colors if no material is provided, however raylib uses both and mixes the colors
                if ((mi == M3D_UNDEF) || vcolor) model.meshes[k].colors = RL_CALLOC(model.meshes[k].vertexCount*4, sizeof(unsigned char));

                // If no map is provided and we allocated vertex colors, set them to white
                if ((mi == M3D_UNDEF) && (model.meshes[k].colors != NULL))
                {
                    for (int c = 0; c < model.meshes[k].vertexCount*4; c++) model.meshes[k].colors[c] = 255;
                }

                if (m3d->numbone && m3d->numskin)
                {
                    model.meshes[k].boneIds = (unsigned char *)RL_CALLOC(model.meshes[k].vertexCount*4, sizeof(unsigned char));
                    model.meshes[k].boneWeights = (float *)RL_CALLOC(model.meshes[k].vertexCount*4, sizeof(float));
                    model.meshes[k].animVertices = (float *)RL_CALLOC(model.meshes[k].vertexCount*3, sizeof(float));
                    model.meshes[k].animNormals = (float *)RL_CALLOC(model.meshes[k].vertexCount*3, sizeof(float));
                }

                model.meshMaterial[k] = mi + 1;
                l = 0;
            }

            // Process meshes per material, add triangles
            model.meshes[k].vertices[l*9 + 0] = m3d->vertex[m3d->face[i].vertex[0]].x*m3d->scale;
            model.meshes[k].vertices[l*9 + 1] = m3d->vertex[m3d->face[i].vertex[0]].y*m3d->scale;
            model.meshes[k].vertices[l*9 + 2] = m3d->vertex[m3d->face[i].vertex[0]].z*m3d->scale;
            model.meshes[k].vertices[l*9 + 3] = m3d->vertex[m3d->face[i].vertex[1]].x*m3d->scale;
            model.meshes[k].vertices[l*9 + 4] = m3d->vertex[m3d->face[i].vertex[1]].y*m3d->scale;
            model.meshes[k].vertices[l*9 + 5] = m3d->vertex[m3d->face[i].vertex[1]].z*m3d->scale;
            model.meshes[k].vertices[l*9 + 6] = m3d->vertex[m3d->face[i].vertex[2]].x*m3d->scale;
            model.meshes[k].vertices[l*9 + 7] = m3d->vertex[m3d->face[i].vertex[2]].y*m3d->scale;
            model.meshes[k].vertices[l*9 + 8] = m3d->vertex[m3d->face[i].vertex[2]].z*m3d->scale;

            // Without vertex color (full transparency), we use the default color
            if (model.meshes[k].colors != NULL)
            {
                if (m3d->vertex[m3d->face[i].vertex[0]].color & 0xFF000000)
                    memcpy(&model.meshes[k].colors[l*12 + 0], &m3d->vertex[m3d->face[i].vertex[0]].color, 4);
                if (m3d->vertex[m3d->face[i].vertex[1]].color & 0xFF000000)
                    memcpy(&model.meshes[k].colors[l*12 + 4], &m3d->vertex[m3d->face[i].vertex[1]].color, 4);
                if (m3d->vertex[m3d->face[i].vertex[2]].color & 0xFF000000)
                    memcpy(&model.meshes[k].colors[l*12 + 8], &m3d->vertex[m3d->face[i].vertex[2]].color, 4);
            }

            if (m3d->face[i].texcoord[0] != M3D_UNDEF)
            {
                model.meshes[k].texcoords[l*6 + 0] = m3d->tmap[m3d->face[i].texcoord[0]].u;
                model.meshes[k].texcoords[l*6 + 1] = 1.0f - m3d->tmap[m3d->face[i].texcoord[0]].v;
                model.meshes[k].texcoords[l*6 + 2] = m3d->tmap[m3d->face[i].texcoord[1]].u;
                model.meshes[k].texcoords[l*6 + 3] = 1.0f - m3d->tmap[m3d->face[i].texcoord[1]].v;
                model.meshes[k].texcoords[l*6 + 4] = m3d->tmap[m3d->face[i].texcoord[2]].u;
                model.meshes[k].texcoords[l*6 + 5] = 1.0f - m3d->tmap[m3d->face[i].texcoord[2]].v;
            }

            if (m3d->face[i].normal[0] != M3D_UNDEF)
            {
                model.meshes[k].normals[l*9 + 0] = m3d->vertex[m3d->face[i].normal[0]].x;
                model.meshes[k].normals[l*9 + 1] = m3d->vertex[m3d->face[i].normal[0]].y;
                model.meshes[k].normals[l*9 + 2] = m3d->vertex[m3d->face[i].normal[0]].z;
                model.meshes[k].normals[l*9 + 3] = m3d->vertex[m3d->face[i].normal[1]].x;
                model.meshes[k].normals[l*9 + 4] = m3d->vertex[m3d->face[i].normal[1]].y;
                model.meshes[k].normals[l*9 + 5] = m3d->vertex[m3d->face[i].normal[1]].z;
                model.meshes[k].normals[l*9 + 6] = m3d->vertex[m3d->face[i].normal[2]].x;
                model.meshes[k].normals[l*9 + 7] = m3d->vertex[m3d->face[i].normal[2]].y;
                model.meshes[k].normals[l*9 + 8] = m3d->vertex[m3d->face[i].normal[2]].z;
            }

            // Add skin (vertex / bone weight pairs)
            if (m3d->numbone && m3d->numskin)
            {
                for (n = 0; n < 3; n++)
                {
                    int skinid = m3d->vertex[m3d->face[i].vertex[n]].skinid;

                    // Check if there is a skin for this mesh, should be, just failsafe
                    if ((skinid != M3D_UNDEF) && (skinid < (int)m3d->numskin))
                    {
                        for (j = 0; j < 4; j++)
                        {
                            model.meshes[k].boneIds[l*12 + n*4 + j] = m3d->skin[skinid].boneid[j];
                            model.meshes[k].boneWeights[l*12 + n*4 + j] = m3d->skin[skinid].weight[j];
                        }
                    }
                    else
                    {
                        // raylib does not handle boneless meshes with skeletal animations, so
                        // we put all vertices without a bone into a special "no bone" bone
                        model.meshes[k].boneIds[l*12 + n*4] = m3d->numbone;
                        model.meshes[k].boneWeights[l*12 + n*4] = 1.0f;
                    }
                }
            }
        }

        // Load materials
        for (i = 0; i < (int)m3d->nummaterial; i++)
        {
            model.materials[i + 1] = RL_LoadMaterialDefault();

            for (j = 0; j < m3d->material[i].numprop; j++)
            {
                prop = &m3d->material[i].prop[j];

                switch (prop->type)
                {
                    case m3dp_Kd:
                    {
                        memcpy(&model.materials[i + 1].maps[MATERIAL_MAP_DIFFUSE].color, &prop->value.color, 4);
                        model.materials[i + 1].maps[MATERIAL_MAP_DIFFUSE].value = 0.0f;
                    } break;
                    case m3dp_Ks:
                    {
                        memcpy(&model.materials[i + 1].maps[MATERIAL_MAP_SPECULAR].color, &prop->value.color, 4);
                    } break;
                    case m3dp_Ns:
                    {
                        model.materials[i + 1].maps[MATERIAL_MAP_SPECULAR].value = prop->value.fnum;
                    } break;
                    case m3dp_Ke:
                    {
                        memcpy(&model.materials[i + 1].maps[MATERIAL_MAP_EMISSION].color, &prop->value.color, 4);
                        model.materials[i + 1].maps[MATERIAL_MAP_EMISSION].value = 0.0f;
                    } break;
                    case m3dp_Pm:
                    {
                        model.materials[i + 1].maps[MATERIAL_MAP_METALNESS].value = prop->value.fnum;
                    } break;
                    case m3dp_Pr:
                    {
                        model.materials[i + 1].maps[MATERIAL_MAP_ROUGHNESS].value = prop->value.fnum;
                    } break;
                    case m3dp_Ps:
                    {
                        model.materials[i + 1].maps[MATERIAL_MAP_NORMAL].color = RL_WHITE;
                        model.materials[i + 1].maps[MATERIAL_MAP_NORMAL].value = prop->value.fnum;
                    } break;
                    default:
                    {
                        if (prop->type >= 128)
                        {
                            RL_Image image = { 0 };
                            image.data = m3d->texture[prop->value.textureid].d;
                            image.width = m3d->texture[prop->value.textureid].w;
                            image.height = m3d->texture[prop->value.textureid].h;
                            image.mipmaps = 1;
                            image.format = (m3d->texture[prop->value.textureid].f == 4)? PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 :
                                           ((m3d->texture[prop->value.textureid].f == 3)? PIXELFORMAT_UNCOMPRESSED_R8G8B8 :
                                           ((m3d->texture[prop->value.textureid].f == 2)? PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA : PIXELFORMAT_UNCOMPRESSED_GRAYSCALE));

                            switch (prop->type)
                            {
                                case m3dp_map_Kd: model.materials[i + 1].maps[MATERIAL_MAP_DIFFUSE].texture = RL_LoadTextureFromImage(image); break;
                                case m3dp_map_Ks: model.materials[i + 1].maps[MATERIAL_MAP_SPECULAR].texture = RL_LoadTextureFromImage(image); break;
                                case m3dp_map_Ke: model.materials[i + 1].maps[MATERIAL_MAP_EMISSION].texture = RL_LoadTextureFromImage(image); break;
                                case m3dp_map_Km: model.materials[i + 1].maps[MATERIAL_MAP_NORMAL].texture = RL_LoadTextureFromImage(image); break;
                                case m3dp_map_Ka: model.materials[i + 1].maps[MATERIAL_MAP_OCCLUSION].texture = RL_LoadTextureFromImage(image); break;
                                case m3dp_map_Pm: model.materials[i + 1].maps[MATERIAL_MAP_ROUGHNESS].texture = RL_LoadTextureFromImage(image); break;
                                default: break;
                            }
                        }
                    } break;
                }
            }
        }

        // Load bones
        if (m3d->numbone)
        {
            model.boneCount = m3d->numbone + 1;
            model.bones = RL_CALLOC(model.boneCount, sizeof(RL_BoneInfo));
            model.bindPose = RL_CALLOC(model.boneCount, sizeof(RL_Transform));

            for (i = 0; i < (int)m3d->numbone; i++)
            {
                model.bones[i].parent = m3d->bone[i].parent;
                strncpy(model.bones[i].name, m3d->bone[i].name, sizeof(model.bones[i].name));
                model.bindPose[i].translation.x = m3d->vertex[m3d->bone[i].pos].x*m3d->scale;
                model.bindPose[i].translation.y = m3d->vertex[m3d->bone[i].pos].y*m3d->scale;
                model.bindPose[i].translation.z = m3d->vertex[m3d->bone[i].pos].z*m3d->scale;
                model.bindPose[i].rotation.x = m3d->vertex[m3d->bone[i].ori].x;
                model.bindPose[i].rotation.y = m3d->vertex[m3d->bone[i].ori].y;
                model.bindPose[i].rotation.z = m3d->vertex[m3d->bone[i].ori].z;
                model.bindPose[i].rotation.w = m3d->vertex[m3d->bone[i].ori].w;

                // TODO: If the orientation quaternion is not normalized, then that's encoding scaling
                model.bindPose[i].rotation = QuaternionNormalize(model.bindPose[i].rotation);
                model.bindPose[i].scale.x = model.bindPose[i].scale.y = model.bindPose[i].scale.z = 1.0f;

                // Child bones are stored in parent bone relative space, convert that into model space
                if (model.bones[i].parent >= 0)
                {
                    model.bindPose[i].rotation = QuaternionMultiply(model.bindPose[model.bones[i].parent].rotation, model.bindPose[i].rotation);
                    model.bindPose[i].translation = Vector3RotateByQuaternion(model.bindPose[i].translation, model.bindPose[model.bones[i].parent].rotation);
                    model.bindPose[i].translation = Vector3Add(model.bindPose[i].translation, model.bindPose[model.bones[i].parent].translation);
                    model.bindPose[i].scale = Vector3Multiply(model.bindPose[i].scale, model.bindPose[model.bones[i].parent].scale);
                }
            }

            // Add a special "no bone" bone
            model.bones[i].parent = -1;
            strcpy(model.bones[i].name, "NO BONE");
            model.bindPose[i].translation.x = 0.0f;
            model.bindPose[i].translation.y = 0.0f;
            model.bindPose[i].translation.z = 0.0f;
            model.bindPose[i].rotation.x = 0.0f;
            model.bindPose[i].rotation.y = 0.0f;
            model.bindPose[i].rotation.z = 0.0f;
            model.bindPose[i].rotation.w = 1.0f;
            model.bindPose[i].scale.x = model.bindPose[i].scale.y = model.bindPose[i].scale.z = 1.0f;
        }

        // Load bone-pose default mesh into animation vertices. These will be updated when RL_UpdateModelAnimation gets
        // called, but not before, however RL_DrawMesh uses these if they exist (so not good if they are left empty)
        if (m3d->numbone && m3d->numskin)
        {
            for (i = 0; i < model.meshCount; i++)
            {
                memcpy(model.meshes[i].animVertices, model.meshes[i].vertices, model.meshes[i].vertexCount*3*sizeof(float));
                memcpy(model.meshes[i].animNormals, model.meshes[i].normals, model.meshes[i].vertexCount*3*sizeof(float));
                
                model.meshes[i].boneCount = model.boneCount;
                model.meshes[i].boneMatrices = RL_CALLOC(model.meshes[i].boneCount, sizeof(RL_Matrix));
                for (j = 0; j < model.meshes[i].boneCount; j++)
                {
                    model.meshes[i].boneMatrices[j] = MatrixIdentity();
                }
            }
        }

        m3d_free(m3d);
        RL_UnloadFileData(fileData);
    }

    return model;
}

#define M3D_ANIMDELAY 17    // Animation frames delay, (~1000 ms/60 FPS = 16.666666* ms)

// Load M3D animation data
static RL_ModelAnimation *LoadModelAnimationsM3D(const char *fileName, int *animCount)
{
    RL_ModelAnimation *animations = NULL;

    m3d_t *m3d = NULL;
    int i = 0, j = 0;
    *animCount = 0;

    int dataSize = 0;
    unsigned char *fileData = RL_LoadFileData(fileName, &dataSize);

    if (fileData != NULL)
    {
        m3d = m3d_load(fileData, m3d_loaderhook, m3d_freehook, NULL);

        if (!m3d || M3D_ERR_ISFATAL(m3d->errcode))
        {
            TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to load M3D data, error code %d", fileName, m3d? m3d->errcode : -2);
            RL_UnloadFileData(fileData);
            return NULL;
        }
        else TRACELOG(LOG_INFO, "MODEL: [%s] M3D data loaded successfully: %i animations, %i bones, %i skins", fileName,
            m3d->numaction, m3d->numbone, m3d->numskin);

        // No animation or bone+skin?
        if (!m3d->numaction || !m3d->numbone || !m3d->numskin)
        {
            m3d_free(m3d);
            RL_UnloadFileData(fileData);
            return NULL;
        }

        animations = RL_MALLOC(m3d->numaction*sizeof(RL_ModelAnimation));
        *animCount = m3d->numaction;

        for (unsigned int a = 0; a < m3d->numaction; a++)
        {
            animations[a].frameCount = m3d->action[a].durationmsec/M3D_ANIMDELAY;
            animations[a].boneCount = m3d->numbone + 1;
            animations[a].bones = RL_MALLOC((m3d->numbone + 1)*sizeof(RL_BoneInfo));
            animations[a].framePoses = RL_MALLOC(animations[a].frameCount*sizeof(RL_Transform *));
            strncpy(animations[a].name, m3d->action[a].name, sizeof(animations[a].name));
            animations[a].name[sizeof(animations[a].name) - 1] = '\0';

            TRACELOG(LOG_INFO, "MODEL: [%s] animation #%i: %i msec, %i frames", fileName, a, m3d->action[a].durationmsec, animations[a].frameCount);

            for (i = 0; i < (int)m3d->numbone; i++)
            {
                animations[a].bones[i].parent = m3d->bone[i].parent;
                strncpy(animations[a].bones[i].name, m3d->bone[i].name, sizeof(animations[a].bones[i].name));
            }

            // A special, never transformed "no bone" bone, used for boneless vertices
            animations[a].bones[i].parent = -1;
            strcpy(animations[a].bones[i].name, "NO BONE");

            // M3D stores frames at arbitrary intervals with sparse skeletons. We need full skeletons at
            // regular intervals, so let the M3D SDK do the heavy lifting and calculate interpolated bones
            for (i = 0; i < animations[a].frameCount; i++)
            {
                animations[a].framePoses[i] = RL_MALLOC((m3d->numbone + 1)*sizeof(RL_Transform));

                m3db_t *pose = m3d_pose(m3d, a, i*M3D_ANIMDELAY);

                if (pose != NULL)
                {
                    for (j = 0; j < (int)m3d->numbone; j++)
                    {
                        animations[a].framePoses[i][j].translation.x = m3d->vertex[pose[j].pos].x*m3d->scale;
                        animations[a].framePoses[i][j].translation.y = m3d->vertex[pose[j].pos].y*m3d->scale;
                        animations[a].framePoses[i][j].translation.z = m3d->vertex[pose[j].pos].z*m3d->scale;
                        animations[a].framePoses[i][j].rotation.x = m3d->vertex[pose[j].ori].x;
                        animations[a].framePoses[i][j].rotation.y = m3d->vertex[pose[j].ori].y;
                        animations[a].framePoses[i][j].rotation.z = m3d->vertex[pose[j].ori].z;
                        animations[a].framePoses[i][j].rotation.w = m3d->vertex[pose[j].ori].w;
                        animations[a].framePoses[i][j].rotation = QuaternionNormalize(animations[a].framePoses[i][j].rotation);
                        animations[a].framePoses[i][j].scale.x = animations[a].framePoses[i][j].scale.y = animations[a].framePoses[i][j].scale.z = 1.0f;

                        // Child bones are stored in parent bone relative space, convert that into model space
                        if (animations[a].bones[j].parent >= 0)
                        {
                            animations[a].framePoses[i][j].rotation = QuaternionMultiply(animations[a].framePoses[i][animations[a].bones[j].parent].rotation, animations[a].framePoses[i][j].rotation);
                            animations[a].framePoses[i][j].translation = Vector3RotateByQuaternion(animations[a].framePoses[i][j].translation, animations[a].framePoses[i][animations[a].bones[j].parent].rotation);
                            animations[a].framePoses[i][j].translation = Vector3Add(animations[a].framePoses[i][j].translation, animations[a].framePoses[i][animations[a].bones[j].parent].translation);
                            animations[a].framePoses[i][j].scale = Vector3Multiply(animations[a].framePoses[i][j].scale, animations[a].framePoses[i][animations[a].bones[j].parent].scale);
                        }
                    }

                    // Default transform for the "no bone" bone
                    animations[a].framePoses[i][j].translation.x = 0.0f;
                    animations[a].framePoses[i][j].translation.y = 0.0f;
                    animations[a].framePoses[i][j].translation.z = 0.0f;
                    animations[a].framePoses[i][j].rotation.x = 0.0f;
                    animations[a].framePoses[i][j].rotation.y = 0.0f;
                    animations[a].framePoses[i][j].rotation.z = 0.0f;
                    animations[a].framePoses[i][j].rotation.w = 1.0f;
                    animations[a].framePoses[i][j].scale.x = animations[a].framePoses[i][j].scale.y = animations[a].framePoses[i][j].scale.z = 1.0f;
                    RL_FREE(pose);
                }
            }
        }

        m3d_free(m3d);
        RL_UnloadFileData(fileData);
    }

    return animations;
}
#endif

#endif      // SUPPORT_MODULE_RMODELS
