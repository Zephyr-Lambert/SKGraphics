// Depth-derived screen-space obscurance. No game shaders or third-party code.
sampler2D Depth : register(s0);
sampler2D Occlusion : register(s1);
sampler2D CharacterMask : register(s2);
float4 Projection : register(c0); // A, B, inverse projection X/Y
float4 Camera : register(c1);     // projection offsets X/Y, full inverse size
float4 Settings : register(c2);   // world radius, strength, angular bias, samples
float4 AOBufferInfo : register(c3);     // half-resolution inverse size; optional maximum view distance in Z

float linearDepth(float d) { return Projection.y / min(d-Projection.x, -0.0000001); }
float3 positionAt(float2 uv, float d) {
 float z=linearDepth(d);
 return float3((float2(2*uv.x-1,1-2*uv.y)-Camera.xy)*Projection.zw*z,z);
}
float2 depthUV(float2 uv) { return (floor(uv/Camera.zw)+0.5)*Camera.zw; }
float readDepth(float2 uv) { return tex2Dlod(Depth,float4(uv,0,0)).r; }
float4 obscurance(float2 uv:TEXCOORD0):COLOR0 {
 uv=depthUV(uv);
 float d=readDepth(uv);
 if(d>=0.9999999) return 1;
 float3 p=positionAt(uv,d);
 if(AOBufferInfo.z>0&&p.z>=AOBufferInfo.z)return 1;
 float2 dx=float2(4*Camera.z,0),dy=float2(0,4*Camera.w);
 float3 right=positionAt(uv+dx,readDepth(uv+dx))-p;
 float3 left=p-positionAt(uv-dx,readDepth(uv-dx));
 float3 down=positionAt(uv+dy,readDepth(uv+dy))-p;
 float3 up=p-positionAt(uv-dy,readDepth(uv-dy));
 float3 vx=abs(right.z)<abs(left.z)?right:left;
 float3 vy=abs(down.z)<abs(up.z)?down:up;
 // Average both sides on locally continuous surfaces. Retain the
 // one-sided estimate across large depth discontinuities.
 float normalTolerance=max(Settings.x*0.02,4*p.z*p.z/(max(-Projection.y,0.0001)*16777215.0));
 if(abs(right.z-left.z)<normalTolerance)vx=(right+left)*0.5;
 if(abs(down.z-up.z)<normalTolerance)vy=(down+up)*0.5;
 float3 normal=cross(vx,vy);
 float nn=dot(normal,normal);
 if(nn<0.000000000001) return 1;
 normal*=rsqrt(nn);if(dot(normal,p)>0)normal=-normal;
 float radius=Settings.x;
 float2 screenRadius=radius/(2*p.z*Projection.zw);
 // Stable spatial pattern, no temporal history or frame-dependent noise.
 float2 pixel=floor(uv/AOBufferInfo.xy);
 float angle=frac(52.9829189*frac(dot(pixel,float2(0.06711056,0.00583715))))*6.2831853;
 float sum=0;
 [loop] for(int i=0;i<32;i++) {
  if(i<Settings.w) {
   float a=angle+i*2.39996323;
   float r=sqrt((i+0.5)/Settings.w);
   float2 sampleUV=depthUV(uv+float2(cos(a),sin(a))*screenRadius*r);
   if(all(sampleUV>0)&&all(sampleUV<1)) {
    float sd=readDepth(sampleUV);
    if(sd<0.9999999) {
     float3 v=positionAt(sampleUV,sd)-p;
     float len2=dot(v,v);
     if(len2>=0.000001&&len2<radius*radius) {
      float distance=sqrt(len2),falloff=1-distance/radius;
      // Suppress tiny separations caused by finite depth precision.
      float depthError=max(p.z*p.z,(p.z+v.z)*(p.z+v.z))/(max(-Projection.y,0.0001)*16777215.0);
      float separationBias=max(radius*0.01,2*depthError);
      sum+=max(0,(dot(normal,v)-separationBias)/distance-Settings.z)*falloff*falloff;
     }
    }
   }
  }
 }
 float visibility=1-saturate(4*sum/Settings.w);
 return float4(visibility,visibility,visibility,1);
}
float4 applyAO(float2 uv:TEXCOORD0):COLOR0 {
 if(AOBufferInfo.w>0.5 && tex2Dlod(CharacterMask,float4(uv,0,0)).r>0.5)return 1;
 float d=readDepth(uv);if(d>=0.9999999)return 1;
 float z=linearDepth(d),sum=0,weight=0;
 if(AOBufferInfo.z>0&&z>=AOBufferInfo.z)return 1;
 // 3x3 depth-aware reconstruction smooths noise without mixing sky/backdrop
 // into the character or blindly blurring across depth discontinuities.
 float2 center=(floor(uv/AOBufferInfo.xy)+0.5)*AOBufferInfo.xy;
 [unroll] for(int y=-1;y<=1;y++) {
  [unroll] for(int x=-1;x<=1;x++) {
   float2 q=center+float2(x,y)*AOBufferInfo.xy;
   if(all(q>0)&&all(q<1)) {
    float qd=readDepth(q);
    if(qd<0.9999999) {
     float delta=abs(linearDepth(qd)-z);
     float w=exp2(-delta/max(Settings.x*0.1,0.001))*(x==0&&y==0?2:1);
     sum+=tex2Dlod(Occlusion,float4(q,0,0)).r*w;weight+=w;
    }
   }
  }
 }
 float visibility=weight>0.00001?sum/weight:1;
 float fade=AOBufferInfo.z>0?saturate((AOBufferInfo.z-z)/(AOBufferInfo.z*0.25)):1;
 float factor=max(0.1,1-(1-visibility)*Settings.y*fade);
 return float4(factor,factor,factor,1);
}
