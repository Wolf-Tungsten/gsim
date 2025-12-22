#ifndef GSIM_GSIM_INT_H
#define GSIM_GSIM_INT_H

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <type_traits>

namespace gsim {

template<typename U, typename Enable = void>
struct UnsignedHelper {
  using type = std::make_unsigned_t<U>;
};

template<typename U>
struct UnsignedHelper<U, std::enable_if_t<std::is_same<U, bool>::value>> {
  using type = unsigned int;
};

template<int WIDTH, bool SIGNED>
class GsimInt {
 public:
  static_assert(WIDTH > 0, "WIDTH must be positive");
  using limb_t = uint64_t;
  static constexpr int width = WIDTH;
  static constexpr bool is_signed = SIGNED;
  static constexpr size_t kLimbBits = 64;
  static constexpr size_t kLimbCount = (WIDTH + kLimbBits - 1) / kLimbBits;
  static constexpr limb_t kTopMask = (WIDTH % kLimbBits == 0)
      ? ~limb_t(0)
      : ((limb_t(1) << (WIDTH % kLimbBits)) - 1);

  GsimInt() { zero(); }

  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  explicit GsimInt(T v) { assign_integral(v); }

  GsimInt(const GsimInt& other) = default;
  GsimInt& operator=(const GsimInt& other) = default;
  GsimInt(GsimInt&& other) noexcept = default;
  GsimInt& operator=(GsimInt&& other) noexcept = default;
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GsimInt& operator=(T v) { assign_integral(v); return *this; }

  template<int OTHER_W, bool OTHER_S,
           typename = std::enable_if_t<((OTHER_W != WIDTH) || (OTHER_S != SIGNED)) && (OTHER_W <= WIDTH)>>
  GsimInt(const GsimInt<OTHER_W, OTHER_S>& other) { assign_from_other(other); }
  // Narrowing conversions require an explicit cast to avoid ambiguous implicit paths.
  template<int OTHER_W, bool OTHER_S, typename = void,
           typename = std::enable_if_t<(OTHER_W > WIDTH)>>
  explicit GsimInt(const GsimInt<OTHER_W, OTHER_S>& other) { assign_from_other(other); }
  template<int OTHER_W, bool OTHER_S, typename = std::enable_if_t<(OTHER_W != WIDTH) || (OTHER_S != SIGNED)>>
  GsimInt& operator=(const GsimInt<OTHER_W, OTHER_S>& other) { assign_from_other(other); return *this; }

  operator uint64_t() const { return data_[0]; }

  // Arithmetic
  GsimInt operator+(const GsimInt& rhs) const { return add(rhs); }
  GsimInt operator-(const GsimInt& rhs) const { return sub(rhs); }
  GsimInt operator*(const GsimInt& rhs) const { return mul(rhs); }
  GsimInt operator/(const GsimInt& rhs) const { return div(rhs, nullptr); }
  GsimInt operator%(const GsimInt& rhs) const { GsimInt rem; (void)div(rhs, &rem); return rem; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GsimInt operator+(T rhs) const { return *this + GsimInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GsimInt operator-(T rhs) const { return *this - GsimInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GsimInt operator*(T rhs) const { return *this * GsimInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GsimInt operator/(T rhs) const { return *this / GsimInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GsimInt operator%(T rhs) const { return *this % GsimInt(rhs); }

  GsimInt& operator+=(const GsimInt& rhs) { *this = add(rhs); return *this; }
  GsimInt& operator-=(const GsimInt& rhs) { *this = sub(rhs); return *this; }
  GsimInt& operator*=(const GsimInt& rhs) { *this = mul(rhs); return *this; }
  GsimInt& operator/=(const GsimInt& rhs) { *this = div(rhs, nullptr); return *this; }
  GsimInt& operator%=(const GsimInt& rhs) { GsimInt rem; *this = div(rhs, &rem); *this = rem; return *this; }

  // Bitwise
  GsimInt operator&(const GsimInt& rhs) const { return bitwise(rhs, [](limb_t a, limb_t b) { return a & b; }); }
  GsimInt operator|(const GsimInt& rhs) const { return bitwise(rhs, [](limb_t a, limb_t b) { return a | b; }); }
  GsimInt operator^(const GsimInt& rhs) const { return bitwise(rhs, [](limb_t a, limb_t b) { return a ^ b; }); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GsimInt operator&(T rhs) const { return *this & GsimInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GsimInt operator|(T rhs) const { return *this | GsimInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GsimInt operator^(T rhs) const { return *this ^ GsimInt(rhs); }
  GsimInt& operator&=(const GsimInt& rhs) { *this = *this & rhs; return *this; }
  GsimInt& operator|=(const GsimInt& rhs) { *this = *this | rhs; return *this; }
  GsimInt& operator^=(const GsimInt& rhs) { *this = *this ^ rhs; return *this; }
  GsimInt operator~() const { GsimInt ret; for (size_t i = 0; i < kLimbCount; ++i) ret.data_[i] = ~data_[i]; ret.apply_mask(); return ret; }
  GsimInt operator-() const { return neg(); }

  // Shifts
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GsimInt operator<<(T shift) const { return shl(static_cast<unsigned>(shift)); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GsimInt operator>>(T shift) const { return shr(static_cast<unsigned>(shift)); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GsimInt& operator<<=(T shift) { *this = shl(static_cast<unsigned>(shift)); return *this; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GsimInt& operator>>=(T shift) { *this = shr(static_cast<unsigned>(shift)); return *this; }

  // Comparisons
  bool operator==(const GsimInt& rhs) const { return data_ == rhs.data_; }
  bool operator!=(const GsimInt& rhs) const { return !(*this == rhs); }
  bool operator<(const GsimInt& rhs) const { return compare(rhs) < 0; }
  bool operator>(const GsimInt& rhs) const { return rhs < *this; }
  bool operator<=(const GsimInt& rhs) const { return !(*this > rhs); }
  bool operator>=(const GsimInt& rhs) const { return !(*this < rhs); }

  friend std::ostream& operator<<(std::ostream& os, const GsimInt& v) {
    os << v.to_hex();
    return os;
  }

  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GsimInt operator+(T lhs, const GsimInt& rhs) { return GsimInt(lhs) + rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GsimInt operator-(T lhs, const GsimInt& rhs) { return GsimInt(lhs) - rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GsimInt operator*(T lhs, const GsimInt& rhs) { return GsimInt(lhs) * rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GsimInt operator/(T lhs, const GsimInt& rhs) { return GsimInt(lhs) / rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GsimInt operator%(T lhs, const GsimInt& rhs) { return GsimInt(lhs) % rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GsimInt operator&(T lhs, const GsimInt& rhs) { return GsimInt(lhs) & rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GsimInt operator|(T lhs, const GsimInt& rhs) { return GsimInt(lhs) | rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GsimInt operator^(T lhs, const GsimInt& rhs) { return GsimInt(lhs) ^ rhs; }

 private:
  template<int, bool> friend class GsimInt;
  std::array<limb_t, kLimbCount> data_{};

  void zero() { data_.fill(0); }

  static limb_t add2(limb_t a, limb_t b, limb_t& carry_out) {
    limb_t res = a + b;
    carry_out = (res < a) ? 1 : 0;
    return res;
  }

  static limb_t add3(limb_t a, limb_t b, limb_t c, limb_t& carry_out) {
    limb_t carry1 = 0;
    limb_t res = add2(a, b, carry1);
    limb_t carry2 = 0;
    res = add2(res, c, carry2);
    carry_out = carry1 + carry2; // can be 0,1,2
    return res;
  }

  static limb_t sub2(limb_t a, limb_t b, limb_t& borrow_out) {
    limb_t res = a - b;
    borrow_out = (a < b) ? 1 : 0;
    return res;
  }

  static limb_t sub3(limb_t a, limb_t b, limb_t c, limb_t& borrow_out) {
    limb_t borrow1 = 0;
    limb_t res = sub2(a, b, borrow1);
    limb_t borrow2 = 0;
    res = sub2(res, c, borrow2);
    borrow_out = borrow1 + borrow2; // can be 0,1,2
    return res;
  }

  static void mul_64(limb_t a, limb_t b, limb_t& hi, limb_t& lo) {
    const limb_t a0 = static_cast<uint32_t>(a);
    const limb_t a1 = a >> 32;
    const limb_t b0 = static_cast<uint32_t>(b);
    const limb_t b1 = b >> 32;

    const limb_t p0 = a0 * b0;
    const limb_t p1 = a0 * b1;
    const limb_t p2 = a1 * b0;
    const limb_t p3 = a1 * b1;

    limb_t mid_low = (p0 >> 32) + (p1 & 0xffffffffULL) + (p2 & 0xffffffffULL);
    limb_t carry = mid_low >> 32;
    lo = (p0 & 0xffffffffULL) | (mid_low << 32);

    hi = p3 + (p1 >> 32) + (p2 >> 32) + carry;
  }

  bool is_negative() const {
    if constexpr (!SIGNED) return false;
    const unsigned bit = (WIDTH - 1) % kLimbBits;
    return (data_[kLimbCount - 1] >> bit) & 1;
  }

  static bool is_zero(const std::array<limb_t, kLimbCount>& d) {
    for (auto v : d) if (v != 0) return false;
    return true;
  }

  void apply_mask() {
    data_[kLimbCount - 1] &= kTopMask;
  }

  template<typename T>
  void assign_integral(T v) {
    zero();
    using UnsignedT = typename UnsignedHelper<T>::type;
    UnsignedT val = static_cast<UnsignedT>(v);
    data_[0] = static_cast<limb_t>(val);
    if constexpr (SIGNED && std::is_signed<T>::value) {
      if (v < 0) {
        for (size_t i = 1; i < kLimbCount; ++i) data_[i] = ~limb_t(0);
      }
    }
    apply_mask();
  }

  template<int OTHER_W, bool OTHER_S>
  void assign_from_other(const GsimInt<OTHER_W, OTHER_S>& other) {
    zero();
    constexpr size_t min_limbs = (OTHER_W + kLimbBits - 1) / kLimbBits < kLimbCount
                                   ? (OTHER_W + kLimbBits - 1) / kLimbBits
                                   : kLimbCount;
    for (size_t i = 0; i < min_limbs; ++i) data_[i] = other.raw_limb(i);
    if constexpr (SIGNED && OTHER_S) {
      if (other.is_negative()) {
        for (size_t i = min_limbs; i < kLimbCount; ++i) data_[i] = ~limb_t(0);
      }
    }
    apply_mask();
  }

  limb_t raw_limb(size_t idx) const { return idx < kLimbCount ? data_[idx] : 0; }

  static int compare_unsigned(const std::array<limb_t, kLimbCount>& a,
                              const std::array<limb_t, kLimbCount>& b) {
    for (size_t idx = kLimbCount; idx-- > 0;) {
      if (a[idx] == b[idx]) continue;
      return (a[idx] > b[idx]) ? 1 : -1;
    }
    return 0;
  }

  int compare(const GsimInt& rhs) const {
    if constexpr (SIGNED) {
      const bool neg_lhs = is_negative();
      const bool neg_rhs = rhs.is_negative();
      if (neg_lhs != neg_rhs) return neg_lhs ? -1 : 1;
    }
    return compare_unsigned(data_, rhs.data_);
  }

  template<typename Func>
  GsimInt bitwise(const GsimInt& rhs, Func func) const {
    GsimInt ret;
    for (size_t i = 0; i < kLimbCount; ++i) ret.data_[i] = func(data_[i], rhs.data_[i]);
    ret.apply_mask();
    return ret;
  }

  GsimInt add(const GsimInt& rhs) const {
    GsimInt ret;
    limb_t carry = 0;
    for (size_t i = 0; i < kLimbCount; ++i) {
      limb_t carry_out = 0;
      ret.data_[i] = add3(data_[i], rhs.data_[i], carry, carry_out);
      carry = carry_out;
    }
    ret.apply_mask();
    return ret;
  }

  GsimInt sub(const GsimInt& rhs) const {
    GsimInt ret;
    limb_t borrow = 0;
    for (size_t i = 0; i < kLimbCount; ++i) {
      limb_t borrow_out = 0;
      ret.data_[i] = sub3(data_[i], rhs.data_[i], borrow, borrow_out);
      borrow = borrow_out;
    }
    ret.apply_mask();
    return ret;
  }

  GsimInt mul(const GsimInt& rhs) const {
    GsimInt ret;
    for (size_t i = 0; i < kLimbCount; ++i) {
      limb_t carry = 0;
      limb_t carry_extra = 0;
      for (size_t j = 0; j + i < kLimbCount; ++j) {
        // fold any extra overflow from the previous iteration into carry
        if (carry_extra) {
          limb_t overflow = 0;
          carry = add2(carry, carry_extra, overflow);
          carry_extra = overflow;
        }

        limb_t hi = 0, lo = 0;
        mul_64(data_[i], rhs.data_[j], hi, lo);

        limb_t carry_sum = 0;
        limb_t tmp = add3(ret.data_[i + j], lo, carry, carry_sum);
        ret.data_[i + j] = tmp;

        limb_t next_carry_overflow = 0;
        carry = add3(hi, carry_sum, 0, next_carry_overflow);
        carry_extra += next_carry_overflow;
      }
      // any remaining carry or carry_extra falls outside the truncated width and is discarded
    }
    ret.apply_mask();
    return ret;
  }

  static bool get_bit(const std::array<limb_t, kLimbCount>& d, unsigned bit) {
    return (d[bit / kLimbBits] >> (bit % kLimbBits)) & 1;
  }

  static void set_bit(std::array<limb_t, kLimbCount>& d, unsigned bit, bool val) {
    limb_t mask = limb_t(1) << (bit % kLimbBits);
    if (val) d[bit / kLimbBits] |= mask;
    else d[bit / kLimbBits] &= ~mask;
  }

  static void ext_shift_left1(std::array<limb_t, kLimbCount + 1>& d) {
    limb_t carry = 0;
    for (size_t i = 0; i < d.size(); ++i) {
      limb_t next = d[i] >> (kLimbBits - 1);
      d[i] = (d[i] << 1) | carry;
      carry = next;
    }
  }

  static int ext_compare(const std::array<limb_t, kLimbCount + 1>& a,
                         const std::array<limb_t, kLimbCount + 1>& b) {
    for (size_t idx = kLimbCount + 1; idx-- > 0;) {
      if (a[idx] == b[idx]) continue;
      return (a[idx] > b[idx]) ? 1 : -1;
    }
    return 0;
  }

  static void ext_sub(std::array<limb_t, kLimbCount + 1>& a,
                      const std::array<limb_t, kLimbCount + 1>& b) {
    limb_t borrow = 0;
    for (size_t i = 0; i < a.size(); ++i) {
      limb_t borrow_out = 0;
      a[i] = sub3(a[i], b[i], borrow, borrow_out);
      borrow = borrow_out;
    }
  }

  static void divmod_unsigned(const GsimInt& lhs, const GsimInt& rhs,
                              GsimInt& quot, GsimInt& rem) {
    quot.zero();
    rem.zero();
    if (is_zero(rhs.data_)) return;

    std::array<limb_t, kLimbCount + 1> rem_ext{};
    std::array<limb_t, kLimbCount + 1> rhs_ext{};
    for (size_t i = 0; i < kLimbCount; ++i) rhs_ext[i] = rhs.data_[i];

    for (int bit = WIDTH - 1; bit >= 0; --bit) {
      ext_shift_left1(rem_ext);
      if (get_bit(lhs.data_, static_cast<unsigned>(bit))) rem_ext[0] |= 1;
      if (ext_compare(rem_ext, rhs_ext) >= 0) {
        ext_sub(rem_ext, rhs_ext);
        set_bit(quot.data_, static_cast<unsigned>(bit), true);
      }
    }
    for (size_t i = 0; i < kLimbCount; ++i) rem.data_[i] = rem_ext[i];
    quot.apply_mask();
    rem.apply_mask();
  }

  GsimInt abs_value() const {
    if (is_negative()) return neg();
    return *this;
  }

  GsimInt neg() const {
    GsimInt ret;
    for (size_t i = 0; i < kLimbCount; ++i) ret.data_[i] = ~data_[i];
    limb_t carry = 1;
    for (size_t i = 0; i < kLimbCount; ++i) {
      limb_t carry_out = 0;
      ret.data_[i] = add3(ret.data_[i], carry, 0, carry_out);
      carry = carry_out;
      if (!carry) break;
    }
    ret.apply_mask();
    return ret;
  }

  GsimInt div(const GsimInt& rhs, GsimInt* rem_out) const {
    GsimInt quot, rem;
    if constexpr (!SIGNED) {
      divmod_unsigned(*this, rhs, quot, rem);
    } else {
      if (is_zero(rhs.data_)) {
        quot.zero();
        rem.zero();
      } else {
        const bool neg_lhs = is_negative();
        const bool neg_rhs = rhs.is_negative();
        GsimInt lhs_abs = abs_value();
        GsimInt rhs_abs = rhs.abs_value();
        divmod_unsigned(lhs_abs, rhs_abs, quot, rem);
        if (neg_lhs != neg_rhs) {
          if (!is_zero(rem.data_)) {
            GsimInt one(1);
            quot = (quot.add(one)).neg();
            GsimInt adj = rhs_abs.sub(rem);
            rem = neg_rhs ? adj.neg() : adj;
          } else {
            quot = quot.neg();
          }
        } else {
          if (neg_rhs) rem = rem.neg();
        }
      }
    }
    quot.apply_mask();
    rem.apply_mask();
    if (rem_out) *rem_out = rem;
    return quot;
  }

  GsimInt shl(unsigned shift) const {
    if (shift >= static_cast<unsigned>(WIDTH)) return GsimInt();
    GsimInt ret;
    const unsigned limb_shift = shift / kLimbBits;
    const unsigned bit_shift = shift % kLimbBits;
    for (size_t i = kLimbCount; i-- > 0;) {
      if (i < limb_shift) {
        ret.data_[i] = 0;
        continue;
      }
      size_t src = i - limb_shift;
      limb_t val = data_[src] << bit_shift;
      if (bit_shift && src > 0) {
        val |= data_[src - 1] >> (kLimbBits - bit_shift);
      }
      ret.data_[i] = val;
    }
    ret.apply_mask();
    return ret;
  }

  GsimInt shr(unsigned shift) const {
    if (shift >= static_cast<unsigned>(WIDTH)) {
      if constexpr (SIGNED) {
        return is_negative() ? all_ones() : GsimInt();
      } else {
        return GsimInt();
      }
    }
    GsimInt ret;
    const unsigned limb_shift = shift / kLimbBits;
    const unsigned bit_shift = shift % kLimbBits;
    for (size_t i = 0; i < kLimbCount; ++i) {
      size_t src = i + limb_shift;
      if (src >= kLimbCount) {
        ret.data_[i] = 0;
        continue;
      }
      limb_t val = data_[src] >> bit_shift;
      if (bit_shift && src + 1 < kLimbCount) {
        val |= data_[src + 1] << (kLimbBits - bit_shift);
      }
      ret.data_[i] = val;
    }
    if constexpr (SIGNED) {
      if (is_negative() && bit_shift) {
        const unsigned fill_bits = kLimbBits - bit_shift;
        limb_t fill_mask = (~limb_t(0)) << fill_bits;
        ret.data_[kLimbCount - 1] |= fill_mask;
      }
      if (is_negative()) {
        for (size_t i = kLimbCount; i-- > 0;) {
          if (i + limb_shift >= kLimbCount) ret.data_[i] = ~limb_t(0);
          else break;
        }
      }
    }
    ret.apply_mask();
    return ret;
  }

  static GsimInt all_ones() {
    GsimInt v;
    for (auto& x : v.data_) x = ~limb_t(0);
    v.apply_mask();
    return v;
  }

  std::string to_hex() const {
    std::ostringstream oss;
    size_t ms_limb = kLimbCount;
    while (ms_limb > 0 && data_[ms_limb - 1] == 0) --ms_limb;
    if (ms_limb == 0) {
      oss << "0";
      return oss.str();
    }
    oss << std::hex << data_[ms_limb - 1];
    for (size_t i = ms_limb; i-- > 0;) {
      if (i == 0) break;
      oss << std::setfill('0') << std::setw(16) << data_[i - 1];
    }
    return oss.str();
  }
};

template<int WIDTH> using GsimIntU = GsimInt<WIDTH, false>;
template<int WIDTH> using GsimIntS = GsimInt<WIDTH, true>;
template<int WIDTH> using GsimWideU = GsimIntU<WIDTH>;
template<int WIDTH> using GsimWideS = GsimIntS<WIDTH>;

} // namespace gsim

using gsim::GsimIntU;
using gsim::GsimIntS;
using gsim::GsimWideU;
using gsim::GsimWideS;

#endif // GSIM_GSIM_INT_H
