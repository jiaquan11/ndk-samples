// Copyright (C) 2018 The Android Open Source Project
// SPDX-License-Identifier: Apache-2.0

//  ShaderPlain.fsh
//

uniform lowp vec3       vMaterialAmbient;
uniform lowp vec4       vMaterialSpecular;

uniform samplerCube samplerObj;

varying lowp vec4 colorDiffuse;

uniform highp vec3   vLight0;
varying mediump vec3 position;
varying mediump vec3 normal;

void main()
{
    mediump vec3 halfVector = normalize(-vLight0 + position);
    mediump float NdotH = max(dot(normalize(normal), halfVector), 0.0);
    mediump float fPower = vMaterialSpecular.w;
    mediump float specular = pow(NdotH, fPower);

    lowp vec4 colorSpecular = vec4( vMaterialSpecular.xyz * specular, 1 );
    mediump vec3 texCoord = normal;
    texCoord.y = -normal.y;
    gl_FragColor = colorDiffuse * textureCube(samplerObj, normalize(texCoord)) +
                      vec4(vMaterialAmbient.xyz, 1.0);
}
