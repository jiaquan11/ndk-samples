// Copyright (C) 2018 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

//  ShaderPlain.fsh
//

#define USE_PHONG (1)

uniform lowp vec3       vMaterialAmbient;
uniform lowp vec4       vMaterialSpecular;

uniform sampler2D samplerObj;

varying mediump vec2    texCoord;
varying lowp vec4 diffuseLight;

#if USE_PHONG
uniform highp vec3   vLight0;
varying mediump vec3 position;
varying mediump vec3 normal;
#else
varying lowp vec4 colorSpecular;
#endif

void main()
{
#if USE_PHONG
    mediump vec3 halfVector = normalize(-vLight0 + position);
    mediump float NdotH = max(dot(normalize(normal), halfVector), 0.0);
    mediump float fPower = vMaterialSpecular.w;
    mediump float specular = pow(NdotH, fPower);

    lowp vec4 colorSpecular = vec4( vMaterialSpecular.xyz * specular, 1 );

    // increase ambient light to brighten the teapot :-)
    gl_FragColor = diffuseLight * texture2D(samplerObj, texCoord) +
            2.0f * vec4(vMaterialAmbient.xyz, 1.0f) + colorSpecular;
#else
    gl_FragColor = mix(colorDiffuse, texture2D(samplerObj, texCoord), 0.5) + colorSpecular;
#endif
}
