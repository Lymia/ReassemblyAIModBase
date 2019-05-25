
//
// MinCore.cpp - Various basic routines copied from outlaws-core.
// 

// Copyright (c) 2013-2016 Arthur Danskin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.


#include "StdAfx.h"
#include "Graphics.h"

#pragma region Graphics.cpp
uint rgbaf2argb(const float4 &rgba_)
{
	float4 rgba = rgba_;
    rgba.x = clamp(rgba.x, 0.f, 1.f);
    rgba.y = clamp(rgba.y, 0.f, 1.f);
    rgba.z = clamp(rgba.z, 0.f, 1.f);
    rgba.w = clamp(rgba.w, 0.f, 1.f);
    return (round_int(rgba.w * 255.f)<<24) | (round_int(rgba.x * 255.f)<<16) |
        (round_int(rgba.y * 255.f)<<8) | round_int(rgba.z * 255.f);
}

uint rgbf2rgb(const float3 &rgb_)
{
	float3 rgb = rgb_;
    rgb.x = clamp(rgb.x, 0.f, 1.f);
    rgb.y = clamp(rgb.y, 0.f, 1.f);
    rgb.z = clamp(rgb.z, 0.f, 1.f);
    return round_int(rgb.x * 255.f)<<16 | round_int(rgb.y * 255.f)<<8 | round_int(rgb.z * 255.f);
}

uint PremultiplyAlphaAXXX(uint color)
{
    float4 c = argb2rgbaf(color);
    c.x *= c.w;
    c.y *= c.w;
    c.z *= c.w;
    return rgbaf2argb(c);
}

uint PremultiplyAlphaAXXX(uint color, float alpha)
{
    float4 c = argb2rgbaf(color);
    c.x *= c.w;
    c.y *= c.w;
    c.z *= c.w;
    c.w = alpha;
    return rgbaf2argb(c);
}

uint PremultiplyAlphaXXX(uint color, float preAlpha, float alpha)
{
    float3 c = rgb2rgbf(color);
    c.x *= preAlpha;
    c.y *= preAlpha;
    c.z *= preAlpha;
    return ALPHAF(alpha)|rgbf2rgb(c);
}

uint32 argb2abgr(uint32 argb, float alpha)
{
    uint  a  = clamp(round_int(float(argb>>24) * alpha), 0, 0xff);
    uint  r0b = (argb&0xff00ff);
    return (a << 24) | (r0b << 16)| (argb&0x00ff00)  | (r0b >> 16);
}

uint32 argb2abgr_zero_alpha(uint32 argb, float alpha)
{
    if ((argb&ALPHA_OPAQUE) == 0)
        argb = argb|ALPHA_OPAQUE;
    uint  a  = clamp(round_int(float(argb>>24) * alpha), 0, 0xff);
    uint  r0b = (argb&0xff00ff);
    return (a << 24) | (r0b << 16)| (argb&0x00ff00)  | (r0b >> 16);
}    

uint rgbaf2abgr(const float4 &rgba_)
{
	float4 rgba = rgba_;
    rgba.x = clamp(rgba.x, 0.f, 1.f);
    rgba.y = clamp(rgba.y, 0.f, 1.f);
    rgba.z = clamp(rgba.z, 0.f, 1.f);
    rgba.w = clamp(rgba.w, 0.f, 1.f);
    rgba = round(rgba * 255.f);
    return (uint(rgba.w)<<24) | (uint(rgba.z)<<16) | (uint(rgba.y)<<8) | uint(rgba.x);
}

uint randlerpXXX(const std::initializer_list<uint>& lst)
{
    float3 color;
    float total = 0.f;
    foreach (uint cl, lst)
    {
        const float val = randrange(epsilon, 1.f);
        color += val * rgb2rgbf(cl);
        total += val;
    }
    return rgbf2rgb(color / total);
}

template<typename genType> genType glm_step(genType edge, genType x) {
	return glm::mix(static_cast<genType>(1), static_cast<genType>(0), x < edge);
}

float3 rgb2hsv_(float3 c)
{
    float4 K = float4(0.f, -1.f / 3.f, 2.f / 3.f, -1.f);
    float4 p = glm::mix(float4(c.z, c.y, K.w, K.z), float4(c.y, c.z, K.x, K.y), glm_step(c.z, c.y));
    float4 q = glm::mix(float4(p.x, p.y, p.w, c.x), float4(c.x, p.y, p.z, p.x), glm_step(p.x, c.x));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.f * d + e)), d / (q.x + e), q.x);
}

float3 hsv2rgb_(float3 c)
{
    float4 K = float4(1.f, 2.f / 3.f, 1.f / 3.f, 3.f);
    float3 p = abs(glm::fract(float3(c.x) + float3(K.x, K.y, K.z)) * 6.f - float3(K.w));
    return c.z * glm::mix(float3(K.x), clamp(p - float3(K.x), 0.f, 1.f), c.y);
}

float3 hsvf2rgbf(float3 hsv)
{
    hsv.x = modulo(hsv.x/360.f, 1.f);
    hsv.y = clamp(hsv.y, 0.f, 1.f);
    hsv.z = clamp(hsv.z, 0.f, 1.f);
    return hsv2rgb_(hsv);
}
#pragma endregion
#pragma region Win32Main.cpp
string ws2s(const std::wstring& wstr)
{
	DASSERT(wstr.size());
	int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wstr.length(), 0, 0, NULL, NULL);
	std::string r(len, '\0');
	int written = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wstr.length(), &r[0], len, NULL, NULL);
	DASSERT(len == written);
	r.resize(written);
	DASSERT(r.c_str()[r.size()] == '\0');
	return r;
}

std::wstring s2ws(const std::string& s)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), s.length(), NULL, 0);
	std::wstring r(len, L'\0');
	int written = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), s.length(), &r[0], len);
	DASSERT(len == written);
	r.resize(written);
	DASSERT(r.c_str()[r.size()] == L'\0');
	return r;
}

// used in ZipFile.cpp and stl_ext.cpp
void ReportWin32Err1(const char *msg, DWORD dwLastError, const char* file, int line)
{
	if (dwLastError == 0)
		return;                 // Don't want to see a "operation done successfully" error ;-)
	wchar_t lpBuffer[256] = L"?";
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, dwLastError,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		lpBuffer, (sizeof(lpBuffer) / sizeof(wchar_t)) - 1, NULL);
	const std::string buf = str_strip(ws2s(lpBuffer));
	::Report(str_format("%s:%d:error: %s failed: %#x %s\n", file, line, msg, dwLastError, buf.c_str()));
}
#pragma endregion
