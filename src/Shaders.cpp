////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Stars.hpp"

namespace csp::stars {

////////////////////////////////////////////////////////////////////////////////////////////////////

const std::string Stars::cStarsVert = R"(
#version 330

// inputs
// ========================================================================
layout(location = 0) in vec2  inDir;
layout(location = 1) in float inDist;
layout(location = 2) in vec3  inColor;
layout(location = 3) in float inMagnitude;
                                                                            
// uniforms
// ========================================================================
uniform mat4 uMatMV;
uniform mat4 uInvMV;

uniform float fLoadedMinMagnitude;
uniform float fLoadedMaxMagnitude;

uniform float fMinSize;
uniform float fMaxSize;

uniform float fMinOpacity;
uniform float fMaxOpacity;

uniform float fScalingExponent;
uniform bool  bDraw3D;

// outputs
// ========================================================================
out vec4  vColor;
out float fScale;
out float fMagnitude;

// ========================================================================
void main()
{
    if (bDraw3D)
    {
        vec3 starPos = vec3(
            cos(inDir.x) * cos(inDir.y) * inDist,
            sin(inDir.x) * inDist,
            cos(inDir.x) * sin(inDir.y) * inDist);

        float absMagnitude = inMagnitude - 5.0*log(inDist) + 5.0;
    
        const float parsecToMeter = 3.08567758e16;
        vec3 observerPos = uInvMV[3].xyz / parsecToMeter;

        fMagnitude = absMagnitude + 5.0*log(length(starPos-observerPos))-5.0;
        gl_Position = uMatMV * vec4(starPos*parsecToMeter, 1);
    }
    else
    {
        vec3 starPos = vec3(
            cos(inDir.x) * cos(inDir.y),
            sin(inDir.x),
            cos(inDir.x) * sin(inDir.y));
        
        fMagnitude = inMagnitude;
        gl_Position = uMatMV * vec4(starPos, 0);
    }

    float scaleFac = 1.0 - (fMagnitude-fLoadedMinMagnitude) /
                           (fLoadedMaxMagnitude-fLoadedMinMagnitude);
    scaleFac = pow(scaleFac, fScalingExponent);

    vColor = vec4(inColor, mix(fMinOpacity, fMaxOpacity, scaleFac));
    fScale = mix(fMinSize, fMaxSize, scaleFac) * 0.01;
}
)";

////////////////////////////////////////////////////////////////////////////////////////////////////

const std::string Stars::cStarsFrag = R"(
#version 330

// inputs
// ========================================================================
in vec4 vFragColor;
in vec2 vTexcoords;

// uniforms
// ========================================================================
uniform sampler2D iStarTexture;

// outputs
// ========================================================================
layout(location = 0) out vec3 vOutColor;

// ========================================================================
void main()
{
    float intensity = texture(iStarTexture, vTexcoords).r;
    vOutColor = vFragColor.a * mix(vFragColor.rgb, vec3(1), pow(intensity, 4)) * intensity;
}
)";

////////////////////////////////////////////////////////////////////////////////////////////////////

const std::string Stars::cStarsGeom = R"(
#version 330

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

// inputs
// ========================================================================
in vec4  vColor[];in float fScale[];                                                          
in float fMagnitude[];

// uniforms
// ========================================================================
uniform float fMinMagnitude;
uniform float fMaxMagnitude;
uniform mat4 uMatP;

// outputs
// ========================================================================
out vec4 vFragColor;
out vec2 vTexcoords;

// ========================================================================
void main()
{
    if (fMagnitude[0] >= fMinMagnitude &&
        fMagnitude[0] <= fMaxMagnitude )
    {
        vFragColor = vColor[0];
    
        float scale = length(gl_in[0].gl_Position.xyz);
        vec3 y = vec3(0, 1, 0);
        vec3 z = gl_in[0].gl_Position.xyz / scale;
        vec3 x = normalize(cross(z, y));
        y = normalize(cross(z, x));

        const float yo[2] = float[2](0.5, -0.5);
        const float xo[2] = float[2](0.5, -0.5);

        for(int j=0; j!=2; ++j)
        {
            for(int i=0; i!=2; ++i)
            {
                vTexcoords = vec2(xo[i], yo[j]) + 0.5;
                vec3 pos = gl_in[0].gl_Position.xyz + (xo[i] * x + yo[j] * y) * fScale[0] * scale;

                gl_Position = uMatP * vec4(pos, 1);


                if (gl_Position.w > 0) {
                    gl_Position /= gl_Position.w;
                    if (gl_Position.z >= 1) {
                        gl_Position.z = 0.999999;
                    }
                    EmitVertex();
                }
            }
        }
        EndPrimitive();
    }
}
)";

////////////////////////////////////////////////////////////////////////////////////////////////////

const std::string Stars::cBackgroundVert = R"(
#version 330

// inputs
// ========================================================================
layout(location = 0) in vec2 vPosition;

// uniforms
// ========================================================================
uniform mat4 uInvMVP;
uniform mat4 uInvMV;

// outputs
// ========================================================================
out vec3 vView;

// ========================================================================
void main()
{
    vec3 vRayOrigin = (uInvMV * vec4(0, 0, 0, 1)).xyz;
    vec4 vRayEnd    = uInvMVP * vec4(vPosition, 0, 1);
    vView  = vRayEnd.xyz / vRayEnd.w - vRayOrigin;
    gl_Position = vec4(vPosition, 1, 1);
}
)";

////////////////////////////////////////////////////////////////////////////////////////////////////

const std::string Stars::cBackgroundFrag = R"(
#version 330

// inputs
// ========================================================================
in vec3 vView;

// uniforms
// ========================================================================
uniform sampler2D iTexture;
uniform vec4      cColor;

// outputs
// ========================================================================
layout(location = 0) out vec3 vOutColor;

// ========================================================================
float my_atan2(float a, float b) {
    return 2.0 * atan(a/(sqrt(b*b + a*a) + b));
}

void main()
{
    const float PI = 3.14159265359;
    vec3 view = normalize(vView);
    vec2 texcoord = vec2(0.5*my_atan2(view.x, -view.z)/PI,
                         acos(view.y)/PI);
    vOutColor  = texture(iTexture, texcoord).rgb * cColor.rgb * cColor.a;
}
)";

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::stars
