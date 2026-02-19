/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#pragma once

#include <math.h>
#include <stddef.h>

#include "inc/MarlinConfigPre.h"


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
//
// Conditional type assignment magic. For example...
//
// typename IF<(MYOPT==12), int, float>::type myvar;
//
template <bool, class L, class R>
struct IF { typedef R type; };
template <class L, class R>
struct IF<true, L, R> { typedef L type; };

#define NUM_AXIS_GANG(V...) GANG_N(NUM_AXES, V)
#define NUM_AXIS_CODE(V...) CODE_N(NUM_AXES, V)
#define NUM_AXIS_LIST(V...) LIST_N(NUM_AXES, V)
#define NUM_AXIS_LIST_1(V)  LIST_N_1(NUM_AXES, V)
#define NUM_AXIS_ARRAY(V...) { NUM_AXIS_LIST(V) }
#define NUM_AXIS_ARRAY_1(V)  { NUM_AXIS_LIST_1(V) }
#define NUM_AXIS_ARGS(T...) NUM_AXIS_LIST(T x, T y, T z, T i, T j, T k, T u, T v, T w)
#define NUM_AXIS_ELEM(O)    NUM_AXIS_LIST(O.x, O.y, O.z, O.i, O.j, O.k, O.u, O.v, O.w)
#define NUM_AXIS_DEFS(T,V)  NUM_AXIS_LIST(T x=V, T y=V, T z=V, T i=V, T j=V, T k=V, T u=V, T v=V, T w=V)
#define MAIN_AXIS_NAMES     NUM_AXIS_LIST(X, Y, Z, I, J, K, U, V, W)
#define MAIN_AXIS_MAP(F)    MAP(F, MAIN_AXIS_NAMES)
#define STR_AXES_MAIN       NUM_AXIS_GANG("X", "Y", "Z", STR_I, STR_J, STR_K, STR_U, STR_V, STR_W)

#define LOGICAL_AXIS_GANG(E,V...) NUM_AXIS_GANG(V) GANG_ITEM_E(E)
#define LOGICAL_AXIS_CODE(E,V...) NUM_AXIS_CODE(V) CODE_ITEM_E(E)
#define LOGICAL_AXIS_LIST(E,V...) NUM_AXIS_LIST(V) LIST_ITEM_E(E)
#define LOGICAL_AXIS_LIST_1(V)    NUM_AXIS_LIST_1(V) LIST_ITEM_E(V)
#define LOGICAL_AXIS_ARRAY(E,V...) { LOGICAL_AXIS_LIST(E,V) }
#define LOGICAL_AXIS_ARRAY_1(V)    { LOGICAL_AXIS_LIST_1(V) }
#define LOGICAL_AXIS_ARGS(T...) LOGICAL_AXIS_LIST(T e, T x, T y, T z, T i, T j, T k, T u, T v, T w)
#define LOGICAL_AXIS_ELEM(O)    LOGICAL_AXIS_LIST(O.e, O.x, O.y, O.z, O.i, O.j, O.k, O.u, O.v, O.w)
#define LOGICAL_AXIS_DECL(T,V)  LOGICAL_AXIS_LIST(T e=V, T x=V, T y=V, T z=V, T i=V, T j=V, T k=V, T u=V, T v=V, T w=V)
#define LOGICAL_AXIS_NAMES      LOGICAL_AXIS_LIST(E, X, Y, Z, I, J, K, U, V, W)
#define LOGICAL_AXIS_MAP(F)     MAP(F, LOGICAL_AXIS_NAMES)
#define STR_AXES_LOGICAL     LOGICAL_AXIS_GANG("E", "X", "Y", "Z", STR_I, STR_J, STR_K, STR_U, STR_V, STR_W)

#define XYZ_GANG(V...) GANG_N(PRIMARY_LINEAR_AXES, V)
#define XYZ_CODE(V...) CODE_N(PRIMARY_LINEAR_AXES, V)

#define SECONDARY_AXIS_GANG(V...) GANG_N(SECONDARY_AXES, V)
#define SECONDARY_AXIS_CODE(V...) CODE_N(SECONDARY_AXES, V)

#if HAS_ROTATIONAL_AXES
  #define ROTATIONAL_AXIS_GANG(V...) GANG_N(ROTATIONAL_AXES, V)
#endif

#if HAS_EXTRUDERS
  #define LIST_ITEM_E(N) , N
  #define CODE_ITEM_E(N) ; N
  #define GANG_ITEM_E(N) N
#else
  #define LIST_ITEM_E(N)
  #define CODE_ITEM_E(N)
  #define GANG_ITEM_E(N)
#endif

#define AXIS_COLLISION(L) (AXIS4_NAME == L || AXIS5_NAME == L || AXIS6_NAME == L || AXIS7_NAME == L || AXIS8_NAME == L || AXIS9_NAME == L)

// General Flags for some number of states
template<size_t N>
struct Flags {
  typedef typename IF<(N>8), uint16_t, uint8_t>::type bits_t;
  typedef struct { bool b0:1, b1:1, b2:1, b3:1, b4:1, b5:1, b6:1, b7:1; } N8;
  typedef struct { bool b0:1, b1:1, b2:1, b3:1, b4:1, b5:1, b6:1, b7:1, b8:1, b9:1, b10:1, b11:1, b12:1, b13:1, b14:1, b15:1; } N16;
  union {
    bits_t b;
    typename IF<(N>8), N16, N8>::type flag;
  };
  void reset()                             { b = 0; }
  void set(const int n, const bool onoff)  { onoff ? set(n) : clear(n); }
  void set(const int n)                    { b |=  (bits_t)_BV(n); }
  void clear(const int n)                  { b &= ~(bits_t)_BV(n); }
  bool test(const int n) const             { return TEST(b, n); }
  bool operator[](const int n)       { return test(n); }
  bool operator[](const int n) const { return test(n); }
  int size() const                         { return sizeof(b); }
};

// Specialization for a single bool flag
template<>
struct Flags<1> {
  bool b;
  void reset()                            { b = false; }
  void set(const int n, const bool onoff) { onoff ? set(n) : clear(n); }
  void set(const int)                     { b = true; }
  void clear(const int)                   { b = false; }
  bool test(const int) const              { return b; }
  bool& operator[](const int)             { return b; }
  bool  operator[](const int) const       { return b; }
  int size() const                        { return sizeof(b); }
};

typedef Flags<8> flags_8_t;
typedef Flags<16> flags_16_t;

// Flags for some axis states, with per-axis aliases xyzijkuvwe
typedef struct AxisFlags {
  union {
    struct Flags<LOGICAL_AXES> flags;
    struct { bool LOGICAL_AXIS_LIST(e:1, x:1, y:1, z:1, i:1, j:1, k:1, u:1, v:1, w:1); };
  };
  void reset()                             { flags.reset(); }
  void set(const int n)                    { flags.set(n); }
  void set(const int n, const bool onoff)  { flags.set(n, onoff); }
  void clear(const int n)                  { flags.clear(n); }
  bool test(const int n) const             { return flags.test(n); }
  bool operator[](const int n)             { return flags[n]; }
  bool operator[](const int n) const       { return flags[n]; }
  int size() const                         { return sizeof(flags); }
} axis_flags_t;

//
// Enumerated axis indices
//
//  - X_AXIS, Y_AXIS, and Z_AXIS should be used for axes in Cartesian space
//  - A_AXIS, B_AXIS, and C_AXIS should be used for Steppers, corresponding to XYZ on Cartesians
//  - X_HEAD, Y_HEAD, and Z_HEAD should be used for Steppers on Core kinematics
//
enum AxisEnum : uint8_t {

  // Linear axes may be controlled directly or indirectly
  NUM_AXIS_LIST(X_AXIS, Y_AXIS, Z_AXIS, I_AXIS, J_AXIS, K_AXIS, U_AXIS, V_AXIS, W_AXIS)

  // Extruder axes may be considered distinctly
  #define _EN_ITEM(N) , E##N##_AXIS
  REPEAT(E_STEPPERS, _EN_ITEM)
  #undef _EN_ITEM

  // Core also keeps toolhead directions
  #if ANY(IS_CORE, MARKFORGED_XY, MARKFORGED_YX)
    , X_HEAD, Y_HEAD, Z_HEAD
  #endif

  // Distinct axes, including all E and Core
  , NUM_AXIS_ENUMS

  // Most of the time we refer only to the single E_AXIS
  #if HAS_EXTRUDERS
    , E_AXIS = E0_AXIS
  #endif

  // A, B, and C are for DELTA, SCARA, etc.
  , A_AXIS = X_AXIS
  #if HAS_Y_AXIS
    , B_AXIS = Y_AXIS
  #endif
  #if HAS_Z_AXIS
    , C_AXIS = Z_AXIS
  #endif

  // To refer to all or none
  , ALL_AXES_ENUM = 0xFE, NO_AXIS_ENUM = 0xFF
};

typedef IF<(NUM_AXIS_ENUMS > 8), uint16_t, uint8_t>::type axis_bits_t;

//
// Loop over axes
//
#define LOOP_ABC(VAR) LOOP_S_LE_N(VAR, A_AXIS, C_AXIS)
#define LOOP_NUM_AXES(VAR) LOOP_S_L_N(VAR, X_AXIS, NUM_AXES)
#define LOOP_LOGICAL_AXES(VAR) LOOP_S_L_N(VAR, X_AXIS, LOGICAL_AXES)
#define LOOP_DISTINCT_AXES(VAR) LOOP_S_L_N(VAR, X_AXIS, DISTINCT_AXES)
#define LOOP_DISTINCT_E(VAR) LOOP_L_N(VAR, DISTINCT_E)

//
// feedRate_t is just a humble float
//
typedef float feedRate_t;

//
// celsius_t is the native unit of temperature. Signed to handle a disconnected thermistor value (-14).
// For more resolition (e.g., for a chocolate printer) this may later be changed to Celsius x 100
//
typedef uint16_t raw_adc_t;
typedef int16_t celsius_t;
typedef float celsius_float_t;

// Conversion macros
#define MMM_TO_MMS(MM_M) feedRate_t(static_cast<float>(MM_M) / 60.0f)
#define MMS_TO_MMM(MM_S) (static_cast<float>(MM_S) * 60.0f)

//
// Coordinates structures for XY, XYZ, XYZE...
//

// Helpers
#define _RECIP(N) ((N) ? 1.0f / static_cast<float>(N) : 0.0f)
#define _ABS(N) ((N) < 0 ? -(N) : (N))
#define _LS(N)  (N = (T)(uint32_t(N) << v))
#define _RS(N)  (N = (T)(uint32_t(N) >> v))
#define FI FORCE_INLINE

/// Tag for logical position strong types.
/// Logical vectors are used only on the G-Code level - those are XYZE coordinates BEFORE hotend offseds (and workspace offset) are applied
struct LogicalPosTag {};

/// Tag for native position strong types.
/// Native positions are AFTER hotend offsets applied, but BEFORE modifiers (MBL, skew, ...) applied.
/// Vast majority of the firmware works with native coordinates
struct NativePosTag {};

/// Tag for machine position strong types.
/// Machine positions are AFTER everything applied (hotend offsets, MBL, skew, ...).
/// This is what planner works with.
/// REMOVEME: For now, make it just an alias for NativePosTag so that we can migrate gradually.
using MachinePosTag = NativePosTag;

// Forward declarations
template<typename T, typename Tag = NativePosTag> struct XYval;
template<typename T, typename Tag = NativePosTag> struct XYZval;
template<typename T, typename Tag = NativePosTag> struct XYZEval;

typedef struct XYval<bool>          xy_bool_t;
typedef struct XYZval<bool>        xyz_bool_t;
typedef struct XYZEval<bool>      xyze_bool_t;

typedef struct XYval<char>          xy_char_t;
typedef struct XYZval<char>        xyz_char_t;
typedef struct XYZEval<char>      xyze_char_t;

typedef struct XYval<unsigned char>     xy_uchar_t;
typedef struct XYZval<unsigned char>   xyz_uchar_t;
typedef struct XYZEval<unsigned char> xyze_uchar_t;

typedef struct XYval<int8_t>        xy_int8_t;
typedef struct XYZval<int8_t>      xyz_int8_t;
typedef struct XYZEval<int8_t>    xyze_int8_t;

typedef struct XYval<uint8_t>      xy_uint8_t;
typedef struct XYZval<uint8_t>    xyz_uint8_t;
typedef struct XYZEval<uint8_t>  xyze_uint8_t;

typedef struct XYval<int16_t>        xy_int_t;
typedef struct XYZval<int16_t>      xyz_int_t;
typedef struct XYZEval<int16_t>    xyze_int_t;

typedef struct XYval<uint16_t>      xy_uint_t;
typedef struct XYZval<uint16_t>    xyz_uint_t;
typedef struct XYZEval<uint16_t>  xyze_uint_t;

typedef struct XYval<int32_t>       xy_long_t;
typedef struct XYZval<int32_t>     xyz_long_t;
typedef struct XYZEval<int32_t>   xyze_long_t;

typedef struct XYval<uint32_t>     xy_ulong_t;
typedef struct XYZval<uint32_t>   xyz_ulong_t;
typedef struct XYZEval<uint32_t> xyze_ulong_t;

typedef struct XYZval<volatile int32_t>   xyz_vlong_t;
typedef struct XYZEval<volatile int32_t> xyze_vlong_t;

typedef struct XYval<float>        xy_float_t;
typedef struct XYZval<float>      xyz_float_t;
typedef struct XYZEval<float>    xyze_float_t;

typedef struct XYZval<double>    xyz_double_t;
typedef struct XYZEval<double>    xyze_double_t;

typedef struct XYval<feedRate_t>     xy_feedrate_t;
typedef struct XYZval<feedRate_t>   xyz_feedrate_t;
typedef struct XYZEval<feedRate_t> xyze_feedrate_t;

typedef xy_uint8_t xy_byte_t;
typedef xyz_uint8_t xyz_byte_t;
typedef xyze_uint8_t xyze_byte_t;

typedef xyz_long_t abc_long_t;
typedef xyze_long_t abce_long_t;
typedef xyz_ulong_t abc_ulong_t;
typedef xyze_ulong_t abce_ulong_t;

typedef xy_float_t xy_pos_t;
typedef xyz_float_t xyz_pos_t;
typedef xyze_float_t xyze_pos_t;

typedef xy_float_t ab_float_t;
typedef xyz_float_t abc_float_t;
typedef xyze_float_t abce_float_t;

typedef ab_float_t ab_pos_t;
typedef abc_float_t abc_pos_t;
typedef abce_float_t abce_pos_t;

using MachinePosXY = XYval<float, MachinePosTag>;
using MachinePosXYZ = XYZval<float, MachinePosTag>;
using MachinePosXYZE = XYZEval<float, MachinePosTag>;

// External conversion methods
template<template <typename T, typename Tag> typename V, typename T>
[[nodiscard]] V<T, LogicalPosTag> toLogical(const V<T, NativePosTag> &v);

template<template <typename T, typename Tag> typename V, typename T>
[[nodiscard]] V<T, NativePosTag> toNative(const V<T, LogicalPosTag> &v);

//
// Paired XY coordinates, counters, flags, etc.
// Tag is used for creating strong type variants
//
template<typename T, typename Tag>
struct XYval {
  using XYZval = ::XYZval<T, Tag>;
  using XYZEval = ::XYZEval<T, Tag>;

  union {
    struct { T x = 0, y = 0; };
    struct { T a, b; };
    T pos[2];
  };

  // Set all to 0
  FI void reset()                                       { x = y = 0; }

  // Setters taking struct types and arrays
  FI void set(const T px)                               { x = px; }
  #if HAS_Y_AXIS
    FI void set(const T px, const T py)                 { x = px; y = py; }
    FI void set(const T (&arr)[XY])                     { x = arr[0]; y = arr[1]; }
  #endif
  #if NUM_AXES > XY
    FI void set(const T (&arr)[NUM_AXES])               { x = arr[0]; y = arr[1]; }
  #endif
  #if LOGICAL_AXES > NUM_AXES
    FI void set(const T (&arr)[LOGICAL_AXES])           { x = arr[0]; y = arr[1]; }
    #if DISTINCT_AXES > LOGICAL_AXES
      FI void set(const T (&arr)[DISTINCT_AXES])        { x = arr[0]; y = arr[1]; }
    #endif
  #endif

  // Length reduced to one dimension
  FI T magnitude()                                const { return (T)sqrtf(x*x + y*y); }
  // Pointer to the data as a simple array
  FI operator T* ()                                     { return pos; }
  // If any element is true then it's true
  FI operator bool()                                    { return x || y; }

  // Explicit copy and copies with conversion
  FI XYval           copy()                    const { return *this; }
  FI XYval            ABS()                    const { return { T(_ABS(x)), T(_ABS(y)) }; }
  FI XYval<int16_t, Tag>    asInt()                          { return { int16_t(x), int16_t(y) }; }
  FI XYval<int16_t, Tag>    asInt()                    const { return { int16_t(x), int16_t(y) }; }
  FI XYval<int32_t, Tag>   asLong()                          { return { int32_t(x), int32_t(y) }; }
  FI XYval<int32_t, Tag>   asLong()                    const { return { int32_t(x), int32_t(y) }; }
  FI XYval<int32_t, Tag>   ROUNDL()                          { return { int32_t(LROUND(x)), int32_t(LROUND(y)) }; }
  FI XYval<int32_t, Tag>   ROUNDL()                    const { return { int32_t(LROUND(x)), int32_t(LROUND(y)) }; }
  FI XYval<float, Tag>    asFloat()                          { return { static_cast<float>(x), static_cast<float>(y) }; }
  FI XYval<float, Tag>    asFloat()                    const { return { static_cast<float>(x), static_cast<float>(y) }; }
  FI XYval<float, Tag> reciprocal()                    const { return {  _RECIP(x),  _RECIP(y) }; }

  /// Basically a reinterpret_cast for the vectors. Only use when you know what you're doing
  template<typename NewTag>
  [[deprecated("UNSAFE. Only use when you know what you're doing")]]
  FI XYval<T, NewTag> to_tag()                    const { return { x, y }; }

  // Marlin workspace shifting is done with G92 and M206
  FI auto  asLogical() const requires(std::is_same_v<Tag, NativePosTag>) { return toLogical(*this); }
  FI auto   asNative() const requires(std::is_same_v<Tag, LogicalPosTag>) { return toNative(*this); }

  // Cast to a type with more fields by making a new object
  explicit FI operator XYZval()                         const { return NUM_AXIS_ARRAY(x, y, 0, 0, 0, 0, 0, 0, 0); }
  explicit FI operator XYZEval()                        const { return LOGICAL_AXIS_ARRAY(0, x, y, 0, 0, 0, 0, 0, 0, 0); }

  // Accessor via an AxisEnum (or any integer) [index]
  FI       T&  operator[](const int n)                  { return pos[n]; }
  FI const T&  operator[](const int n)            const { return pos[n]; }

  // Assignment operator overrides do the expected thing
  FI XYval& operator=(const XYval&) = default;

  // Don't use these, they are dangerous. Always retype explicitly.
  FI XYval& operator= (const T v) = delete;
  FI XYval& operator= (const XYZval  &rs) = delete;
  FI XYval& operator= (const XYZEval &rs) = delete;

  // Override other operators to get intuitive behaviors
  FI XYval  operator+ (const XYval   &rs)   const { XYval ls = *this; ls.x += rs.x; ls.y += rs.y; return ls; }
  FI XYval  operator- (const XYval   &rs)   const { XYval ls = *this; ls.x -= rs.x; ls.y -= rs.y; return ls; }
  FI XYval  operator* (const XYval   &rs)   const { XYval ls = *this; ls.x *= rs.x; ls.y *= rs.y; return ls; }
  FI XYval  operator/ (const XYval   &rs)   const { XYval ls = *this; ls.x /= rs.x; ls.y /= rs.y; return ls; }
  FI XYval  operator+ (const XYZval  &rs)   const { XYval ls = *this; ls.x += rs.x; ls.y += rs.y; return ls; }
  FI XYval  operator- (const XYZval  &rs)   const { XYval ls = *this; ls.x -= rs.x; ls.y -= rs.y; return ls; }
  FI XYval  operator* (const XYZval  &rs)   const { XYval ls = *this; ls.x *= rs.x; ls.y *= rs.y; return ls; }
  FI XYval  operator/ (const XYZval  &rs)   const { XYval ls = *this; ls.x /= rs.x; ls.y /= rs.y; return ls; }
  FI XYval  operator+ (const XYZEval &rs)   const { XYval ls = *this; ls.x += rs.x; ls.y += rs.y; return ls; }
  FI XYval  operator- (const XYZEval &rs)   const { XYval ls = *this; ls.x -= rs.x; ls.y -= rs.y; return ls; }
  FI XYval  operator* (const XYZEval &rs)   const { XYval ls = *this; ls.x *= rs.x; ls.y *= rs.y; return ls; }
  FI XYval  operator/ (const XYZEval &rs)   const { XYval ls = *this; ls.x /= rs.x; ls.y /= rs.y; return ls; }
  FI XYval  operator* (const float &v)         const { XYval ls = *this; ls.x *= v;    ls.y *= v;    return ls; }
  FI XYval  operator* (const int &v)           const { XYval ls = *this; ls.x *= v;    ls.y *= v;    return ls; }
  FI XYval  operator/ (const float &v)         const { XYval ls = *this; ls.x /= v;    ls.y /= v;    return ls; }
  FI XYval  operator/ (const int &v)           const { XYval ls = *this; ls.x /= v;    ls.y /= v;    return ls; }
  FI XYval  operator>>(const int &v)           const { XYval ls = *this; _RS(ls.x);    _RS(ls.y);    return ls; }
  FI XYval  operator<<(const int &v)           const { XYval ls = *this; _LS(ls.x);    _LS(ls.y);    return ls; }
  FI const XYval operator-()                   const { XYval o = *this; o.x = -x; o.y = -y; return o; }

  // Modifier operators
  FI XYval& operator+=(const XYval   &rs)         { x += rs.x; y += rs.y; return *this; }
  FI XYval& operator-=(const XYval   &rs)         { x -= rs.x; y -= rs.y; return *this; }
  FI XYval& operator*=(const XYval   &rs)         { x *= rs.x; y *= rs.y; return *this; }
  FI XYval& operator+=(const XYZval  &rs)         { x += rs.x; y += rs.y; return *this; }
  FI XYval& operator-=(const XYZval  &rs)         { x -= rs.x; y -= rs.y; return *this; }
  FI XYval& operator*=(const XYZval  &rs)         { x *= rs.x; y *= rs.y; return *this; }
  FI XYval& operator+=(const XYZEval &rs)         { x += rs.x; y += rs.y; return *this; }
  FI XYval& operator-=(const XYZEval &rs)         { x -= rs.x; y -= rs.y; return *this; }
  FI XYval& operator*=(const XYZEval &rs)         { x *= rs.x; y *= rs.y; return *this; }
  FI XYval& operator*=(const float &v)               { x *= v;    y *= v;    return *this; }
  FI XYval& operator*=(const int &v)                 { x *= v;    y *= v;    return *this; }
  FI XYval& operator>>=(const int &v)                { _RS(x);    _RS(y);    return *this; }
  FI XYval& operator<<=(const int &v)                { _LS(x);    _LS(y);    return *this; }

  // Exact comparisons. For floats a "NEAR" operation may be better.
  FI bool      operator==(const XYval   &rs)   const { return x == rs.x && y == rs.y; }
  FI bool      operator!=(const XYval   &rs)   const { return !operator==(rs); }
};

//
// Linear Axes coordinates, counters, flags, etc.
// Tag is used for creating strong type variants
//
template<typename T, typename Tag>
struct XYZval {
  using XYval = ::XYval<T, Tag>;
  using XYZEval = ::XYZEval<T, Tag>;

  union {
    struct { T NUM_AXIS_DEFS(,0); };
    struct { T NUM_AXIS_LIST(a, b, c, _i, _j, _k, _u, _v, _w); };
    T pos[NUM_AXES];
  };

  // Set all to 0
  FI void reset()                                      { NUM_AXIS_GANG(x =, y =, z =, i =, j =, k =, u =, v =, w =) 0; }

  // Setters taking struct types and arrays
  FI void set(const T px)                              { x = px; }
  FI void set(const T px, const T py)                  { x = px; y = py; }
  FI void set(const XYval pxy)                      { x = pxy.x; y = pxy.y; }
  FI void set(const XYval pxy, const T pz)          { NUM_AXIS_CODE(x = pxy.x, y = pxy.y, z = pz, NOOP, NOOP, NOOP, NOOP, NOOP, NOOP); }
  FI void set(const T (&arr)[XY])                      { x = arr[0]; y = arr[1]; }
  #if HAS_Z_AXIS
    FI void set(const T (&arr)[NUM_AXES])              { NUM_AXIS_CODE(x = arr[0], y = arr[1], z = arr[2], i = arr[3], j = arr[4], k = arr[5], u = arr[6], v = arr[7], w = arr[8]); }
    FI void set(NUM_AXIS_ARGS(const T))                { NUM_AXIS_CODE(a = x,      b = y,      c = z,     _i = i,     _j = j,     _k = k,     _u = u,     _v = v,     _w = w   ); }
  #endif
  #if LOGICAL_AXES > NUM_AXES
    FI void set(const T (&arr)[LOGICAL_AXES])          { NUM_AXIS_CODE(x = arr[0], y = arr[1], z = arr[2], i = arr[3], j = arr[4], k = arr[5], u = arr[6], v = arr[7], w = arr[8]); }
    FI void set(LOGICAL_AXIS_ARGS(const T))            { NUM_AXIS_CODE(a = x,      b = y,      c = z,     _i = i,     _j = j,     _k = k,     _u = u,     _v = v,     _w = w   ); }
    #if DISTINCT_AXES > LOGICAL_AXES
      FI void set(const T (&arr)[DISTINCT_AXES])       { NUM_AXIS_CODE(x = arr[0], y = arr[1], z = arr[2], i = arr[3], j = arr[4], k = arr[5], u = arr[6], v = arr[7], w = arr[8]); }
    #endif
  #endif
  #if HAS_I_AXIS
    FI void set(const T px, const T py, const T pz) { x = px; y = py; z = pz; }
  #endif
  #if HAS_J_AXIS
    FI void set(const T px, const T py, const T pz, const T pi) { x = px; y = py; z = pz; i = pi; }
  #endif
  #if HAS_K_AXIS
    FI void set(const T px, const T py, const T pz, const T pi, const T pj) { x = px; y = py; z = pz; i = pi; j = pj; }
  #endif
  #if HAS_U_AXIS
    FI void set(const T px, const T py, const T pz, const T pi, const T pj, const T pk) { x = px; y = py; z = pz; i = pi; j = pj; k = pk; }
  #endif
  #if HAS_V_AXIS
    FI void set(const T px, const T py, const T pz, const T pi, const T pj, const T pk, const T pu) { x = px; y = py; z = pz; i = pi; j = pj; k = pk; u = pu; }
  #endif
  #if HAS_W_AXIS
    FI void set(const T px, const T py, const T pz, const T pi, const T pj, const T pk, const T pu, const T pv) { x = px; y = py; z = pz; i = pi; j = pj; k = pk; u = pu; v = pv; }
  #endif

  // Length reduced to one dimension
  FI T magnitude()                               const { return (T)sqrtf(NUM_AXIS_GANG(x*x, + y*y, + z*z, + i*i, + j*j, + k*k, + u*u, + v*v, + w*w)); }
  // Pointer to the data as a simple array
  FI operator T* ()                                    { return pos; }
  // If any element is true then it's true
  FI operator bool()                                   { return NUM_AXIS_GANG(x, || y, || z, || i, || j, || k, || u, || v, || w); }

  // Explicit copy and copies with conversion
  FI XYZval          copy()                   const { XYZval o = *this; return o; }
  FI XYZval           ABS()                   const { return NUM_AXIS_ARRAY(T(_ABS(x)), T(_ABS(y)), T(_ABS(z)), T(_ABS(i)), T(_ABS(j)), T(_ABS(k)), T(_ABS(u)), T(_ABS(v)), T(_ABS(w))); }
  FI XYZval<int16_t, Tag>   asInt()                         { return NUM_AXIS_ARRAY(int16_t(x), int16_t(y), int16_t(z), int16_t(i), int16_t(j), int16_t(k), int16_t(u), int16_t(v), int16_t(w)); }
  FI XYZval<int16_t, Tag>   asInt()                   const { return NUM_AXIS_ARRAY(int16_t(x), int16_t(y), int16_t(z), int16_t(i), int16_t(j), int16_t(k), int16_t(u), int16_t(v), int16_t(w)); }
  FI XYZval<int32_t, Tag>  asLong()                         { return NUM_AXIS_ARRAY(int32_t(x), int32_t(y), int32_t(z), int32_t(i), int32_t(j), int32_t(k), int32_t(u), int32_t(v), int32_t(w)); }
  FI XYZval<int32_t, Tag>  asLong()                   const { return NUM_AXIS_ARRAY(int32_t(x), int32_t(y), int32_t(z), int32_t(i), int32_t(j), int32_t(k), int32_t(u), int32_t(v), int32_t(w)); }
  FI XYZval<int32_t, Tag>  ROUNDL()                         { return NUM_AXIS_ARRAY(int32_t(LROUND(x)), int32_t(LROUND(y)), int32_t(LROUND(z)), int32_t(LROUND(i)), int32_t(LROUND(j)), int32_t(LROUND(k)), int32_t(LROUND(u)), int32_t(LROUND(v)), int32_t(LROUND(w))); }
  FI XYZval<int32_t, Tag>  ROUNDL()                   const { return NUM_AXIS_ARRAY(int32_t(LROUND(x)), int32_t(LROUND(y)), int32_t(LROUND(z)), int32_t(LROUND(i)), int32_t(LROUND(j)), int32_t(LROUND(k)), int32_t(LROUND(u)), int32_t(LROUND(v)), int32_t(LROUND(w))); }
  FI XYZval<float, Tag>   asFloat()                         { return NUM_AXIS_ARRAY(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(i), static_cast<float>(j), static_cast<float>(k), static_cast<float>(u), static_cast<float>(v), static_cast<float>(w)); }
  FI XYZval<float, Tag>   asFloat()                   const { return NUM_AXIS_ARRAY(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(i), static_cast<float>(j), static_cast<float>(k), static_cast<float>(u), static_cast<float>(v), static_cast<float>(w)); }
  FI XYZval<float, Tag> reciprocal()                  const { return NUM_AXIS_ARRAY(_RECIP(x),  _RECIP(y),  _RECIP(z),  _RECIP(i),  _RECIP(j),  _RECIP(k),  _RECIP(u),  _RECIP(v),  _RECIP(w)); }

  /// Basically a reinterpret_cast for the vectors. Only use when you know what you're doing
  template<typename NewTag>
  [[deprecated("UNSAFE. Only use when you know what you're doing")]]
  FI XYZval<T, NewTag> to_tag()                    const { return NUM_AXIS_ARRAY(x, y, z, i, j, k, u, v, w); }

  // Marlin workspace shifting is done with G92 and M206
  FI auto  asLogical() const requires(std::is_same_v<Tag, NativePosTag>) { return toLogical(*this); }
  FI auto   asNative() const requires(std::is_same_v<Tag, LogicalPosTag>) { return toNative(*this); }

  // In-place cast to types having fewer fields
  FI operator XYval()                         const { return XYval{x, y}; }

  // Cast to a type with more fields by making a new object
  explicit FI operator       XYZEval()                 const { return LOGICAL_AXIS_ARRAY(0, x, y, z, i, j, k, u, v, w); }

  // Accessor via an AxisEnum (or any integer) [index]
  FI       T&   operator[](const int n)                { return pos[n]; }
  FI const T&   operator[](const int n)          const { return pos[n]; }

  // Assignment operator overrides do the expected thing
  FI XYZval& operator=(const XYZval&) = default;

  // Don't use these, they are dangerous. Always retype explicitly.
  FI XYZval& operator= (const T v) = delete;
  FI XYZval& operator= (const XYval   &rs) = delete;
  FI XYZval& operator= (const XYZEval &rs) = delete;

  // Override other operators to get intuitive behaviors
  FI XYZval  operator+ (const XYval   &rs) const { XYZval ls = *this; NUM_AXIS_CODE(ls.x += rs.x, ls.y += rs.y, NOOP        , NOOP        , NOOP        , NOOP        , NOOP        , NOOP        , NOOP        ); return ls; }
  FI XYZval  operator- (const XYval   &rs) const { XYZval ls = *this; NUM_AXIS_CODE(ls.x -= rs.x, ls.y -= rs.y, NOOP        , NOOP        , NOOP        , NOOP        , NOOP        , NOOP        , NOOP        ); return ls; }
  FI XYZval  operator* (const XYval   &rs) const { XYZval ls = *this; NUM_AXIS_CODE(ls.x *= rs.x, ls.y *= rs.y, NOOP        , NOOP        , NOOP        , NOOP        , NOOP        , NOOP        , NOOP        ); return ls; }
  FI XYZval  operator/ (const XYval   &rs) const { XYZval ls = *this; NUM_AXIS_CODE(ls.x /= rs.x, ls.y /= rs.y, NOOP        , NOOP        , NOOP        , NOOP        , NOOP        , NOOP        , NOOP        ); return ls; }
  FI XYZval  operator+ (const XYZval  &rs) const { XYZval ls = *this; NUM_AXIS_CODE(ls.x += rs.x, ls.y += rs.y, ls.z += rs.z, ls.i += rs.i, ls.j += rs.j, ls.k += rs.k, ls.u += rs.u, ls.v += rs.v, ls.w += rs.w); return ls; }
  FI XYZval  operator- (const XYZval  &rs) const { XYZval ls = *this; NUM_AXIS_CODE(ls.x -= rs.x, ls.y -= rs.y, ls.z -= rs.z, ls.i -= rs.i, ls.j -= rs.j, ls.k -= rs.k, ls.u -= rs.u, ls.v -= rs.v, ls.w -= rs.w); return ls; }
  FI XYZval  operator* (const XYZval  &rs) const { XYZval ls = *this; NUM_AXIS_CODE(ls.x *= rs.x, ls.y *= rs.y, ls.z *= rs.z, ls.i *= rs.i, ls.j *= rs.j, ls.k *= rs.k, ls.u *= rs.u, ls.v *= rs.v, ls.w *= rs.w); return ls; }
  FI XYZval  operator/ (const XYZval  &rs) const { XYZval ls = *this; NUM_AXIS_CODE(ls.x /= rs.x, ls.y /= rs.y, ls.z /= rs.z, ls.i /= rs.i, ls.j /= rs.j, ls.k /= rs.k, ls.u /= rs.u, ls.v /= rs.v, ls.w /= rs.w); return ls; }
  FI XYZval  operator+ (const XYZEval &rs) const { XYZval ls = *this; NUM_AXIS_CODE(ls.x += rs.x, ls.y += rs.y, ls.z += rs.z, ls.i += rs.i, ls.j += rs.j, ls.k += rs.k, ls.u += rs.u, ls.v += rs.v, ls.w += rs.w); return ls; }
  FI XYZval  operator- (const XYZEval &rs) const { XYZval ls = *this; NUM_AXIS_CODE(ls.x -= rs.x, ls.y -= rs.y, ls.z -= rs.z, ls.i -= rs.i, ls.j -= rs.j, ls.k -= rs.k, ls.u -= rs.u, ls.v -= rs.v, ls.w -= rs.w); return ls; }
  FI XYZval  operator* (const XYZEval &rs) const { XYZval ls = *this; NUM_AXIS_CODE(ls.x *= rs.x, ls.y *= rs.y, ls.z *= rs.z, ls.i *= rs.i, ls.j *= rs.j, ls.k *= rs.k, ls.u *= rs.u, ls.v *= rs.v, ls.w *= rs.w); return ls; }
  FI XYZval  operator/ (const XYZEval &rs) const { XYZval ls = *this; NUM_AXIS_CODE(ls.x /= rs.x, ls.y /= rs.y, ls.z /= rs.z, ls.i /= rs.i, ls.j /= rs.j, ls.k /= rs.k, ls.u /= rs.u, ls.v /= rs.v, ls.w /= rs.w); return ls; }
  FI XYZval  operator* (const float &v)       const { XYZval ls = *this; NUM_AXIS_CODE(ls.x *= v,    ls.y *= v,    ls.z *= v,    ls.i *= v,    ls.j *= v,    ls.k *= v,    ls.u *= v,    ls.v *= v,    ls.w *= v   ); return ls; }
  FI XYZval  operator* (const int &v)         const { XYZval ls = *this; NUM_AXIS_CODE(ls.x *= v,    ls.y *= v,    ls.z *= v,    ls.i *= v,    ls.j *= v,    ls.k *= v,    ls.u *= v,    ls.v *= v,    ls.w *= v   ); return ls; }
  FI XYZval  operator/ (const float &v)       const { XYZval ls = *this; NUM_AXIS_CODE(ls.x /= v,    ls.y /= v,    ls.z /= v,    ls.i /= v,    ls.j /= v,    ls.k /= v,    ls.u /= v,    ls.v /= v,    ls.w /= v   ); return ls; }
  FI XYZval  operator/ (const int &v)         const { XYZval ls = *this; NUM_AXIS_CODE(ls.x /= v,    ls.y /= v,    ls.z /= v,    ls.i /= v,    ls.j /= v,    ls.k /= v,    ls.u /= v,    ls.v /= v,    ls.w /= v   ); return ls; }
  FI XYZval  operator/ (const uint32_t &v)    const { XYZval ls = *this; NUM_AXIS_CODE(ls.x /= v,    ls.y /= v,    ls.z /= v,    ls.i /= v,    ls.j /= v,    ls.k /= v,    ls.u /= v,    ls.v /= v,    ls.w /= v   ); return ls; }
  FI XYZval  operator>>(const int &v)         const { XYZval ls = *this; NUM_AXIS_CODE(_RS(ls.x),    _RS(ls.y),    _RS(ls.z),    _RS(ls.i),    _RS(ls.j),    _RS(ls.k),    _RS(ls.u),    _RS(ls.v),    _RS(ls.w)   ); return ls; }
  FI XYZval  operator<<(const int &v)         const { XYZval ls = *this; NUM_AXIS_CODE(_LS(ls.x),    _LS(ls.y),    _LS(ls.z),    _LS(ls.i),    _LS(ls.j),    _LS(ls.k),    _LS(ls.u),    _LS(ls.v),    _LS(ls.w)   ); return ls; }
  FI const XYZval operator-()                 const { XYZval o = *this; NUM_AXIS_CODE(o.x = -x, o.y = -y, o.z = -z, o.i = -i, o.j = -j, o.k = -k, o.u = -u, o.v = -v, o.w = -w); return o; }

  // Modifier operators
  FI XYZval& operator+=(const XYval   &rs)       { NUM_AXIS_CODE(x += rs.x, y += rs.y, NOOP,      NOOP,      NOOP,      NOOP,      NOOP,      NOOP,      NOOP     ); return *this; }
  FI XYZval& operator-=(const XYval   &rs)       { NUM_AXIS_CODE(x -= rs.x, y -= rs.y, NOOP,      NOOP,      NOOP,      NOOP,      NOOP,      NOOP,      NOOP     ); return *this; }
  FI XYZval& operator*=(const XYval   &rs)       { NUM_AXIS_CODE(x *= rs.x, y *= rs.y, NOOP,      NOOP,      NOOP,      NOOP,      NOOP,      NOOP,      NOOP     ); return *this; }
  FI XYZval& operator/=(const XYval   &rs)       { NUM_AXIS_CODE(x /= rs.x, y /= rs.y, NOOP,      NOOP,      NOOP,      NOOP,      NOOP,      NOOP,      NOOP     ); return *this; }
  FI XYZval& operator+=(const XYZval  &rs)       { NUM_AXIS_CODE(x += rs.x, y += rs.y, z += rs.z, i += rs.i, j += rs.j, k += rs.k, u += rs.u, v += rs.v, w += rs.w); return *this; }
  FI XYZval& operator-=(const XYZval  &rs)       { NUM_AXIS_CODE(x -= rs.x, y -= rs.y, z -= rs.z, i -= rs.i, j -= rs.j, k -= rs.k, u -= rs.u, v -= rs.v, w -= rs.w); return *this; }
  FI XYZval& operator*=(const XYZval  &rs)       { NUM_AXIS_CODE(x *= rs.x, y *= rs.y, z *= rs.z, i *= rs.i, j *= rs.j, k *= rs.k, u *= rs.u, v *= rs.v, w *= rs.w); return *this; }
  FI XYZval& operator/=(const XYZval  &rs)       { NUM_AXIS_CODE(x /= rs.x, y /= rs.y, z /= rs.z, i /= rs.i, j /= rs.j, k /= rs.k, u /= rs.u, v /= rs.v, w /= rs.w); return *this; }
  FI XYZval& operator+=(const XYZEval &rs)       { NUM_AXIS_CODE(x += rs.x, y += rs.y, z += rs.z, i += rs.i, j += rs.j, k += rs.k, u += rs.u, v += rs.v, w += rs.w); return *this; }
  FI XYZval& operator-=(const XYZEval &rs)       { NUM_AXIS_CODE(x -= rs.x, y -= rs.y, z -= rs.z, i -= rs.i, j -= rs.j, k -= rs.k, u -= rs.u, v -= rs.v, w -= rs.w); return *this; }
  FI XYZval& operator*=(const XYZEval &rs)       { NUM_AXIS_CODE(x *= rs.x, y *= rs.y, z *= rs.z, i *= rs.i, j *= rs.j, k *= rs.k, u *= rs.u, v *= rs.v, w *= rs.w); return *this; }
  FI XYZval& operator/=(const XYZEval &rs)       { NUM_AXIS_CODE(x /= rs.x, y /= rs.y, z /= rs.z, i /= rs.i, j /= rs.j, k /= rs.k, u /= rs.u, v /= rs.v, w /= rs.w); return *this; }
  FI XYZval& operator*=(const float &v)             { NUM_AXIS_CODE(x *= v,    y *= v,    z *= v,    i *= v,    j *= v,    k *= v,    u *= v,    v *= v,    w *= v);    return *this; }
  FI XYZval& operator*=(const double &v)  requires(std::is_same_v<T, double>)   { NUM_AXIS_CODE(x *= v,    y *= v,    z *= v,    i *= v,    j *= v,    k *= v,    u *= v,    v *= v,    w *= v);    return *this; }
  FI XYZval& operator*=(const int &v)               { NUM_AXIS_CODE(x *= v,    y *= v,    z *= v,    i *= v,    j *= v,    k *= v,    u *= v,    v *= v,    w *= v);    return *this; }
  FI XYZval& operator>>=(const int &v)              { NUM_AXIS_CODE(_RS(x),    _RS(y),    _RS(z),    _RS(i),    _RS(j),    _RS(k),    _RS(u),    _RS(v),    _RS(w));    return *this; }
  FI XYZval& operator<<=(const int &v)              { NUM_AXIS_CODE(_LS(x),    _LS(y),    _LS(z),    _LS(i),    _LS(j),    _LS(k),    _LS(u),    _LS(v),    _LS(w));    return *this; }

  // Exact comparisons. For floats a "NEAR" operation may be better.
  FI bool       operator==(const XYZval &rs) const { return true NUM_AXIS_GANG(&& x == rs.x, && y == rs.y, && z == rs.z, && i == rs.i, && j == rs.j, && k == rs.k, && u == rs.u, && v == rs.v, && w == rs.w); }
  FI bool       operator!=(const XYZval &rs) const { return !operator==(rs); }
};

//
// Logical Axes coordinates, counters, etc.
// Tag is used for creating strong type variants
//
template<typename T, typename Tag>
struct XYZEval {
  using XYval = ::XYval<T, Tag>;
  using XYZval = ::XYZval<T, Tag>;

  union {
    struct { T LOGICAL_AXIS_DECL(,0); };
    struct { T LOGICAL_AXIS_LIST(_e, a, b, c, _i, _j, _k, _u, _v, _w); };
    T pos[LOGICAL_AXES];
  };
  // Reset all to 0
  FI void reset()                     { LOGICAL_AXIS_GANG(e =, x =, y =, z =, i =, j =, k =, u =, v =, w =) 0; }

  // Setters for some number of linear axes, not all
  FI void set(const T px)                                                                                       { x = px; }
  FI void set(const T px, const T py)                                                                           { x = px; y = py; }
  #if HAS_I_AXIS
    FI void set(const T px, const T py, const T pz)                                                             { x = px; y = py; z = pz; }
  #endif
  #if HAS_J_AXIS
    FI void set(const T px, const T py, const T pz, const T pi)                                                 { x = px; y = py; z = pz; i = pi; }
  #endif
  #if HAS_K_AXIS
    FI void set(const T px, const T py, const T pz, const T pi, const T pj)                                     { x = px; y = py; z = pz; i = pi; j = pj; }
  #endif
  #if HAS_U_AXIS
    FI void set(const T px, const T py, const T pz, const T pi, const T pj, const T pk)                         { x = px; y = py; z = pz; i = pi; j = pj; k = pk; }
  #endif
  #if HAS_V_AXIS
    FI void set(const T px, const T py, const T pz, const T pi, const T pj, const T pk, const T pu)             { x = px; y = py; z = pz; i = pi; j = pj; k = pk; u = pu; }
  #endif
  #if HAS_W_AXIS
    FI void set(const T px, const T py, const T pz, const T pi, const T pj, const T pk, const T pu, const T pv) { x = px; y = py; z = pz; i = pi; j = pj; k = pk; u = pu; v = pv; }
  #endif

  // Setters taking struct types and arrays
  FI void set(const XYval pxy)                           { x = pxy.x; y = pxy.y; }
  FI void set(const XYZval pxyz)                         { set(NUM_AXIS_ELEM(pxyz)); }
  #if HAS_Z_AXIS
    FI void set(NUM_AXIS_ARGS(const T))                     { NUM_AXIS_CODE(a = x, b = y, c = z, _i = i, _j = j, _k = k, _u = u, _v = v, _w = w); }
  #endif
  FI void set(const XYval pxy, const T pz)               { set(pxy); TERN_(HAS_Z_AXIS, z = pz); }
  #if LOGICAL_AXES > NUM_AXES
    FI void set(const XYval pxy, const T pz, const T pe) { set(pxy, pz); e = pe; }
    FI void set(const XYZval pxyz, const T pe)           { set(pxyz); e = pe; }
    FI void set(LOGICAL_AXIS_ARGS(const T))                 { LOGICAL_AXIS_CODE(_e = e, a = x, b = y, c = z, _i = i, _j = j, _k = k, _u = u, _v = v, _w = w); }
  #endif

  // Length reduced to one dimension
  FI T magnitude()                                 const { return (T)sqrtf(LOGICAL_AXIS_GANG(+ e*e, + x*x, + y*y, + z*z, + i*i, + j*j, + k*k, + u*u, + v*v, + w*w)); }
  // Pointer to the data as a simple array
  FI operator T* ()                                      { return pos; }
  // If any element is true then it's true
  FI operator bool()                                     { return 0 LOGICAL_AXIS_GANG(|| e, || x, || y, || z, || i, || j, || k, || u, || v, || w); }

  // Explicit copy and copies with conversion
  FI XYZEval          copy()  const { XYZEval v = *this; return v; }
  FI XYZEval           ABS()  const { return LOGICAL_AXIS_ARRAY(T(_ABS(e)), T(_ABS(x)), T(_ABS(y)), T(_ABS(z)), T(_ABS(i)), T(_ABS(j)), T(_ABS(k)), T(_ABS(u)), T(_ABS(v)), T(_ABS(w))); }
  FI XYZEval<int16_t, Tag>   asInt()        { return LOGICAL_AXIS_ARRAY(int16_t(e), int16_t(x), int16_t(y), int16_t(z), int16_t(i), int16_t(j), int16_t(k), int16_t(u), int16_t(v), int16_t(w)); }
  FI XYZEval<int16_t, Tag>   asInt()  const { return LOGICAL_AXIS_ARRAY(int16_t(e), int16_t(x), int16_t(y), int16_t(z), int16_t(i), int16_t(j), int16_t(k), int16_t(u), int16_t(v), int16_t(w)); }
  FI XYZEval<int32_t, Tag>  asLong()        { return LOGICAL_AXIS_ARRAY(int32_t(e), int32_t(x), int32_t(y), int32_t(z), int32_t(i), int32_t(j), int32_t(k), int32_t(u), int32_t(v), int32_t(w)); }
  FI XYZEval<int32_t, Tag>  asLong()  const { return LOGICAL_AXIS_ARRAY(int32_t(e), int32_t(x), int32_t(y), int32_t(z), int32_t(i), int32_t(j), int32_t(k), int32_t(u), int32_t(v), int32_t(w)); }
  FI XYZEval<int32_t, Tag>  ROUNDL()        { return LOGICAL_AXIS_ARRAY(int32_t(LROUND(e)), int32_t(LROUND(x)), int32_t(LROUND(y)), int32_t(LROUND(z)), int32_t(LROUND(i)), int32_t(LROUND(j)), int32_t(LROUND(k)), int32_t(LROUND(u)), int32_t(LROUND(v)), int32_t(LROUND(w))); }
  FI XYZEval<int32_t, Tag>  ROUNDL()  const { return LOGICAL_AXIS_ARRAY(int32_t(LROUND(e)), int32_t(LROUND(x)), int32_t(LROUND(y)), int32_t(LROUND(z)), int32_t(LROUND(i)), int32_t(LROUND(j)), int32_t(LROUND(k)), int32_t(LROUND(u)), int32_t(LROUND(v)), int32_t(LROUND(w))); }
  FI XYZEval<float, Tag>   asFloat()        { return LOGICAL_AXIS_ARRAY(static_cast<float>(e), static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(i), static_cast<float>(j), static_cast<float>(k), static_cast<float>(u), static_cast<float>(v), static_cast<float>(w)); }
  FI XYZEval<float, Tag>   asFloat()  const { return LOGICAL_AXIS_ARRAY(static_cast<float>(e), static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(i), static_cast<float>(j), static_cast<float>(k), static_cast<float>(u), static_cast<float>(v), static_cast<float>(w)); }
  FI XYZEval<float, Tag> reciprocal() const { return LOGICAL_AXIS_ARRAY(_RECIP(e),  _RECIP(x),  _RECIP(y),  _RECIP(z),  _RECIP(i),  _RECIP(j),  _RECIP(k),  _RECIP(u),  _RECIP(v),  _RECIP(w)); }

  /// Basically a reinterpret_cast for the vectors. Only use when you know what you're doing
  template<typename NewTag>
  [[deprecated("UNSAFE. Only use when you know what you're doing")]]
  FI XYZEval<T, NewTag> to_tag()                    const { return LOGICAL_AXIS_ARRAY(e, x, y, z, i, j, k, u, v, w); }

  // Marlin workspace shifting is done with G92 and M206
  FI auto  asLogical() const requires(std::is_same_v<Tag, NativePosTag>) { return toLogical(*this); }
  FI auto   asNative() const requires(std::is_same_v<Tag, LogicalPosTag>) { return toNative(*this); }

  // In-place cast to types having fewer fields
  FI operator       XYval&()        { return *(XYval*)this; }
  FI operator const XYval&()  const { return *(const XYval*)this; }
  FI operator       XYZval&()       { return *(XYZval*)this; }
  FI operator const XYZval&() const { return *(const XYZval*)this; }

  // Accessor via an AxisEnum (or any integer) [index]
  FI       T&    operator[](const int n)                  { return pos[n]; }
  FI const T&    operator[](const int n)            const { return pos[n]; }

  // Assignment operator overrides do the expected thing
  FI XYZEval &operator=(const XYZEval &) = default;

  // Don't use these, they are dangerous. Always retype explicitly.
  FI XYZEval& operator= (const T v) = delete;
  FI XYZEval& operator= (const XYval   &rs) = delete;
  FI XYZEval& operator= (const XYZval  &rs) = delete;

  // Override other operators to get intuitive behaviors
  FI XYZEval  operator+ (const XYval   &rs)   const { XYZEval ls = *this; ls.x += rs.x; ls.y += rs.y; return ls; }
  FI XYZEval  operator- (const XYval   &rs)   const { XYZEval ls = *this; ls.x -= rs.x; ls.y -= rs.y; return ls; }
  FI XYZEval  operator* (const XYval   &rs)   const { XYZEval ls = *this; ls.x *= rs.x; ls.y *= rs.y; return ls; }
  FI XYZEval  operator/ (const XYval   &rs)   const { XYZEval ls = *this; ls.x /= rs.x; ls.y /= rs.y; return ls; }
  FI XYZEval  operator+ (const XYZval  &rs)   const { XYZEval  ls = *this; NUM_AXIS_CODE(ls.x += rs.x, ls.y += rs.y, ls.z += rs.z, ls.i += rs.i, ls.j += rs.j, ls.k += rs.k, ls.u += rs.u, ls.v += rs.v, ls.w += rs.w); return ls; }
  FI XYZEval  operator- (const XYZval  &rs)   const { XYZEval  ls = *this; NUM_AXIS_CODE(ls.x -= rs.x, ls.y -= rs.y, ls.z -= rs.z, ls.i -= rs.i, ls.j -= rs.j, ls.k -= rs.k, ls.u -= rs.u, ls.v -= rs.v, ls.w -= rs.w); return ls; }
  FI XYZEval  operator* (const XYZval  &rs)   const { XYZEval  ls = *this; NUM_AXIS_CODE(ls.x *= rs.x, ls.y *= rs.y, ls.z *= rs.z, ls.i *= rs.i, ls.j *= rs.j, ls.k *= rs.k, ls.u *= rs.u, ls.v *= rs.v, ls.w *= rs.w); return ls; }
  FI XYZEval  operator/ (const XYZval  &rs)   const { XYZEval  ls = *this; NUM_AXIS_CODE(ls.x /= rs.x, ls.y /= rs.y, ls.z /= rs.z, ls.i /= rs.i, ls.j /= rs.j, ls.k /= rs.k, ls.u /= rs.u, ls.v /= rs.v, ls.w /= rs.w); return ls; }
  FI XYZEval  operator+ (const XYZEval  &rs)  const { XYZEval ls = *this; LOGICAL_AXIS_CODE(ls.e += rs.e, ls.x += rs.x, ls.y += rs.y, ls.z += rs.z, ls.i += rs.i, ls.j += rs.j, ls.k += rs.k, ls.u += rs.u, ls.v += rs.v, ls.w += rs.w); return ls; }
  FI XYZEval  operator- (const XYZEval  &rs)  const { XYZEval ls = *this; LOGICAL_AXIS_CODE(ls.e -= rs.e, ls.x -= rs.x, ls.y -= rs.y, ls.z -= rs.z, ls.i -= rs.i, ls.j -= rs.j, ls.k -= rs.k, ls.u -= rs.u, ls.v -= rs.v, ls.w -= rs.w); return ls; }
  FI XYZEval  operator* (const XYZEval  &rs)  const { XYZEval ls = *this; LOGICAL_AXIS_CODE(ls.e *= rs.e, ls.x *= rs.x, ls.y *= rs.y, ls.z *= rs.z, ls.i *= rs.i, ls.j *= rs.j, ls.k *= rs.k, ls.u *= rs.u, ls.v *= rs.v, ls.w *= rs.w); return ls; }
  FI XYZEval  operator/ (const XYZEval  &rs)  const { XYZEval ls = *this; LOGICAL_AXIS_CODE(ls.e /= rs.e, ls.x /= rs.x, ls.y /= rs.y, ls.z /= rs.z, ls.i /= rs.i, ls.j /= rs.j, ls.k /= rs.k, ls.u /= rs.u, ls.v /= rs.v, ls.w /= rs.w); return ls; }
  FI XYZEval  operator* (const float &v)         const { XYZEval ls = *this; LOGICAL_AXIS_CODE(ls.e *= v,    ls.x *= v,    ls.y *= v,    ls.z *= v,    ls.i *= v,    ls.j *= v,    ls.k *= v,    ls.u *= v,    ls.v *= v,    ls.w *= v   ); return ls; }
  FI XYZEval  operator* (const double &v)        const { XYZEval ls = *this; LOGICAL_AXIS_CODE(ls.e *= v,    ls.x *= v,    ls.y *= v,    ls.z *= v,    ls.i *= v,    ls.j *= v,    ls.k *= v,    ls.u *= v,    ls.v *= v,    ls.w *= v   ); return ls; }
  FI XYZEval  operator* (const int &v)           const { XYZEval ls = *this; LOGICAL_AXIS_CODE(ls.e *= v,    ls.x *= v,    ls.y *= v,    ls.z *= v,    ls.i *= v,    ls.j *= v,    ls.k *= v,    ls.u *= v,    ls.v *= v,    ls.w *= v   ); return ls; }
  FI XYZEval  operator/ (const float &v)         const { XYZEval ls = *this; LOGICAL_AXIS_CODE(ls.e /= v,    ls.x /= v,    ls.y /= v,    ls.z /= v,    ls.i /= v,    ls.j /= v,    ls.k /= v,    ls.u /= v,    ls.v /= v,    ls.w /= v   ); return ls; }
  FI XYZEval  operator/ (const double &v)        const { XYZEval ls = *this; LOGICAL_AXIS_CODE(ls.e /= v,    ls.x /= v,    ls.y /= v,    ls.z /= v,    ls.i /= v,    ls.j /= v,    ls.k /= v,    ls.u /= v,    ls.v /= v,    ls.w /= v   ); return ls; }
  FI XYZEval  operator/ (const int &v)           const { XYZEval ls = *this; LOGICAL_AXIS_CODE(ls.e /= v,    ls.x /= v,    ls.y /= v,    ls.z /= v,    ls.i /= v,    ls.j /= v,    ls.k /= v,    ls.u /= v,    ls.v /= v,    ls.w /= v   ); return ls; }
  FI XYZEval  operator>>(const int &v)           const { XYZEval ls = *this; LOGICAL_AXIS_CODE(_RS(ls.e),    _RS(ls.x),    _RS(ls.y),    _RS(ls.z),    _RS(ls.i),    _RS(ls.j),    _RS(ls.k),    _RS(ls.u),    _RS(ls.v),    _RS(ls.w)   ); return ls; }
  FI XYZEval  operator<<(const int &v)           const { XYZEval ls = *this; LOGICAL_AXIS_CODE(_LS(ls.e),    _LS(ls.x),    _LS(ls.y),    _LS(ls.z),    _LS(ls.i),    _LS(ls.j),    _LS(ls.k),    _LS(ls.u),    _LS(ls.v),    _LS(ls.w)   ); return ls; }
  FI const XYZEval operator-()                   const { return LOGICAL_AXIS_ARRAY(-e, -x, -y, -z, -i, -j, -k, -u, -v, -w); }

  // Modifier operators
  FI XYZEval& operator+=(const XYval   &rs)         { x += rs.x; y += rs.y; return *this; }
  FI XYZEval& operator-=(const XYval   &rs)         { x -= rs.x; y -= rs.y; return *this; }
  FI XYZEval& operator*=(const XYval   &rs)         { x *= rs.x; y *= rs.y; return *this; }
  FI XYZEval& operator/=(const XYval   &rs)         { x /= rs.x; y /= rs.y; return *this; }
  FI XYZEval& operator+=(const XYZval  &rs)         { NUM_AXIS_CODE(x += rs.x, y += rs.y, z += rs.z, i += rs.i, j += rs.j, k += rs.k, u += rs.u, v += rs.v, w += rs.w); return *this; }
  FI XYZEval& operator-=(const XYZval  &rs)         { NUM_AXIS_CODE(x -= rs.x, y -= rs.y, z -= rs.z, i -= rs.i, j -= rs.j, k -= rs.k, u -= rs.u, v -= rs.v, w -= rs.w); return *this; }
  FI XYZEval& operator*=(const XYZval  &rs)         { NUM_AXIS_CODE(x *= rs.x, y *= rs.y, z *= rs.z, i *= rs.i, j *= rs.j, k *= rs.k, u *= rs.u, v *= rs.v, w *= rs.w); return *this; }
  FI XYZEval& operator/=(const XYZval  &rs)         { NUM_AXIS_CODE(x /= rs.x, y /= rs.y, z /= rs.z, i /= rs.i, j /= rs.j, k /= rs.k, u /= rs.u, v /= rs.v, w /= rs.w); return *this; }
  FI XYZEval& operator+=(const XYZEval &rs)         { LOGICAL_AXIS_CODE(e += rs.e, x += rs.x, y += rs.y, z += rs.z, i += rs.i, j += rs.j, k += rs.k, u += rs.u, v += rs.v, w += rs.w); return *this; }
  FI XYZEval& operator-=(const XYZEval &rs)         { LOGICAL_AXIS_CODE(e -= rs.e, x -= rs.x, y -= rs.y, z -= rs.z, i -= rs.i, j -= rs.j, k -= rs.k, u -= rs.u, v -= rs.v, w -= rs.w); return *this; }
  FI XYZEval& operator*=(const XYZEval &rs)         { LOGICAL_AXIS_CODE(e *= rs.e, x *= rs.x, y *= rs.y, z *= rs.z, i *= rs.i, j *= rs.j, k *= rs.k, u *= rs.u, v *= rs.v, w *= rs.w); return *this; }
  FI XYZEval& operator/=(const XYZEval &rs)         { LOGICAL_AXIS_CODE(e /= rs.e, x /= rs.x, y /= rs.y, z /= rs.z, i /= rs.i, j /= rs.j, k /= rs.k, u /= rs.u, v /= rs.v, w /= rs.w); return *this; }
  FI XYZEval& operator*=(const T &v)                   { LOGICAL_AXIS_CODE(e *= v,    x *= v,    y *= v,    z *= v,    i *= v,    j *= v,    k *= v,    u *= v,    v *= v,    w *= v);    return *this; }
  FI XYZEval& operator>>=(const int &v)                { LOGICAL_AXIS_CODE(_RS(e),    _RS(x),    _RS(y),    _RS(z),    _RS(i),    _RS(j),    _RS(k),    _RS(u),    _RS(v),    _RS(w));    return *this; }
  FI XYZEval& operator<<=(const int &v)                { LOGICAL_AXIS_CODE(_LS(e),    _LS(x),    _LS(y),    _LS(z),    _LS(i),    _LS(j),    _LS(k),    _LS(u),    _LS(v),    _LS(w));    return *this; }

  // Exact comparisons. For floats a "NEAR" operation may be better.
  FI bool        operator==(const XYZEval &rs)   const { return true LOGICAL_AXIS_GANG(&& e == rs.e, && x == rs.x, && y == rs.y, && z == rs.z, && i == rs.i, && j == rs.j, && k == rs.k, && u == rs.u, && v == rs.v, && w == rs.w); }
  FI bool        operator!=(const XYZEval &rs)   const { return !operator==(rs); }
};

#undef _RECIP
#undef _ABS
#undef _LS
#undef _RS
#undef FI

// TODO: remove or integrate these local/legacy exceptions
#define LOOP_XY(VAR) LOOP_S_LE_N(VAR, X_AXIS, Y_AXIS)
#define LOOP_XYZ(VAR) LOOP_S_LE_N(VAR, X_AXIS, Z_AXIS)
#define LOOP_XYZE(VAR) LOOP_S_LE_N(VAR, X_AXIS, E_AXIS)
#define LOOP_XYZE_N(VAR) LOOP_S_L_N(VAR, X_AXIS, XYZE_N)
#define LOOP_ABCE(VAR) LOOP_S_LE_N(VAR, A_AXIS, E_AXIS)

#pragma GCC diagnostic pop
