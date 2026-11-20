#pragma once

#include <math.h>
#ifdef __cplusplus
#include <cmath>
#include <complex>
#include <random>
#include <valarray>
#include <algorithm>
#include <limits>
#endif

static inline float gomsvc_sinf(float x) { return (float)sin((double)x); }
static inline float gomsvc_cosf(float x) { return (float)cos((double)x); }
static inline float gomsvc_tanf(float x) { return (float)tan((double)x); }
static inline float gomsvc_asinf(float x) { return (float)asin((double)x); }
static inline float gomsvc_acosf(float x) { return (float)acos((double)x); }
static inline float gomsvc_atanf(float x) { return (float)atan((double)x); }
static inline float gomsvc_atan2f(float y, float x) { return (float)atan2((double)y, (double)x); }
static inline float gomsvc_powf(float x, float y) { return (float)pow((double)x, (double)y); }
static inline float gomsvc_expf(float x) { return (float)exp((double)x); }
static inline float gomsvc_logf(float x) { return (float)log((double)x); }

#ifdef __cplusplus
#define GOMSVC_UNARY(name) \
	static inline float gomsvc_##name(float x) { return (float)::name((double)x); } \
	static inline double gomsvc_##name(double x) { return ::name(x); } \
	static inline long double gomsvc_##name(long double x) { return ::name(x); } \
	template <class T> static inline double gomsvc_##name(T x) { return ::name((double)x); }

#define GOMSVC_BINARY(name) \
	static inline float gomsvc_##name(float a, float b) { return (float)::name((double)a, (double)b); } \
	static inline double gomsvc_##name(double a, double b) { return ::name(a, b); } \
	static inline long double gomsvc_##name(long double a, long double b) { return ::name(a, b); } \
	template <class A, class B> static inline double gomsvc_##name(A a, B b) { return ::name((double)a, (double)b); }

GOMSVC_UNARY(sin)
GOMSVC_UNARY(cos)
GOMSVC_UNARY(tan)
GOMSVC_UNARY(asin)
GOMSVC_UNARY(acos)
GOMSVC_UNARY(atan)
GOMSVC_UNARY(exp)
GOMSVC_UNARY(log)
GOMSVC_BINARY(atan2)
GOMSVC_BINARY(pow)

#undef GOMSVC_UNARY
#undef GOMSVC_BINARY

namespace std
{
	using ::gomsvc_sinf;
	using ::gomsvc_cosf;
	using ::gomsvc_tanf;
	using ::gomsvc_asinf;
	using ::gomsvc_acosf;
	using ::gomsvc_atanf;
	using ::gomsvc_atan2f;
	using ::gomsvc_powf;
	using ::gomsvc_expf;
	using ::gomsvc_logf;
	using ::gomsvc_sin;
	using ::gomsvc_cos;
	using ::gomsvc_tan;
	using ::gomsvc_asin;
	using ::gomsvc_acos;
	using ::gomsvc_atan;
	using ::gomsvc_exp;
	using ::gomsvc_log;
	using ::gomsvc_atan2;
	using ::gomsvc_pow;
}

#define sin(x) gomsvc_sin(x)
#define cos(x) gomsvc_cos(x)
#define tan(x) gomsvc_tan(x)
#define asin(x) gomsvc_asin(x)
#define acos(x) gomsvc_acos(x)
#define atan(x) gomsvc_atan(x)
#define exp(x) gomsvc_exp(x)
#define log(x) gomsvc_log(x)
#define atan2(a, b) gomsvc_atan2(a, b)
#define pow(a, b) gomsvc_pow(a, b)
#endif

#define sinf gomsvc_sinf
#define cosf gomsvc_cosf
#define tanf gomsvc_tanf
#define asinf gomsvc_asinf
#define acosf gomsvc_acosf
#define atanf gomsvc_atanf
#define atan2f gomsvc_atan2f
#define powf gomsvc_powf
#define expf gomsvc_expf
#define logf gomsvc_logf
