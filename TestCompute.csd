float4 TestConstant;

RWTexture2D<float4> TestTexture;
Texture2D readTexture;
RWTexture2D<float4> TestTexture1;
Texture2D readTexture1;

[numthreads(5,5,1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	TestTexture[dtid.xy] = readTexture[dtid.xy];
	TestTexture1[dtid.xy] = readTexture1[dtid.xy];
}