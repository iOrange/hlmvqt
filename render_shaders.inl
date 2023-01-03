//shaders to draw a model
static const char* g_VS_DrawModel = R"==(
attribute vec3 inPos;
attribute vec2 inUV;
attribute vec3 inNormal;

uniform vec3 lightPos;
uniform mat4 mv;
uniform mat4 mvp;
uniform bool isChrome;

varying vec3 normalVec;
varying vec3 lightVec;
varying vec3 texCoords; // z - lighting factor

void main() {
    vec3 worldPos = (mv * vec4(inPos, 1.0)).xyz;
    lightVec = normalize(lightPos - worldPos);
    normalVec = normalize((mv * vec4(inNormal, 0.0)).xyz);
    if (isChrome) {
        vec3 toVert = normalize(-inPos);
        vec3 vright = normalize((mv * vec4(1.0, 1.0, 0.0, 0.0)).xyz);
        vec3 chromeUp = normalize(cross(toVert, vright));
        vec3 chromeRight = normalize(cross(toVert, chromeUp));

        texCoords.x = (dot(normalVec, chromeRight) + 1.0) * 32.0 / 64.0;
        texCoords.y = (dot(normalVec, chromeUp) + 1.0) * 32.0 / 64.0;
        texCoords.z = 0.0;
    } else {
        texCoords = vec3(inUV, 1.0);
    }
    gl_Position = mvp * vec4(inPos, 1.0);
}
)==";

static const char* g_FS_DrawModel = R"==(
varying vec3 normalVec;
varying vec3 lightVec;
varying vec3 texCoords; // z - lighting factor

uniform sampler2D texDiffuse;
uniform vec4 forcedColor;
uniform vec4 alphaTest;

#define saturate(val) clamp((val), 0.0, 1.0)
#define lerp(valA, valB, factorT) mix((valA), (valB), (factorT))

#define minDiffuse 0.35

void main() {
    vec4 diffuseColor = texture2D(texDiffuse, texCoords.xy);
    if (diffuseColor.a < alphaTest.x) {
        discard;
    }

    vec3 normal = normalize(normalVec);
    vec3 dirToLight = normalize(lightVec);
    float NdotL = saturate(dot(normal, dirToLight));
    float diffuseTerm = max(minDiffuse, saturate(NdotL));
    gl_FragColor = vec4(lerp(diffuseColor.rgb, diffuseColor.rgb * diffuseTerm, texCoords.z), 1.0) * forcedColor;
}
)==";


//shaders to draw image
static const char* g_VS_DrawImage = R"==(
attribute vec3 inPos;
attribute vec2 inUV;

uniform mat4 mvp;

varying vec2 texCoords;

void main() {
    texCoords = inUV;
    gl_Position = mvp * vec4(inPos, 1.0);
}
)==";

static const char* g_FS_DrawImage = R"==(
varying vec2 texCoords;

uniform sampler2D texDiffuse;

void main() {
    gl_FragColor = texture2D(texDiffuse, texCoords);
}
)==";


//shaders to draw debug lines
static const char* g_VS_DrawDebug = R"==(
attribute vec3 inPos;
attribute vec4 inColor;

uniform mat4 mvp;

varying vec4 varColor;

void main() {
    varColor = inColor;
    gl_Position = mvp * vec4(inPos, 1.0);
}
)==";

static const char* g_FS_DrawDebug = R"==(
varying vec4 varColor;

void main() {
    gl_FragColor = varColor;
}
)==";

