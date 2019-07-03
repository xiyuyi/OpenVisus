/*-----------------------------------------------------------------------------
Copyright(c) 2010 - 2018 ViSUS L.L.C.,
Scientific Computing and Imaging Institute of the University of Utah

ViSUS L.L.C., 50 W.Broadway, Ste. 300, 84101 - 2044 Salt Lake City, UT
University of Utah, 72 S Central Campus Dr, Room 3750, 84112 Salt Lake City, UT

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THEg
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

For additional information about this project contact : pascucci@acm.org
For support : support@visus.net
-----------------------------------------------------------------------------*/

#ifndef VISUS_POINT_H
#define VISUS_POINT_H

#include <Visus/Kernel.h>
#include <Visus/Utils.h>

#include <array>
#include <algorithm>
#include <type_traits>

namespace Visus {

////////////////////////////////////////////////////////////////////////
template <typename __T__>
class Point2
{
public:

  typedef __T__ T;

  T x = 0, y = 0;

  //default constructor
  Point2() {
  }

  //constructor
  explicit Point2(T a, T b) : x(a),y(b) {
  }

  //constructor
  explicit Point2(const std::vector<T>& src) : Point2(src[0], src[1]) {
    VisusAssert(src.size() == 2);
  }

  //constructor from string
  explicit Point2(String value) {
    std::istringstream parser(value); parser >> x >> y;
  }

  //parseFromString
  static Point2 parseFromString(String value) {
    return Point2(value);
  }

  //parseFromString
  static Point2 one(int pdim) {
    VisusAssert(pdim == 2);
    return Point2(1, 1);
  }

  //castTo
  template <typename Other>
  Other castTo() const {
    return Other((typename Other::T)get(0), (typename Other::T)get(1));
  }

  //toVector
  std::vector<T> toVector() const {
    return std::vector<T>({ x,y });
  }

  //back
  T back() const {
    return y;
  }

#if !SWIG
  //back
  T& back() {
    return y;
  }
#endif

  //module*module
  T module2() const {
    return x*x + y*y;
  }

  //module
  double module() const {
    return std::sqrt((double)module2());
  }

  //distance between two points
  double distance(const Point2& p) const {
    return (p - *this).module();
  }

  //normalize a vector
  Point2 normalized() const
  {
    T len = module();
    if (!len) len = 1.0;
    return Point2(x / len, y / len);
  }

  //abs
  Point2 abs() const {
    return Point2(x >= 0 ? +x : -x, y >= 0 ? +y : -y);
  }

  //inv
  Point2 inv() const {
    return Point2((T)(1.0 / x), (T)(1.0 / y));
  }

  //-a
  Point2 operator-()  const {
    return Point2(-this->x, -this->y);
  }

  //a+b
  Point2 operator+(const Point2&  b)  const {
    return Point2(this->x + b.x, this->y + b.y);
  }

  //a+=b
  Point2& operator+=(const Point2&  b) {
    this->x += b.x; this->y += b.y; return *this;
  }

  //a-b
  Point2 operator-(const Point2&  b)  const {
    return Point2(this->x - b.x, this->y - b.y);
  }

  //a-=b
  Point2& operator-=(const Point2&  b) {
    this->x -= b.x; this->y -= b.y; return *this;
  }

  //a*f
  template <typename Value>
  Point2 operator*(Value value) const {
    return Point2((T)(this->x*value), (T)(this->y*value));
  }

  //a*=f
  Point2& operator*=(T s) {
    this->x = this->x*s; this->y = this->y*s; return *this;
  }

  //a==b
  bool operator==(const Point2& b) const {
    return  x == b.x && y == b.y;
  }

  //a!=b
  bool operator!=(const Point2& b) const {
    return !(operator==(b));
  }

  //dot product
  T dot(const Point2&  b) const {
    return this->x*b.x + this->y*b.y;
  }

  //access an item using an index
  T& get(int i) {
    VisusAssert(((&x) + 1) == &y);
    return (&x)[i];
  }

  //access an item using an index
  const T& get(int i) const {
    VisusAssert(((&x) + 1) == &y);
    return (&x)[i];
  }

  //access an item using an index
  T& operator[](int i) {
    return get(i);
  }

  //access an item using an index
  const T& operator[](int i) const {
    return get(i);
  }

  //set
  Point2& set(int i,T value){
    get(i)=value; return *this;
  }

  //test if numers are ok
  bool valid() const
  {
    return
      Utils::isValidNumber(x) &&
      Utils::isValidNumber(y);
  }

  // return index of smallest/largest value
  int biggest () const { return (x >  y) ? (0) : (1); }
  int smallest() const { return (x <= y) ? (0) : (1); }

  //innerMultiply
  inline Point2 innerMultiply(const Point2& other) const {
    return Point2(x *other.x, y*other.y);
  }

  //innerDiv
  inline Point2 innerDiv(const Point2& other) const {
    return Point2(x / other.x, y / other.y);
  }

public:

  //min,max 
  static Point2 min(const Point2& a, const Point2& b) { return Point2(std::min(a[0], b[0]), std::min(a[1], b[1])); }
  static Point2 max(const Point2& a, const Point2& b) { return Point2(std::max(a[0], b[0]), std::max(a[1], b[1])); }

  //operator (<,<=,>,>=)
  bool operator< (const Point2& b) const { const Point2& a = *this; return a[0] <  b[0] && a[1] <  b[1]; }
  bool operator<=(const Point2& b) const { const Point2& a = *this; return a[0] <= b[0] && a[1] <= b[1]; }
  bool operator> (const Point2& b) const { const Point2& a = *this; return a[0] >  b[0] && a[1] >  b[1]; }
  bool operator>=(const Point2& b) const { const Point2& a = *this; return a[0] >= b[0] && a[1] >= b[1]; }


public:

  //convert to string
  String toString() const
  {
    std::ostringstream out;
    out << this->x << " " << this->y;
    return out.str();
  }

  //operator<<
#if !SWIG
  friend std::ostream& operator<<(std::ostream &out, const Point2& p) {
    out << "<" << p.x << "," << p.y << ">"; return out;
  }
#endif

};//end class Point2


template <typename Value, typename T>
inline Point2<T> operator*(Value value, const Point2<T>& v) {
  return v * value;
}

typedef  Point2<Int64>  Point2i;
typedef  Point2<float>  Point2f;
typedef  Point2<double> Point2d;

////////////////////////////////////////////////////////////////////////
template <typename __T__>
class Point3
{
public:

  typedef __T__ T;

  T x = 0, y = 0, z = 0;

  //default constructor
  Point3() {
  }

  //constructor
  explicit Point3(T a, T b, T c = T(0)) :x(a), y(b), z(c) {
  }

  //constructor
  explicit Point3(const Point2<T>& src, T d = T(0)) : Point3(src.x, src.y,d) {
  }

  //constructor
  explicit Point3(const std::vector<T>& src) : Point3(src[0],src[1],src[2]) {
    VisusAssert(src.size() == 3);
  }

  //constructor from string
  explicit Point3(String value) {
    std::istringstream parser(value); parser >> x >> y >> z;
  }

  //parseFromString
  static Point3 parseFromString(String value) {
    return Point3(value);
  }

  //parseFromString
  static Point3 one(int pdim) {
    VisusAssert(pdim == 3);
    return Point3(1, 1, 1);
  }

  //castTo
  template <typename Other>
  Other castTo() const {
    return Other((typename Other::T)get(0), (typename Other::T)get(1), (typename Other::T)get(2));
  }

  //toVector
  std::vector<T> toVector() const {
    return std::vector<T>({ x,y,z });
  }

  //back
  T back() const {
    return z;
  }

#if !SWIG
  //back
  T& back() {
    return z;
  }
#endif

  //toPoint2
  Point2<T> toPoint2() const {
    return Point2<T>(x, y);
  }

  //dropHomogeneousCoordinate
  Point2<T> dropHomogeneousCoordinate() const {
    return Point2<T>(x / (z? z: 1.0), y / (z ? z : 1.0));
  }

  //module*module
  T module2() const {
    return x*x + y*y + z*z;
  }

  //module
  T module() const {
    return (T)sqrt(module2());
  }

  //distance between two points
  T distance(const Point3& p) const {
    return (p - *this).module();
  }

  //normalize a vector
  Point3 normalized() const
  {
    T len = module();
    if (len==T(0)) len = T(1);
    return Point3((T)(x / len), (T)(y / len), (T)(z / len));
  }

  //abs
  Point3 abs() const {
    return Point3(x >= 0 ? +x : -x, y >= 0 ? +y : -y, z >= 0 ? +z : -z);
  }

  //inverse
  Point3 inv() const {
    return Point3((T)(1.0 / x), (T)(1.0 / y), (T)(1.0 / z));
  }

  //+a
  const Point3& operator+()  const {
    return *this;
  }

  //-a
  Point3 operator-()  const {
    return Point3(-this->x, -this->y, -this->z);
  }

  //a+b
  Point3 operator+(const Point3&  b)  const {
    return Point3(this->x + b.x, this->y + b.y, this->z + b.z);
  }

  //a+=b
  Point3& operator+=(const Point3&  b) {
    this->x += b.x; this->y += b.y; this->z += b.z; return *this;
  }

  //a-b
  Point3 operator-(const Point3&  b)  const {
    return Point3(this->x - b.x, this->y - b.y, this->z - b.z);
  }

  //a-=b
  Point3& operator-=(const Point3&  b) {
    this->x -= b.x; this->y -= b.y; this->z -= b.z; return *this;
  }

  //a*f
  template <typename Coeff>
  Point3 operator*(Coeff coeff) const {
    return Point3((T)(this->x* coeff), (T)(this->y* coeff), (T)(this->z*coeff));
  }

  //a*=f
  template <typename Value>
  Point3& operator*=(Value value) {
    this->x = this->x*value; this->y = this->y*value; this->z = this->z*value; return *this;
  }

  //a==b
  bool operator==(const Point3& b) const {
    return  x == b.x && y == b.y && z == b.z;
  }

  //a!=b
  bool operator!=(const Point3& b) const {
    return  !(operator==(b));
  }

  //dot product
  T dot(const Point3&  b) const {
    return this->x*b.x + this->y*b.y + this->z*b.z;
  }

  //access an item using an index
  T& get(int i) {
    VisusAssert(((&x) + 1) == &y);
    return (&x)[i];
  }

  //access an item using an index
  const T& get(int i) const {
    VisusAssert(((&x) + 1) == &y);
    return (&x)[i];
  }

  //access an item using an index
  T& operator[](int i) {
    return get(i);
  }

  //access an item using an index
  const T& operator[](int i) const {
    return get(i);
  }

  //set
  Point3& set(int i,T value){
    get(i)=value; return *this;
  }

  //cross product
  Point3 cross(const Point3& v) const {
    return Point3 (
      y * v.z - v.y * z,
      z * v.x - v.z * x,
      x * v.y - v.x * y
    );
  }

  //test if numers are ok
  bool valid() const
  {
    return
      Utils::isValidNumber(x) &&
      Utils::isValidNumber(y) &&
      Utils::isValidNumber(z);
  }

  // return index of smallest/largest value
  int biggest () const { return (x >  y) ? (x >  z ? 0 : 2) : (y >  z ? 1 : 2); }
  int smallest() const { return (x <= y) ? (x <= z ? 0 : 2) : (y <= z ? 1 : 2); }

  //min,max 
  static Point3 min(const Point3& a, const Point3& b) { return Point3(std::min(a[0], b[0]), std::min(a[1], b[1]), std::min(a[2], b[2])); }
  static Point3 max(const Point3& a, const Point3& b) { return Point3(std::max(a[0], b[0]), std::max(a[1], b[1]), std::max(a[2], b[2])); }

  //operator (<,<=,>,>=)
  bool operator< (const Point3& b) const { const Point3& a = *this; return a[0] <  b[0] && a[1] <  b[1] && a[2] <  b[2]; }
  bool operator<=(const Point3& b) const { const Point3& a = *this; return a[0] <= b[0] && a[1] <= b[1] && a[2] <= b[2]; }
  bool operator> (const Point3& b) const { const Point3& a = *this; return a[0] >  b[0] && a[1] >  b[1] && a[2] >  b[2]; }
  bool operator>=(const Point3& b) const { const Point3& a = *this; return a[0] >= b[0] && a[1] >= b[1] && a[2] >= b[2]; }


public:

  //convert to string
  String toString() const
  {
    std::ostringstream out;
    out << this->x << " " << this->y << " " << this->z;
    return out.str();
  }


#if !SWIG
  //operator<<
  friend std::ostream& operator<<(std::ostream &out,const Point3& p) {
    out << "<" << p.x << "," << p.y << "," << p.z << ">"; return out;
  }
#endif

};//end class Point3


template <typename Coeff, typename T>
inline Point3<T> operator*(Coeff coeff, const Point3<T>& v) {
  return v * coeff;
}

typedef  Point3<Int64>  Point3i;
typedef  Point3<float>  Point3f;
typedef  Point3<double> Point3d;


///////////////////////////////////////////////////////////////////////////
template <typename __T__>
class Point4
{
public:

  typedef __T__ T;

  T x = 0, y = 0, z = 0, w = 0;

  //default constructor
  Point4() {
  }

  //constructor
  explicit Point4(T a, T b, T c = 0, T d = 0)
    : x(a), y(b), z(c), w(d) {
  }

  //constructor
  explicit Point4(const Point3<T>& src, T d = 0)
    : Point4(src.x, src.y, src.z, d) {
  }

  //constructor
  explicit Point4(const std::vector<T> src) : Point4(src[0],src[1],src[2],src[3]) {
    VisusAssert(src.size() == 4);
  }

  //constructor from string
  explicit Point4(String value) {
    std::istringstream parser(value); parser >> x >> y >> z >> w;
  }

  //parseFromString
  static Point4 parseFromString(String value) {
    return Point4(value);
  }

  //parseFromString
  static Point4 one(int pdim) {
    VisusAssert(pdim == 4);
    return Point4(1, 1, 1, 1);
  }

  //castTo
  template <typename Other>
  Other castTo() const {
    return Other((typename Other::T)get(0), (typename Other::T)get(1), (typename Other::T)get(2), (typename Other::T)get(3));
  }

  //toVector
  std::vector<T> toVector() const {
    return std::vector<T>({ x,y,z,w });
  }

  //back
  T back() const {
    return w;
  }

#if !SWIG
  //back
  T& back() {
    return w;
  }
#endif

  //toPoint3
  Point3<T> toPoint3() const {
    return Point3<T>(x, y, z);
  }

  //dropHomogeneousCoordinate
  Point3<T> dropHomogeneousCoordinate() const
  {
    T W = this->w;
    if (!W) W = 1;
    return Point3<T>(x / W, y / W, z / W);
  }

  //module2
  T module2() const {
    return x*x + y*y + z*z + w*w;
  }

  //module
  T module() const {
    return (T)sqrt((T)module2());
  }

  //distance between two points
  T distance(const Point4& p) const {
    return (p - *this).module();
  }

  //normalized
  Point4 normalized() const
  {
    T len = module();
    if (len==T(0)) return *this;
    auto vs = T(1) / len;
    return Point4(x * vs, y * vs, z * vs, w * vs);
  }

  //abs
  Point4 abs() const {
    return Point4(x >= 0 ? +x : -x, y >= 0 ? +y : -y, z >= 0 ? +z : -z, w >= 0 ? +w : -w);
  }

  //inverse
  Point4 inv() const {
    return Point4((T)(1.0 / x), T(1.0 / y), (T)(1.0 / z), (T)(1.0 / w));
  }




  //operator-
  Point4 operator-()  const {
    return Point4(-x, -y, -z, -w);
  }

  //a+b
  Point4 operator+(const Point4& b)  const {
    return Point4(x + b.x, y + b.y, z + b.z, w + b.w);
  }

  //a+=b
  Point4& operator+=(const Point4&  b) {
    x += b.x; y += b.y; z += b.z; w += b.w; return *this;
  }

  //a-b
  Point4 operator-(const Point4&  b)  const{
    return Point4(x - b.x, y - b.y, z - b.z, w - b.w);
  }

  //a-=b
  Point4& operator-=(const Point4&  b) {
    x -= b.x; y -= b.y; z -= b.z; w -= b.w; return *this;
  }

  //a*f
  template <typename Value>
  Point4 operator*(Value value) const {
    return Point4(x*value, y*value, z*value, w*value);
  }

  //a*=f
  template <typename Value>
  Point4& operator*=(Value value) {
    return (*this=*this * value);
  }

  //a==b
  bool operator==(const Point4& b) const {
    return  x == b.x && y == b.y && z == b.z && w == b.w;
  }

  //a!=b
  bool operator!=(const Point4& b) const {
    return  !(operator==(b));
  }

  //dot product
  T dot(const Point4&  b) const {
    return x*b.x + y*b.y + z*b.z + w*b.w;
  }

  //access an item using an index
  T& get(int i) {
    VisusAssert(((&x) + 1) == &y);
    return (&x)[i];
  }

  //access an item using an index
  const T& get(int i) const {
    VisusAssert(((&x) + 1) == &y);
    return (&x)[i];
  }

  //access an item using an index
  T& operator[](int i) {
    return get(i);
  }

  //access an item using an index
  const T& operator[](int i) const {
    return get(i);
  }

  //set
  Point4& set(int i,T value){
    get(i)=value; return *this;
  }

  //test if numers are ok
  bool valid() const
  {
    return
      Utils::isValidNumber(x) &&
      Utils::isValidNumber(y) &&
      Utils::isValidNumber(z) &&
      Utils::isValidNumber(w);
  }

public:

  //convert to string
  String toString() const
  {
    std::ostringstream out;
    out << this->x << " " << this->y << " " << this->z << " " << this->w;
    return out.str();
  }


#if !SWIG
  //operator<<
  friend std::ostream& operator<<(std::ostream &out,const Point4& p)  {
    out << "<" << p.x << "," << p.y << "," << p.z << "," << p.w << ">"; return out;
  }
#endif

};//end class Point4


template <typename Value, typename T>
inline Point4<T> operator*(Value value, const Point4<T>& v) {
  return v * value;
}

typedef  Point4<Int64>  Point4i;
typedef  Point4<float>  Point4f;
typedef  Point4<double> Point4d;


//////////////////////////////////////////////////////////////
template <typename __T__>
class PointN
{
public:

  typedef __T__ T;

  //_____________________________________________________
  struct Compare
  {
    bool operator()(const PointN& a, const  PointN& b) const {
      return a.coords < b.coords;
    }
  };

  //_____________________________________________________
  class ForEachPoint
  {
  public:

    PointN pos;
    PointN from, to, step;

    int    pdim = 0;
    bool   bEnd = true;

    //constructor
    ForEachPoint(PointN from_, PointN to_, PointN step_) : pos(from_), from(from_), to(to_), step(step_), pdim(from_.getPointDim())
    {
      VisusAssert(pdim == from.getPointDim());
      VisusAssert(pdim == to.getPointDim());
      VisusAssert(pdim == step.getPointDim());
      VisusAssert(pdim == pos.getPointDim());

      if (!pdim)
        return;

      for (int D = 0; D < pdim; D++) {
        if (!(pos[D] >= from[D] && pos[D] < to[D]))
          return;
      }

      this->bEnd = false;
    }

    //end
    bool end() const {
      return bEnd;
    }

    //next
    void next()
    {
      if (!bEnd && ((pos[0] += step[0]) >= to[0]))
      {
        pos[0] = from[0];
        for (int D = 1; D < pdim; D++)
        {
          if ((pos[D] += step[D]) < to[D])
            return;
          pos[D] = from[D];
        }
        bEnd = true;
      }
    }
  };

  std::vector<T> coords;

  //default constructor
  PointN() {
  }

  //constructor
  explicit PointN(const std::vector<T>& coords_)
    : coords(coords_) {
  }

  //constructor
  explicit PointN(int pdim)
    : coords(std::vector<T>(pdim, T(0))) {
  }

  //constructor
  explicit PointN(const PointN& left, T right) :
    coords(left.coords) {
    this->coords.push_back(right);
  }

  //constructor
  explicit PointN(T a, T b)
    : coords({ a,b }) {
  }

  //constructor
  explicit PointN(T a, T b, T c)
    : coords({ a,b,c }) {
  }

  //constructor
  explicit PointN(T a, T b, T c,T d)
    : coords({ a,b,c,d }) {
  }

  //constructor
  explicit PointN(T a, T b, T c, T d,T e)
    : coords({ a,b,c,d,e }) {
  }

  //constructor
  PointN(Point2<T> p) : PointN(p.toVector()) {
  }

  //constructor
  PointN(Point3<T> p) : PointN(p.toVector()) {
  }

  //constructor
  PointN(Point4<T> p) : PointN(p.toVector()) {
  }

  //getPointDim
  int getPointDim() const {
    return (int)coords.size();
  }

  //push_back
  void push_back(T value) {
    this->coords.push_back(value);
  }

  //pop_back
  void pop_back() {
    this->coords.pop_back();
  }

  //withoutBack
  PointN withoutBack() const {
    auto ret = *this;
    ret.pop_back();
    return ret;
  }

  //back
  T back() const {
    return this->coords.back();
  }

  //back
#if !SWIG
  T& back() {
    return this->coords.back();
  }
#endif

  //dropHomogeneousCoordinate
  PointN dropHomogeneousCoordinate() const {
    return applyOperation(*this, MulByCoeff<double>(1.0 / back())).withoutBack();
  }

  //castTo
  template <typename Other>
  Other castTo() const {
    auto pdim = (int)coords.size();
    auto ret = Other(pdim);
    for (int I = 0; I < pdim; I++)
      ret[I] = (typename Other::T)get(I);
    return ret;
  }

  //setPointDim
  void setPointDim(int new_pdim, T default_value = 0.0) {
    this->coords.resize(new_pdim, default_value);
  }

  //constructor
  static PointN one(int pdim) {
    return PointN(std::vector<T>(pdim, T(1)));
  }

  //toVector
  const std::vector<T>& toVector() const {
    return coords;
  }

  //test if numers are ok
  bool valid() const {
    return checkAll<ConditionValidNumber>(*this);
  }

  //get
  T& get(int i) {
    return coords[i];
  }

  //const
  const T& get(int i) const {
    return coords[i];
  }

  //operator[]
  const T& operator[](int i) const {
    return get(i);
  }

  //operator[]
  T& operator[](int i) {
    return get(i);
  }

  //set
  PointN& set(int i,T value){
    get(i)=value; return *this;
  }

  //operator-
  PointN operator-()  const {
    return applyOperation<NegOp>(*this);
  }

  //a+b
  PointN operator+(const PointN& other)  const {
    return applyOperation<AddOp>(*this, other);
  }

  //a-b
  PointN operator-(const PointN& other)  const {
    return applyOperation<SubOp>(*this, other);
  }

  //a*s
  template <typename Coeff>
  PointN operator*(Coeff coeff) const {
    return applyOperation(*this, MulByCoeff<Coeff>(coeff));
  }

  //a+=b
  PointN& operator+=(const PointN& other) {
    return ((*this) = (*this) + other);
  }

  //a-=b
  PointN& operator-=(const PointN& other) {
    return ((*this) = (*this) - other);
  }

  //a*=s
  PointN& operator*=(T s) {
    return ((*this) = (*this) * s);
  }

  //a==b
  bool operator==(const PointN& other) const {
    return coords == other.coords;;
  }

  //a!=b
  bool operator!=(const PointN& other) const {
    return coords != other.coords;;
  }

  //min
  static PointN min(const PointN& a, const PointN& b) {
    return applyOperation<MinOp>(a, b);
  }

  //max
  static PointN max(const PointN& a, const PointN& b) {
    return applyOperation< MaxOp>(a, b);
  }

  //module2
  T module2() const {
    return this->dot(*this);
  }

  //module
  double module() const {
    return std::sqrt((double)module2());
  }

  //distance between two points
  double distance(const PointN& p) const {
    return (p - *this).module();
  }

  //normalized
  PointN normalized() const
  {
    T len = module();
    if (!len) len = 1.0;
    return (*this) * (1.0/len);
  }

  //abs
  PointN abs() const {
    return applyOperation<AbsOp>(*this);
  }

  //inv
  PointN inv() const {
    return applyOperation<InvOp>(*this);  
  }

  //minsize
  T minsize() const {
    return Utils::min(this->coords);
  }

  //maxsize
  T maxsize() const {
    return Utils::max(this->coords);
  }

  bool checkAllLess        (const PointN& a, const PointN& b) const { return checkAll< ConditionL  >(a, b); }
  bool checkAllLessEqual   (const PointN& a, const PointN& b) const { return checkAll< ConditionLE >(a, b); }
  bool checkAllGreater     (const PointN& a, const PointN& b) const { return checkAll< ConditionG  >(a, b); }
  bool checkAllGreaterEqual(const PointN& a, const PointN& b) const { return checkAll< ConditionGE >(a, b); }

  //operator (<,<=,>,>=) (NOTE: it's different from lexigraphical order)
  bool operator< (const PointN& b) const { return checkAllLess        (*this, b); }
  bool operator<=(const PointN& b) const { return checkAllLessEqual   (*this, b); }
  bool operator> (const PointN& b) const { return checkAllGreater     (*this, b); }
  bool operator>=(const PointN& b) const { return checkAllGreaterEqual(*this, b); }

public:

  //dot product
  T dot(const PointN& other) const {
    return accumulateOperation<AddOp>(T(0), this->innerMultiply(other));
  }

  //dotProduct
  T dotProduct(const PointN& other) const {
    return dot(other);
  }

  //stride 
  PointN stride() const
  {
    auto pdim = (int)coords.size();
    auto ret = PointN(pdim);
    ret[0] = 1;
    for (int I = 0; I < pdim - 1; I++)
      ret[I + 1] = ret[I] * get(I);
    return ret;
  }

  //innerMultiply
  PointN innerMultiply(const PointN& other) const {
    return applyOperation<MulOp>(*this, other);
  }

  //innerDiv
  PointN innerDiv(const PointN& other) const {
    return applyOperation<DivOp>(*this, other);
  }

  //innerProduct 
  T innerProduct() const
  {
    auto pdim = getPointDim();
    if (pdim == 0) 
      return 0;

    //check overflow
#ifdef VISUS_DEBUG
    T __acc__ = 1;
    for (int I = 0; I < pdim; I++)
      if (!Utils::safe_mul(__acc__, __acc__, coords[I]))
        VisusAssert(false);
#endif

    return accumulateOperation<MulOp>(1, *this);
  }

  //innerMod
#if !SWIG

  //leftShift
  template <typename = std::enable_if<std::is_integral<T>::value > >
  PointN leftShift(const T & value) const {
    return applyOperation(*this, LShiftByValue(value));
  }

  //rightShift
  template <typename = std::enable_if<std::is_integral<T>::value > >
  PointN rightShift(const T & value) const {
    return applyOperation(*this, RShiftByValue(value));
  }

  //leftShift
  template <typename = std::enable_if<std::is_integral<T>::value > >
  PointN leftShift(const PointN & value) const {
    return applyOperation<LShiftOp>(*this, value);
  }

  //rightShift
  template <typename = std::enable_if<std::is_integral<T>::value > >
  PointN rightShift(const PointN & value) const {
    return applyOperation<RShiftOp>(*this, value);
  }

  //getLog2
  template <typename = std::enable_if<std::is_integral<T>::value > >
  PointN getLog2() const {
    return applyOperation<Log2Op>(*this);
  }

  template <typename = std::enable_if<std::is_integral<T>::value > >
  PointN innerMod(const PointN & other) const {
    return applyOperation<ModOp>(*this, other);
  }
#endif

public:

  //toPoint2
  Point2<T> toPoint2() const {
    auto coords = this->coords;
    coords.resize(2);
    return Point2<T>(coords[0], coords[1]);
  }

  //toPoint3
  Point3<T> toPoint3() const {
    auto coords = this->coords;
    coords.resize(3);
    return Point3<T>(coords[0], coords[1], coords[2]);
  }

  //toPoint4
  Point4<T> toPoint4() const {
    auto coords = this->coords;
    coords.resize(4);
    return Point4<T>(coords[0], coords[1], coords[2],coords[3]);
  }

public:

  //parseFromString
  static PointN parseFromString(String src)
  {
    std::vector<T> ret;
    std::istringstream parser(src);
    T parsed; while (parser >> parsed)
      ret.push_back(parsed);
    return PointN(ret);
  }

  //parseDims
  static PointN parseDims(String src)
  {
    auto ret = parseFromString(src);

    //backward compatible: remove unnecessary dimensions
    while (ret.getPointDim() && ret.back() == T(1))
      ret.pop_back();

    return ret;
  }

  //convert to string
  String toString(String sep = " ") const {
    auto pdim = (int)coords.size();
    std::ostringstream out;
    for (int I = 0; I < pdim; I++)
      out << (I ? sep : "") << get(I);
    return out.str();
  }

#if !SWIG
  //operator<<
  friend std::ostream& operator<<(std::ostream& out, const PointN& p) {
    out << "<" << p.toString(",") << ">";
    return out;
  }
#endif

private:

  struct NegOp  { static T compute(T a) { return -a; } };
  struct Log2Op { static T compute(T a) { return Utils::getLog2(a); } };
  struct AbsOp  { static T compute(T a) { return a >= 0 ? +a : -a; } };
  struct InvOp  { static T compute(T a) { return (T)(1.0 / a); } };

  struct AddOp { static T compute(T a, T b) { return a + b; } };
  struct SubOp { static T compute(T a, T b) { return a - b; } };
  struct MulOp { static T compute(T a, T b) { return a * b; } };
  struct DivOp { static T compute(T a, T b) { return a / b; } };
  struct ModOp { static T compute(T a, T b) { return a % b; } };

  struct MinOp { static T compute(T a, T b) { return std::min(a, b); } };
  struct MaxOp { static T compute(T a, T b) { return std::max(a, b); } };

  struct LShiftOp { static T compute(T a, T b) { return a << b; } };
  struct RShiftOp { static T compute(T a, T b) { return a >> b; } };

  struct ConditionL  { static bool isTrue(T a, T b) { return a <  b; } };
  struct ConditionLE { static bool isTrue(T a, T b) { return a <= b; } };
  struct ConditionG  { static bool isTrue(T a, T b) { return a >  b; } };
  struct ConditionGE { static bool isTrue(T a, T b) { return a >= b; } };

  struct ConditionValidNumber { static bool isTrue(T a) { return Utils::isValidNumber(a); } };

  template <typename Coeff>
  class MulByCoeff {
  public:
    Coeff value;
    MulByCoeff(Coeff value_ = Coeff(0)) : value(value_) {}
    T compute(T a) { return (T)(a * value); }
  };

  class LShiftByValue  {
  public:
    T value;
    LShiftByValue(T value_ = T(0)) : value(value_) {}
    T compute(T a) { return a << value; }
  };

  class RShiftByValue {
  public:
    T value;
    RShiftByValue(T value_ = T(0)) : value(value_) {}
    T compute(T a) { return a >> value; }
  };

  //applyOperation
  template <typename Operation>
  static PointN applyOperation(const PointN& a)
  {
    auto pdim = a.getPointDim();
    PointN ret(pdim);
    switch (pdim)
    {
      case 5: ret[4] = Operation::compute(a[4]); /*following below*/
      case 4: ret[3] = Operation::compute(a[3]); /*following below*/
      case 3: ret[2] = Operation::compute(a[2]); /*following below*/
      case 2: ret[1] = Operation::compute(a[1]); /*following below*/
      case 1: ret[0] = Operation::compute(a[0]); /*following below*/
      case 0: return ret;
      default:
        for (int I = 0; I < pdim; I++)
          ret[I] = Operation::compute(a[I]);
        return ret;
    }
  }

  //applyOperation
  template <typename Operation>
  static PointN applyOperation(const PointN& a, Operation op)
  {
    auto pdim = a.getPointDim();
    PointN ret(pdim);
    switch (pdim)
    {
    case 5: ret[4] = op.compute(a[4]); /*following below*/
    case 4: ret[3] = op.compute(a[3]); /*following below*/
    case 3: ret[2] = op.compute(a[2]); /*following below*/
    case 2: ret[1] = op.compute(a[1]); /*following below*/
    case 1: ret[0] = op.compute(a[0]); /*following below*/
    case 0: return ret;
    default:
      for (int I = 0; I < pdim; I++)
        ret[I] = op.compute(a[I]);
      return ret;
    }
  }

  //applyOperation
  template <typename Operation>
  static PointN applyOperation(const PointN& a, const PointN& b)
  {
    auto pdim = a.getPointDim();
    VisusAssert(pdim == b.getPointDim());
    PointN ret(pdim);
    switch (pdim)
    {
    case 5: ret[4] = Operation::compute(a[4], b[4]); /*following below*/
    case 4: ret[3] = Operation::compute(a[3], b[3]); /*following below*/
    case 3: ret[2] = Operation::compute(a[2], b[2]); /*following below*/
    case 2: ret[1] = Operation::compute(a[1], b[1]); /*following below*/
    case 1: ret[0] = Operation::compute(a[0], b[0]); /*following below*/
    case 0: return ret;
    default:
      for (int I = 0; I < pdim; I++)
        ret[I] = Operation::compute(a[I], b[I]);
      return ret;
    }
  }

  //checkAll
  template <typename Condition>
  static bool checkAll(const PointN& a) {
    auto pdim = a.getPointDim();
    switch (pdim)
    {
    case 5: if (!Condition::isTrue(a[4])) return false; /*following below*/
    case 4: if (!Condition::isTrue(a[3])) return false; /*following below*/
    case 3: if (!Condition::isTrue(a[2])) return false; /*following below*/
    case 2: if (!Condition::isTrue(a[1])) return false; /*following below*/
    case 1: if (!Condition::isTrue(a[0])) return false; /*following below*/
    case 0: return true;
    default:
      for (int I = 0; I < pdim; I++)
        if (!Condition::isTrue(a[I])) return false;
      return true;
    }
  }

  //checkAll
  template <typename Condition>
  static bool checkAll(const PointN& a, const PointN& b) {
    auto pdim = a.getPointDim();
    switch (pdim)
    {
    case 5: if (!Condition::isTrue(a[4], b[4])) return false; /*following below*/
    case 4: if (!Condition::isTrue(a[3], b[3])) return false; /*following below*/
    case 3: if (!Condition::isTrue(a[2], b[2])) return false; /*following below*/
    case 2: if (!Condition::isTrue(a[1], b[1])) return false; /*following below*/
    case 1: if (!Condition::isTrue(a[0], b[0])) return false; /*following below*/
    case 0: return true;
    default:
      for (int I = 0; I < pdim; I++)
        if (!Condition::isTrue(a[I], b[I])) return false;
      return true;
    }
  }

  //accumulateOperation
  template <typename AccumulateOp>
  static T accumulateOperation(T initial_value, const PointN& a)
  {
    T ret = initial_value;
    auto pdim = a.getPointDim();
    switch (pdim)
    {
    case 5: ret = AccumulateOp::compute(ret, a[0]); ret = AccumulateOp::compute(ret, a[1]); ret = AccumulateOp::compute(ret, a[2]); ret = AccumulateOp::compute(ret, a[3]); ret = AccumulateOp::compute(ret, a[4]); return ret;
    case 4: ret = AccumulateOp::compute(ret, a[0]); ret = AccumulateOp::compute(ret, a[1]); ret = AccumulateOp::compute(ret, a[2]); ret = AccumulateOp::compute(ret, a[3]); return ret;
    case 3: ret = AccumulateOp::compute(ret, a[0]); ret = AccumulateOp::compute(ret, a[1]); ret = AccumulateOp::compute(ret, a[2]); return ret;
    case 2: ret = AccumulateOp::compute(ret, a[0]); ret = AccumulateOp::compute(ret, a[1]); return ret;
    case 1: ret = AccumulateOp::compute(ret, a[0]); return ret;
    case 0: return ret;
    default:
      for (int I = 0; I < pdim; I++)
        ret = AccumulateOp::compute(ret, a[I]);
      return ret;
    }
  }

};//end class PointN

template <typename Value, typename T>
inline PointN<T> operator*(Value s, const PointN<T>& p) {
  return p * s;
}

typedef  PointN<double> PointNd;
typedef  PointN<Int64>  PointNi;


template <typename T>
inline typename PointN<T>::ForEachPoint ForEachPoint(PointN<T> from, PointN<T> to, PointN<T> step) {
  return typename PointN<T>::ForEachPoint(from, to, step);
}

template <typename T>
inline typename PointN<T>::ForEachPoint ForEachPoint(PointN<T> dims) {
  auto pdim = dims.getPointDim();
  return ForEachPoint(PointN<T>(pdim), dims, PointN<T>::one(pdim));
}

} //namespace Visus


#endif //VISUS_POINT__H
