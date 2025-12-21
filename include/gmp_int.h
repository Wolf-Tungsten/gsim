#ifndef GSIM_GMP_INT_H
#define GSIM_GMP_INT_H

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <type_traits>

namespace gsim {

template<int WIDTH, bool SIGNED>
class GmpInt {
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

  GmpInt() { zero(); }

  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  explicit GmpInt(T v) { assign_integral(v); }

  GmpInt(const GmpInt& other) = default;
  GmpInt& operator=(const GmpInt& other) = default;
  GmpInt(GmpInt&& other) noexcept = default;
  GmpInt& operator=(GmpInt&& other) noexcept = default;
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt& operator=(T v) { assign_integral(v); return *this; }

  template<int OTHER_W, bool OTHER_S, typename = std::enable_if_t<(OTHER_W != WIDTH) || (OTHER_S != SIGNED)>>
  GmpInt(const GmpInt<OTHER_W, OTHER_S>& other) { assign_from_other(other); }
  template<int OTHER_W, bool OTHER_S, typename = std::enable_if_t<(OTHER_W != WIDTH) || (OTHER_S != SIGNED)>>
  GmpInt& operator=(const GmpInt<OTHER_W, OTHER_S>& other) { assign_from_other(other); return *this; }

  operator uint64_t() const { return data_[0]; }

  // Arithmetic
  GmpInt operator+(const GmpInt& rhs) const { return add(rhs); }
  GmpInt operator-(const GmpInt& rhs) const { return sub(rhs); }
  GmpInt operator*(const GmpInt& rhs) const { return mul(rhs); }
  GmpInt operator/(const GmpInt& rhs) const { return div(rhs, nullptr); }
  GmpInt operator%(const GmpInt& rhs) const { GmpInt rem; (void)div(rhs, &rem); return rem; }
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

  GmpInt& operator+=(const GmpInt& rhs) { *this = add(rhs); return *this; }
  GmpInt& operator-=(const GmpInt& rhs) { *this = sub(rhs); return *this; }
  GmpInt& operator*=(const GmpInt& rhs) { *this = mul(rhs); return *this; }
  GmpInt& operator/=(const GmpInt& rhs) { *this = div(rhs, nullptr); return *this; }
  GmpInt& operator%=(const GmpInt& rhs) { GmpInt rem; *this = div(rhs, &rem); *this = rem; return *this; }

  // Bitwise
  GmpInt operator&(const GmpInt& rhs) const { return bitwise(rhs, [](limb_t a, limb_t b) { return a & b; }); }
  GmpInt operator|(const GmpInt& rhs) const { return bitwise(rhs, [](limb_t a, limb_t b) { return a | b; }); }
  GmpInt operator^(const GmpInt& rhs) const { return bitwise(rhs, [](limb_t a, limb_t b) { return a ^ b; }); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator&(T rhs) const { return *this & GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator|(T rhs) const { return *this | GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator^(T rhs) const { return *this ^ GmpInt(rhs); }
  GmpInt& operator&=(const GmpInt& rhs) { *this = *this & rhs; return *this; }
  GmpInt& operator|=(const GmpInt& rhs) { *this = *this | rhs; return *this; }
  GmpInt& operator^=(const GmpInt& rhs) { *this = *this ^ rhs; return *this; }
  GmpInt operator~() const { GmpInt ret; for (size_t i = 0; i < kLimbCount; ++i) ret.data_[i] = ~data_[i]; ret.apply_mask(); return ret; }
  GmpInt operator-() const { return neg(); }

  // Shifts
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator<<(T shift) const { return shl(static_cast<unsigned>(shift)); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator>>(T shift) const { return shr(static_cast<unsigned>(shift)); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt& operator<<=(T shift) { *this = shl(static_cast<unsigned>(shift)); return *this; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt& operator>>=(T shift) { *this = shr(static_cast<unsigned>(shift)); return *this; }

  // Comparisons
  bool operator==(const GmpInt& rhs) const { return data_ == rhs.data_; }
  bool operator!=(const GmpInt& rhs) const { return !(*this == rhs); }
  bool operator<(const GmpInt& rhs) const { return compare(rhs) < 0; }
  bool operator>(const GmpInt& rhs) const { return rhs < *this; }
  bool operator<=(const GmpInt& rhs) const { return !(*this > rhs); }
  bool operator>=(const GmpInt& rhs) const { return !(*this < rhs); }

  friend std::ostream& operator<<(std::ostream& os, const GmpInt& v) {
    os << v.to_hex();
    return os;
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

 private:
  template<int, bool> friend class GmpInt;
  std::array<limb_t, kLimbCount> data_{};

  template<typename U>
  struct UnsignedHelper { using type = std::make_unsigned_t<U>; };
  template<>
  struct UnsignedHelper<bool> { using type = unsigned int; };

  void zero() { data_.fill(0); }

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
  void assign_from_other(const GmpInt<OTHER_W, OTHER_S>& other) {
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

  int compare(const GmpInt& rhs) const {
    if constexpr (SIGNED) {
      const bool neg_lhs = is_negative();
      const bool neg_rhs = rhs.is_negative();
      if (neg_lhs != neg_rhs) return neg_lhs ? -1 : 1;
    }
    return compare_unsigned(data_, rhs.data_);
  }

  template<typename Func>
  GmpInt bitwise(const GmpInt& rhs, Func func) const {
    GmpInt ret;
    for (size_t i = 0; i < kLimbCount; ++i) ret.data_[i] = func(data_[i], rhs.data_[i]);
    ret.apply_mask();
    return ret;
  }

  GmpInt add(const GmpInt& rhs) const {
    GmpInt ret;
    unsigned __int128 carry = 0;
    for (size_t i = 0; i < kLimbCount; ++i) {
      unsigned __int128 sum = static_cast<unsigned __int128>(data_[i]) + rhs.data_[i] + carry;
      ret.data_[i] = static_cast<limb_t>(sum);
      carry = sum >> kLimbBits;
    }
    ret.apply_mask();
    return ret;
  }

  GmpInt sub(const GmpInt& rhs) const {
    GmpInt ret;
    unsigned __int128 borrow = 0;
    for (size_t i = 0; i < kLimbCount; ++i) {
      unsigned __int128 diff = static_cast<unsigned __int128>(data_[i]) - rhs.data_[i] - borrow;
      ret.data_[i] = static_cast<limb_t>(diff);
      borrow = (diff >> (sizeof(unsigned __int128) * 8 - 1)) & 1;
    }
    ret.apply_mask();
    return ret;
  }

  GmpInt mul(const GmpInt& rhs) const {
    GmpInt ret;
    for (size_t i = 0; i < kLimbCount; ++i) {
      unsigned __int128 carry = 0;
      for (size_t j = 0; j + i < kLimbCount; ++j) {
        unsigned __int128 acc = static_cast<unsigned __int128>(data_[i]) * rhs.data_[j];
        acc += ret.data_[i + j];
        acc += carry;
        ret.data_[i + j] = static_cast<limb_t>(acc);
        carry = acc >> kLimbBits;
      }
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
    unsigned __int128 borrow = 0;
    for (size_t i = 0; i < a.size(); ++i) {
      unsigned __int128 diff = static_cast<unsigned __int128>(a[i]) - b[i] - borrow;
      a[i] = static_cast<limb_t>(diff);
      borrow = (diff >> (sizeof(unsigned __int128) * 8 - 1)) & 1;
    }
  }

  static void divmod_unsigned(const GmpInt& lhs, const GmpInt& rhs,
                              GmpInt& quot, GmpInt& rem) {
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

  GmpInt abs_value() const {
    if (is_negative()) return neg();
    return *this;
  }

  GmpInt neg() const {
    GmpInt ret;
    for (size_t i = 0; i < kLimbCount; ++i) ret.data_[i] = ~data_[i];
    unsigned __int128 carry = 1;
    for (size_t i = 0; i < kLimbCount; ++i) {
      unsigned __int128 sum = static_cast<unsigned __int128>(ret.data_[i]) + carry;
      ret.data_[i] = static_cast<limb_t>(sum);
      carry = sum >> kLimbBits;
      if (!carry) break;
    }
    ret.apply_mask();
    return ret;
  }

  GmpInt div(const GmpInt& rhs, GmpInt* rem_out) const {
    GmpInt quot, rem;
    if constexpr (!SIGNED) {
      divmod_unsigned(*this, rhs, quot, rem);
    } else {
      if (is_zero(rhs.data_)) {
        quot.zero();
        rem.zero();
      } else {
        const bool neg_lhs = is_negative();
        const bool neg_rhs = rhs.is_negative();
        GmpInt lhs_abs = abs_value();
        GmpInt rhs_abs = rhs.abs_value();
        divmod_unsigned(lhs_abs, rhs_abs, quot, rem);
        if (neg_lhs != neg_rhs) {
          if (!is_zero(rem.data_)) {
            GmpInt one(1);
            quot = (quot.add(one)).neg();
            GmpInt adj = rhs_abs.sub(rem);
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

  GmpInt shl(unsigned shift) const {
    if (shift >= static_cast<unsigned>(WIDTH)) return GmpInt();
    GmpInt ret;
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

  GmpInt shr(unsigned shift) const {
    if (shift >= static_cast<unsigned>(WIDTH)) {
      if constexpr (SIGNED) {
        return is_negative() ? all_ones() : GmpInt();
      } else {
        return GmpInt();
      }
    }
    GmpInt ret;
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

  static GmpInt all_ones() {
    GmpInt v;
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
