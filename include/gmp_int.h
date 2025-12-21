#ifndef GSIM_GMP_INT_H
#define GSIM_GMP_INT_H

#include <gmpxx.h>
#include <cstdint>
#include <type_traits>
#include <ostream>

namespace gsim {

template<int WIDTH, bool SIGNED>
class GmpInt {
 public:
  static_assert(WIDTH > 0, "WIDTH must be positive");
  static constexpr int width = WIDTH;
  static constexpr bool is_signed = SIGNED;

  GmpInt() : g_(0) {}

  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt(T v) { assign_integral(v); }

  explicit GmpInt(const mpz_class& v) { assign_mpz(v); }

  GmpInt(const GmpInt&) = default;
  GmpInt& operator=(const GmpInt&) = default;

  template<int OTHER_W, bool OTHER_S, typename = std::enable_if_t<OTHER_W != WIDTH || OTHER_S != SIGNED>>
  GmpInt(const GmpInt<OTHER_W, OTHER_S>& other) { assign_mpz(other.value()); }
  template<int OTHER_W, bool OTHER_S, typename = std::enable_if_t<OTHER_W != WIDTH || OTHER_S != SIGNED>>
  GmpInt& operator=(const GmpInt<OTHER_W, OTHER_S>& other) { assign_mpz(other.value()); return *this; }

  const mpz_class& value() const { return g_; }

  operator uint64_t() const { return canonical(g_).get_ui(); }

  // Arithmetic
  GmpInt operator+(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a + b; }); }
  GmpInt operator-(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a - b; }); }
  GmpInt operator*(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a * b; }); }
  GmpInt operator/(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return b == 0 ? mpz_class(0) : a / b; }); }
  GmpInt operator%(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return b == 0 ? mpz_class(0) : a % b; }); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator+(T rhs) const { return *this + GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator-(T rhs) const { return *this - GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator*(T rhs) const { return *this * GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator/(T rhs) const { return *this / GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator%(T rhs) const { return *this % GmpInt(rhs); }

  GmpInt& operator+=(const GmpInt& rhs) { *this = *this + rhs; return *this; }
  GmpInt& operator-=(const GmpInt& rhs) { *this = *this - rhs; return *this; }
  GmpInt& operator*=(const GmpInt& rhs) { *this = *this * rhs; return *this; }
  GmpInt& operator/=(const GmpInt& rhs) { *this = *this / rhs; return *this; }
  GmpInt& operator%=(const GmpInt& rhs) { *this = *this % rhs; return *this; }

  // Bitwise
  GmpInt operator&(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a & b; }); }
  GmpInt operator|(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a | b; }); }
  GmpInt operator^(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a ^ b; }); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator&(T rhs) const { return *this & GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator|(T rhs) const { return *this | GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator^(T rhs) const { return *this ^ GmpInt(rhs); }
  GmpInt& operator&=(const GmpInt& rhs) { *this = *this & rhs; return *this; }
  GmpInt& operator|=(const GmpInt& rhs) { *this = *this | rhs; return *this; }
  GmpInt& operator^=(const GmpInt& rhs) { *this = *this ^ rhs; return *this; }

  GmpInt operator~() const { GmpInt ret(*this); ret.g_ = ~ret.g_; ret.normalize(); return ret; }
  GmpInt operator-() const { GmpInt ret(*this); ret.g_ = -ret.g_; ret.normalize(); return ret; }

  // Shifts
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator<<(T shift) const { return shift_left(static_cast<unsigned>(shift)); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator>>(T shift) const { return shift_right(static_cast<unsigned>(shift)); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt& operator<<=(T shift) { *this = shift_left(static_cast<unsigned>(shift)); return *this; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt& operator>>=(T shift) { *this = shift_right(static_cast<unsigned>(shift)); return *this; }

  // Comparisons
  bool operator==(const GmpInt& rhs) const { return canonical(g_) == canonical(rhs.g_); }
  bool operator!=(const GmpInt& rhs) const { return !(*this == rhs); }
  bool operator<(const GmpInt& rhs) const { return canonical(g_) < canonical(rhs.g_); }
  bool operator>(const GmpInt& rhs) const { return rhs < *this; }
  bool operator<=(const GmpInt& rhs) const { return !(*this > rhs); }
  bool operator>=(const GmpInt& rhs) const { return !(*this < rhs); }

  friend std::ostream& operator<<(std::ostream& os, const GmpInt& v) {
    return os << v.canonical(v.g_).get_str(16);
  }

  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator+(T lhs, const GmpInt& rhs) { return GmpInt(lhs) + rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator-(T lhs, const GmpInt& rhs) { return GmpInt(lhs) - rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator*(T lhs, const GmpInt& rhs) { return GmpInt(lhs) * rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator/(T lhs, const GmpInt& rhs) { return GmpInt(lhs) / rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator%(T lhs, const GmpInt& rhs) { return GmpInt(lhs) % rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator&(T lhs, const GmpInt& rhs) { return GmpInt(lhs) & rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator|(T lhs, const GmpInt& rhs) { return GmpInt(lhs) | rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator^(T lhs, const GmpInt& rhs) { return GmpInt(lhs) ^ rhs; }

 protected:
  mpz_class g_;

  static mpz_class mask() {
    static mpz_class m = []() {
      mpz_class tmp(1);
      mpz_mul_2exp(tmp.get_mpz_t(), tmp.get_mpz_t(), WIDTH);
      tmp -= 1;
      return tmp;
    }();
    return m;
  }

  static mpz_class two_pow_width() {
    static mpz_class v = []() {
      mpz_class tmp(1);
      mpz_mul_2exp(tmp.get_mpz_t(), tmp.get_mpz_t(), WIDTH);
      return tmp;
    }();
    return v;
  }

  static mpz_class canonical(const mpz_class& in) {
    if constexpr (SIGNED) {
      mpz_class v = in & mask();
      if (mpz_tstbit(v.get_mpz_t(), WIDTH - 1)) v -= two_pow_width();
      return v;
    } else {
      return in & mask();
    }
  }

  void normalize() { g_ = canonical(g_); }

  template<typename Op>
  GmpInt binary_op(const GmpInt& rhs, Op op) const {
    GmpInt ret;
    ret.g_ = canonical(op(g_, rhs.g_));
    return ret;
  }

  GmpInt shift_left(unsigned shift) const {
    GmpInt ret(*this);
    mpz_mul_2exp(ret.g_.get_mpz_t(), ret.g_.get_mpz_t(), shift);
    ret.normalize();
    return ret;
  }

  GmpInt shift_right(unsigned shift) const {
    GmpInt ret(*this);
    if constexpr (SIGNED) {
      mpz_fdiv_q_2exp(ret.g_.get_mpz_t(), ret.g_.get_mpz_t(), shift);
    } else {
      mpz_tdiv_q_2exp(ret.g_.get_mpz_t(), ret.g_.get_mpz_t(), shift);
    }
    ret.normalize();
    return ret;
  }

  template<typename T>
  void assign_integral(T v) {
    if constexpr (std::is_signed<T>::value) mpz_set_si(g_.get_mpz_t(), static_cast<long>(v));
    else mpz_set_ui(g_.get_mpz_t(), static_cast<unsigned long>(v));
    normalize();
  }

  void assign_mpz(const mpz_class& v) {
    g_ = canonical(v);
  }
};

template<int WIDTH> using GmpIntU = GmpInt<WIDTH, false>;
template<int WIDTH> using GmpIntS = GmpInt<WIDTH, true>;
template<int WIDTH> using GmpWideU = GmpIntU<WIDTH>;
template<int WIDTH> using GmpWideS = GmpIntS<WIDTH>;

} // namespace gsim

using gsim::GmpIntU;
using gsim::GmpIntS;
using gsim::GmpWideU;
using gsim::GmpWideS;

#endif // GSIM_GMP_INT_H
