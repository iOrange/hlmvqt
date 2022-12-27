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

#define saturate(val) clamp((val), 0.0, 1.0)
#define lerp(valA, valB, factorT) mix((valA), (valB), (factorT))

void main() {
    vec3 diffuseColor = texture2D(texDiffuse, texCoords.xy).rgb;
    vec3 normal = normalize(normalVec);
    vec3 dirToLight = normalize(lightVec);
    float NdotL = saturate(dot(normal, dirToLight));
    float diffuseTerm = saturate(NdotL + 0.15);
    gl_FragColor = vec4(lerp(diffuseColor, diffuseColor * diffuseTerm, texCoords.z), 1.0);
}
)==";
