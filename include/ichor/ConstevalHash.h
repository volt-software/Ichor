#pragma once

#include <wyhash.h>

static consteval uint64_t consteval_wyrotr(uint64_t v, unsigned k) { return (v >> k) | (v << (64 - k)); }

static consteval uint64_t consteval_wymum(uint64_t A, uint64_t B) {
#ifdef    WYHASH32
    uint64_t	hh=(A>>32)*(B>>32),	hl=(A>>32)*(unsigned)B,	lh=(unsigned)A*(B>>32),	ll=(uint64_t)(unsigned)A*(unsigned)B;
        return	consteval_wyrotr(hl,32)^consteval_wyrotr(lh,32)^hh^ll;
#else
#ifdef __SIZEOF_INT128__
    __uint128_t r = A;
    r *= B;
    return (r >> 64U) ^ r;
#elif    defined(_MSC_VER) && defined(_M_X64)
    A=_umul128(A, B, &B);	return	A^B;
#else
        uint64_t	ha=A>>32,	hb=B>>32,	la=(uint32_t)A,	lb=(uint32_t)B,	hi, lo;
        uint64_t	rh=ha*hb,	rm0=ha*lb,	rm1=hb*la,	rl=la*lb,	t=rl+(rm0<<32),	c=t<rl;
        lo=t+(rm1<<32);	c+=lo<t;hi=rh+(rm0>>32)+(rm1>>32)+c;	return hi^lo;
#endif
#endif
}

template<typename T>
static consteval uint64_t consteval_wyr8(const T *p) {
    uint64_t v = 0;
    v = ((uint64_t) p[0] << 56U) | ((uint64_t) p[1] << 48U) | ((uint64_t) p[2] << 40U) | ((uint64_t) p[3] << 32U) |
        (p[4] << 24U) | (p[5] << 16U) | (p[6] << 8U) | p[7];
    return v;
}

template<typename T>
static consteval uint64_t consteval_wyr4(const T *p) {
    uint32_t v = 0;
    v = (p[0] << 24U) | (p[1] << 16U) | (p[2] << 8U) | p[3];
    return v;
}

template<typename T>
static consteval uint64_t consteval_wyr3(const T *p, unsigned k) {
    return (((uint64_t) p[0]) << 16U) | (((uint64_t) p[k >> 1U]) << 8U) | p[k - 1];
}

template<typename T>
static consteval uint64_t consteval_wyhash(const T *key, uint64_t len, uint64_t seed) {
    static_assert(sizeof(T) == 1, "T must be a char or uint8_t kind type");
#if defined(__GNUC__) || defined(__INTEL_COMPILER)
    if (__builtin_expect(!len, 0)) return 0;
#else
    if(!len)	return	0;
#endif
    const T *p = key;
    if (len < 4)
        return consteval_wymum(consteval_wymum(consteval_wyr3(p, len) ^ seed ^ _wyp0, seed ^ _wyp1), len ^ _wyp4);
    else if (len <= 8)
        return consteval_wymum(
                consteval_wymum(consteval_wyr4(p) ^ seed ^ _wyp0, consteval_wyr4(p + len - 4) ^ seed ^ _wyp1),
                len ^ _wyp4);
    else if (len <= 16)
        return consteval_wymum(
                consteval_wymum(consteval_wyr8(p) ^ seed ^ _wyp0, consteval_wyr8(p + len - 8) ^ seed ^ _wyp1),
                len ^ _wyp4);
    else if (len <= 24)
        return consteval_wymum(
                consteval_wymum(consteval_wyr8(p) ^ seed ^ _wyp0, consteval_wyr8(p + 8) ^ seed ^ _wyp1) ^
                consteval_wymum(consteval_wyr8(p + len - 8) ^ seed ^ _wyp2, seed ^ _wyp3), len ^ _wyp4);
    else if (len <= 32)
        return consteval_wymum(
                consteval_wymum(consteval_wyr8(p) ^ seed ^ _wyp0, consteval_wyr8(p + 8) ^ seed ^ _wyp1) ^
                consteval_wymum(consteval_wyr8(p + 16) ^ seed ^ _wyp2, consteval_wyr8(p + len - 8) ^ seed ^ _wyp3),
                len ^ _wyp4);
    uint64_t see1 = seed, i = len;
    if (i >= 256)
        for (; i >= 256; i -= 256, p += 256) {
            seed = consteval_wymum(consteval_wyr8(p) ^ seed ^ _wyp0, consteval_wyr8(p + 8) ^ seed ^ _wyp1) ^
                   consteval_wymum(consteval_wyr8(p + 16) ^ seed ^ _wyp2, consteval_wyr8(p + 24) ^ seed ^ _wyp3);
            see1 = consteval_wymum(consteval_wyr8(p + 32) ^ see1 ^ _wyp1, consteval_wyr8(p + 40) ^ see1 ^ _wyp2) ^
                   consteval_wymum(consteval_wyr8(p + 48) ^ see1 ^ _wyp3, consteval_wyr8(p + 56) ^ see1 ^ _wyp0);
            seed = consteval_wymum(consteval_wyr8(p + 64) ^ seed ^ _wyp0, consteval_wyr8(p + 72) ^ seed ^ _wyp1) ^
                   consteval_wymum(consteval_wyr8(p + 80) ^ seed ^ _wyp2, consteval_wyr8(p + 88) ^ seed ^ _wyp3);
            see1 = consteval_wymum(consteval_wyr8(p + 96) ^ see1 ^ _wyp1, consteval_wyr8(p + 104) ^ see1 ^ _wyp2) ^
                   consteval_wymum(consteval_wyr8(p + 112) ^ see1 ^ _wyp3, consteval_wyr8(p + 120) ^ see1 ^ _wyp0);
            seed = consteval_wymum(consteval_wyr8(p + 128) ^ seed ^ _wyp0, consteval_wyr8(p + 136) ^ seed ^ _wyp1) ^
                   consteval_wymum(consteval_wyr8(p + 144) ^ seed ^ _wyp2, consteval_wyr8(p + 152) ^ seed ^ _wyp3);
            see1 = consteval_wymum(consteval_wyr8(p + 160) ^ see1 ^ _wyp1, consteval_wyr8(p + 168) ^ see1 ^ _wyp2) ^
                   consteval_wymum(consteval_wyr8(p + 176) ^ see1 ^ _wyp3, consteval_wyr8(p + 184) ^ see1 ^ _wyp0);
            seed = consteval_wymum(consteval_wyr8(p + 192) ^ seed ^ _wyp0, consteval_wyr8(p + 200) ^ seed ^ _wyp1) ^
                   consteval_wymum(consteval_wyr8(p + 208) ^ seed ^ _wyp2, consteval_wyr8(p + 216) ^ seed ^ _wyp3);
            see1 = consteval_wymum(consteval_wyr8(p + 224) ^ see1 ^ _wyp1, consteval_wyr8(p + 232) ^ see1 ^ _wyp2) ^
                   consteval_wymum(consteval_wyr8(p + 240) ^ see1 ^ _wyp3, consteval_wyr8(p + 248) ^ see1 ^ _wyp0);
        }
    for (; i >= 32; i -= 32, p += 32) {
        seed = consteval_wymum(consteval_wyr8(p) ^ seed ^ _wyp0, consteval_wyr8(p + 8) ^ seed ^ _wyp1);
        see1 = consteval_wymum(consteval_wyr8(p + 16) ^ see1 ^ _wyp2, consteval_wyr8(p + 24) ^ see1 ^ _wyp3);
    }
    if (!i) {}
    else if (i < 4) seed = consteval_wymum(consteval_wyr3(p, i) ^ seed ^ _wyp0, seed ^ _wyp1);
    else if (i <= 8)
        seed = consteval_wymum(consteval_wyr4(p) ^ seed ^ _wyp0, consteval_wyr4(p + i - 4) ^ seed ^ _wyp1);
    else if (i <= 16)
        seed = consteval_wymum(consteval_wyr8(p) ^ seed ^ _wyp0, consteval_wyr8(p + i - 8) ^ seed ^ _wyp1);
    else if (i <= 24) {
        seed = consteval_wymum(consteval_wyr8(p) ^ seed ^ _wyp0, consteval_wyr8(p + 8) ^ seed ^ _wyp1);
        see1 = consteval_wymum(consteval_wyr8(p + i - 8) ^ see1 ^ _wyp2, see1 ^ _wyp3);
    }
    else {
        seed = consteval_wymum(consteval_wyr8(p) ^ seed ^ _wyp0, consteval_wyr8(p + 8) ^ seed ^ _wyp1);
        see1 = consteval_wymum(consteval_wyr8(p + 16) ^ see1 ^ _wyp2, consteval_wyr8(p + i - 8) ^ see1 ^ _wyp3);
    }
    return consteval_wymum(seed ^ see1, len ^ _wyp4);
}

namespace Ichor {
    template<typename INTERFACE_TYPENAME>
    [[nodiscard]] consteval auto typeName() {
        constexpr std::string_view result = __PRETTY_FUNCTION__;
        constexpr std::string_view templateStr = "INTERFACE_TYPENAME = ";

        constexpr size_t bpos = result.find(templateStr) + templateStr.size(); //find begin pos after INTERFACE_TYPENAME = entry
        if constexpr (result.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_:<>") == std::string_view::npos) {
            constexpr size_t len = result.length() - bpos;

            static_assert(!result.substr(bpos, len).empty(), "Cannot infer type name in function call");

            return result.substr(bpos, len);
        } else {
            constexpr size_t len = result.substr(bpos).find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_:<>");

            static_assert(!result.substr(bpos, len).empty(), "Cannot infer type name in function call");

            return result.substr(bpos, len);
        }
    }

    template<typename INTERFACE_TYPENAME>
    [[nodiscard]] consteval auto typeNameHash() {
        std::string_view name = typeName<INTERFACE_TYPENAME>();
        return consteval_wyhash(&name[0], name.size(), 0);
    }
}