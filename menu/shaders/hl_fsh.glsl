#version 460

/* S2 selection ring */

layout (location = 0) in vec2 fragLocal;
layout (location = 1) in vec2 fragWH;
layout (location = 2) in vec2 fragRadThick;
layout (location = 3) in vec4 fragC0;
layout (location = 4) in vec4 fragC1;
layout (location = 5) in vec4 fragC2;
layout (location = 6) in vec4 fragC3;
layout (location = 7) in float fragRot;

layout (location = 0) out vec4 outColor;

float rrect(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p - b * 0.5) - (b * 0.5 - vec2(r));
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main()
{
    float W = fragWH.x;
    float H = fragWH.y;
    float R = min(fragRadThick.x, min(W, H) * 0.5);
    float T = clamp(fragRadThick.y, 1.0, min(W, H) * 0.5 - 1.0);
    float Wi = max(W - 2.0 * T, 1.0);
    float Hi = max(H - 2.0 * T, 1.0);
    float Ri = max(R - T, 0.0);

    float dOut = rrect(fragLocal, vec2(W, H), R);
    float dIn = rrect(fragLocal - vec2(T), vec2(Wi, Hi), Ri);
    float a = (1.0 - smoothstep(-1.0, 1.0, dOut)) * smoothstep(-1.0, 1.0, dIn);
    if (a <= 0.0)
        discard;

    /* Arclength along the inner boundary, clockwise from the top edge. */
    vec2 q = fragLocal - vec2(T);
    float topLen = max(Wi - 2.0 * Ri, 0.0);
    float rightLen = max(Hi - 2.0 * Ri, 0.0);
    float arcLen = 1.5707963 * Ri;
    float P = 2.0 * topLen + 2.0 * rightLen + 4.0 * arcLen + 1e-4;
    int cx = (q.x < Ri) ? 0 : ((q.x > Wi - Ri) ? 2 : 1);
    int cy = (q.y < Ri) ? 0 : ((q.y > Hi - Ri) ? 2 : 1);
    float s = 0.0;
    if (cy == 0 && cx == 1) {
        s = q.x - Ri;
    } else if (cx == 2 && cy == 0) {
        float ang = atan(q.y - Ri, q.x - (Wi - Ri));
        s = topLen + (ang + 1.5707963) / 1.5707963 * arcLen;
    } else if (cx == 2 && cy == 1) {
        s = topLen + arcLen + (q.y - Ri);
    } else if (cx == 2 && cy == 2) {
        float ang = atan(q.y - (Hi - Ri), q.x - (Wi - Ri));
        s = topLen + arcLen + rightLen + ang / 1.5707963 * arcLen;
    } else if (cy == 2 && cx == 1) {
        s = topLen + rightLen + 2.0 * arcLen + ((Wi - Ri) - q.x);
    } else if (cx == 0 && cy == 2) {
        float ang = atan(q.y - (Hi - Ri), q.x - Ri);
        s = topLen + rightLen + topLen + 2.0 * arcLen + (ang - 1.5707963) / 1.5707963 * arcLen;
    } else if (cx == 0 && cy == 1) {
        s = 2.0 * topLen + rightLen + 3.0 * arcLen + ((Hi - Ri) - q.y);
    } else {
        float ang = atan(q.y - Ri, q.x - Ri);
        if (ang < 0.0)
            ang += 6.2831853;
        s = P - arcLen + (ang - 3.14159265) / 1.5707963 * arcLen;
    }

    float sn = fract(s / P + fragRot);
    float seg = sn * 4.0;
    int i = int(mod(seg, 4.0)) & 3;
    float f = fract(seg);
    vec3 A = (i == 0) ? fragC3.rgb : (i == 1) ? fragC0.rgb : (i == 2) ? fragC1.rgb : fragC2.rgb;
    vec3 B = (i == 0) ? fragC0.rgb : (i == 1) ? fragC1.rgb : (i == 2) ? fragC2.rgb : fragC3.rgb;
    outColor = vec4(mix(A, B, f), a);
}

