/**********************************************************************************************
 *
 *   raylib-extrss * Utilities and Shared Components for Raylib
 *
 *   raymath_operators.h * CPP operator overloads for raymath methods
 *   DO NOT USE THIS FILE WITH 5.5 DEV, it allready has operators built in
 *
 *   LICENSE: ZLiB
 *
 *   Copyright (c) 2022 Jeffery Myers
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a
 *copy of this software and associated documentation files (the "Software"), to
 *deal in the Software without restriction, including without limitation the
 *rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 *sell copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in
 *all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *IN THE SOFTWARE.
 *
 **********************************************************************************************/

#pragma once
#include "raylib.h"

//------------------RL_Vector2-----------------//
inline constexpr RL_Vector2 operator+( RL_Vector2 lhs, const RL_Vector2& rhs ) {
    return RL_Vector2{ lhs.x + rhs.x, lhs.y + rhs.y };
}

inline constexpr RL_Vector2 operator+( RL_Vector2 lhs, const float& rhs ) {
    return RL_Vector2{ lhs.x + rhs, lhs.y + rhs };
}
inline constexpr RL_Vector2& operator+=( RL_Vector2& lhs,
                                         const RL_Vector2& rhs ) {
    lhs = lhs + rhs;
    return lhs;
}
inline constexpr RL_Vector2& operator+=( RL_Vector2& lhs, const float& rhs ) {
    lhs = lhs + rhs;
    return lhs;
}

inline constexpr RL_Vector2 operator-( RL_Vector2 lhs, const RL_Vector2& rhs ) {
    return RL_Vector2{ lhs.x - rhs.x, lhs.y - rhs.y };
}

inline constexpr RL_Vector2 operator-( RL_Vector2 lhs, const float& rhs ) {
    return RL_Vector2{ lhs.x - rhs, lhs.y - rhs };
}
inline constexpr RL_Vector2& operator-=( RL_Vector2& lhs,
                                         const RL_Vector2& rhs ) {
    lhs = lhs - rhs;
    return lhs;
}
inline constexpr RL_Vector2& operator-=( RL_Vector2& lhs, const float& rhs ) {
    lhs = lhs - rhs;
    return lhs;
}

inline constexpr RL_Vector2 operator*( RL_Vector2 lhs, const float& rhs ) {
    return RL_Vector2{ lhs.x * rhs, lhs.y * rhs };
}

inline constexpr RL_Vector2 operator*( RL_Vector2 lhs, const RL_Vector2& rhs ) {
    return RL_Vector2{ lhs.x * rhs.x, lhs.y * rhs.y };
}
inline constexpr RL_Vector2 operator*=( RL_Vector2& lhs,
                                        const RL_Vector2& rhs ) {
    lhs = lhs * rhs;
    return lhs;
}
inline constexpr RL_Vector2& operator*=( RL_Vector2& lhs, const float& rhs ) {
    lhs = lhs * rhs;
    return lhs;
}

inline constexpr RL_Vector2 operator/( RL_Vector2 lhs, const float& rhs ) {
    return RL_Vector2{ lhs.x / rhs, lhs.y / rhs };
}

inline constexpr RL_Vector2 operator/( RL_Vector2 lhs, const RL_Vector2& rhs ) {
    return RL_Vector2{ lhs.x / rhs.y, lhs.y / rhs.y };
}
inline constexpr RL_Vector2 operator/=( RL_Vector2& lhs,
                                        const RL_Vector2& rhs ) {
    lhs = lhs / rhs;
    return lhs;
}
inline constexpr RL_Vector2& operator/=( RL_Vector2& lhs, const float& rhs ) {
    lhs = lhs / rhs;
    return lhs;
}

inline constexpr bool operator==( const RL_Vector2& lhs,
                                  const RL_Vector2& rhs ) {
    return ( lhs.x == rhs.x ) && ( lhs.y == rhs.y );
}
inline constexpr bool operator!=( const RL_Vector2& lhs,
                                  const RL_Vector2& rhs ) {
    return !( lhs == rhs );
}
inline constexpr bool operator<( const RL_Vector2& lhs,
                                 const RL_Vector2& rhs ) {
    return ( lhs.x < rhs.x ) || ( ( lhs.x == rhs.x ) && ( lhs.y < rhs.y ) );
}
inline constexpr bool operator<=( const RL_Vector2& lhs,
                                  const RL_Vector2& rhs ) {
    return ( lhs < rhs ) || ( lhs == rhs );
}
inline constexpr bool operator>( const RL_Vector2& lhs,
                                 const RL_Vector2& rhs ) {
    return rhs < lhs;
}
inline constexpr bool operator>=( const RL_Vector2& lhs,
                                  const RL_Vector2& rhs ) {
    return ( lhs > rhs ) || ( lhs == rhs );
}

static constexpr RL_Vector2 RL_Vector2Zeros = { 0, 0 };
static constexpr RL_Vector2 RL_Vector2UnitX = { 1, 0 };
static constexpr RL_Vector2 RL_Vector2UnitY = { 0, 1 };

//------------------RL_Vector3-----------------//
inline constexpr RL_Vector3 operator+( RL_Vector3 lhs, const RL_Vector3& rhs ) {
    return RL_Vector3{ lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

inline constexpr RL_Vector3 operator+( RL_Vector3 lhs, const float& rhs ) {
    return RL_Vector3{ lhs.x + rhs, lhs.y + rhs, lhs.z + rhs };
}
inline constexpr RL_Vector3& operator+=( RL_Vector3& lhs,
                                         const RL_Vector3& rhs ) {
    lhs = lhs + rhs;
    return lhs;
}
inline constexpr RL_Vector3& operator+=( RL_Vector3& lhs, const float& rhs ) {
    lhs = lhs + rhs;
    return lhs;
}

inline constexpr RL_Vector3 operator-( RL_Vector3 lhs, const RL_Vector3& rhs ) {
    return RL_Vector3{ lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

inline constexpr RL_Vector3 operator-( RL_Vector3 lhs, const float& rhs ) {
    return RL_Vector3{ lhs.x - rhs, lhs.y - rhs, lhs.z - rhs };
}
inline constexpr RL_Vector3& operator-=( RL_Vector3& lhs,
                                         const RL_Vector3& rhs ) {
    lhs = lhs - rhs;
    return lhs;
}
inline constexpr RL_Vector3& operator-=( RL_Vector3& lhs, const float& rhs ) {
    lhs = lhs - rhs;
    return lhs;
}

inline constexpr RL_Vector3 operator*( RL_Vector3 lhs, const float& rhs ) {
    return RL_Vector3{ lhs.x * rhs, lhs.y * rhs, lhs.z * rhs };
}

inline constexpr RL_Vector3 operator*( RL_Vector3 lhs, const RL_Vector3& rhs ) {
    return RL_Vector3{ lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z };
}
inline constexpr RL_Vector3 operator*=( RL_Vector3& lhs,
                                        const RL_Vector3& rhs ) {
    lhs = lhs * rhs;
    return lhs;
}
inline constexpr RL_Vector3& operator*=( RL_Vector3& lhs, const float& rhs ) {
    lhs = lhs * rhs;
    return lhs;
}
inline constexpr RL_Vector3 operator/( RL_Vector3 lhs, const RL_Vector3& rhs ) {
    return RL_Vector3{ lhs.x / rhs.y, lhs.y / rhs.y, lhs.z / rhs.z };
}

inline constexpr RL_Vector3 operator/( RL_Vector3 lhs, const float& rhs ) {
    return RL_Vector3{ lhs.x / rhs, lhs.y / rhs, lhs.z / rhs };
}
inline constexpr RL_Vector3 operator/=( RL_Vector3& lhs,
                                        const RL_Vector3& rhs ) {
    lhs = lhs / rhs;
    return lhs;
}
inline constexpr RL_Vector3& operator/=( RL_Vector3& lhs, const float& rhs ) {
    lhs = lhs / rhs;
    return lhs;
}

inline constexpr bool operator==( const RL_Vector3& lhs,
                                  const RL_Vector3& rhs ) {
    return ( lhs.x == rhs.x ) && ( lhs.y == rhs.y ) && ( lhs.z == rhs.z );
}
inline constexpr bool operator!=( const RL_Vector3& lhs,
                                  const RL_Vector3& rhs ) {
    return !( lhs == rhs );
}
inline constexpr bool operator<( const RL_Vector3& lhs,
                                 const RL_Vector3& rhs ) {
    return ( lhs.x < rhs.x ) || ( ( lhs.x == rhs.x ) && ( lhs.y < rhs.y ) ) ||
           ( ( lhs.x == rhs.x ) && ( lhs.y == rhs.y ) && ( lhs.z < rhs.z ) );
}
inline constexpr bool operator<=( const RL_Vector3& lhs,
                                  const RL_Vector3& rhs ) {
    return ( lhs < rhs ) || ( lhs == rhs );
}
inline constexpr bool operator>( const RL_Vector3& lhs,
                                 const RL_Vector3& rhs ) {
    return rhs < lhs;
}
inline constexpr bool operator>=( const RL_Vector3& lhs,
                                  const RL_Vector3& rhs ) {
    return ( lhs > rhs ) || ( lhs == rhs );
}

static constexpr RL_Vector3 RL_Vector3Zeros = { 0, 0, 0 };
static constexpr RL_Vector3 RL_Vector3UnitX = { 1, 0, 0 };
static constexpr RL_Vector3 RL_Vector3UnitY = { 0, 1, 0 };
static constexpr RL_Vector3 RL_Vector3UnitZ = { 0, 0, 1 };
