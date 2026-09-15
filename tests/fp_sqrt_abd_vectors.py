"""Emit synthetic C vectors using Python arbitrary-precision integer arithmetic.

No host floating point is used. Finite values are exact integer multiples of
the format's smallest subnormal; square roots use math.isqrt on that full
integer representation. This is independent of the emitter's bounded-width
significand/guard-bit algorithms. Run to a local fp_sqrt_abd_vectors.h file.
"""
import math
import random


def constants(width):
    f, bias = (23, 127) if width == 4 else (52, 1023)
    hidden = 1 << f
    exp = ((bias * 2 + 1) << f)
    return f, bias, hidden, exp, 1 << (width * 8 - 1)


def unpack(bits, width, fz):
    f, bias, hidden, exp, sign = constants(width)
    frac = bits & (hidden - 1)
    field = (bits & exp) >> f
    if field == bias * 2 + 1:
        return ("snan" if frac and not (frac & (hidden >> 1)) else
                "qnan" if frac else "inf"), bits, 0
    if not field and frac and fz:
        return "finite", 0, 128
    units = ((frac | hidden) << (field - 1)) if field else frac
    return "finite", -units if bits & sign else units, 0


def nan_result(a, b, width, dn):
    _, _, hidden, exp, _ = constants(width)
    for kind in ("snan", "qnan"):
        for value in (a, b):
            if value[0] == kind:
                return exp | (hidden >> 1) if dn else value[1] | (hidden >> 1), int(kind == "snan")
    return None


def pack_units(units, width):
    f, _, hidden, _, _ = constants(width)
    if units < hidden:
        return units
    shift = units.bit_length() - (f + 1)
    assert not units & ((1 << shift) - 1)
    return ((shift + 1) << f) | ((units >> shift) - hidden)


def reference(kind, a_bits, b_bits, width, mode):
    f, bias, hidden, exp, sign = constants(width)
    rm, fz, dn = mode & 3, bool(mode & 4), bool(mode & 8)
    a = unpack(a_bits, width, fz)
    b = unpack(b_bits, width, fz) if kind else ("finite", 0, 0)
    flags = a[2] | b[2]
    nan = nan_result(a, b, width, dn)
    if nan:
        result, extra = nan
        return result if not kind else result & ~sign, flags | extra
    if not kind:
        if a[0] == "finite" and a[1] == 0:
            return a_bits & sign, flags
        if a_bits & sign:
            return exp | (hidden >> 1), flags | 1
        if a[0] == "inf":
            return exp, flags
        # x is an integer in units 2^(1-bias-f). Scale its square root
        # into the same integer lattice before finding adjacent FP values.
        radicand = a[1] << (bias + f - 1)
        root = math.isqrt(radicand)
        shift = max(0, root.bit_length() - f - 1)
        step = 1 << shift
        floor = (root >> shift) << shift
        inexact = floor * floor != radicand
        up = inexact and (rm == 1 or (rm == 0 and
             (4 * radicand > (2 * floor + step) ** 2 or
              (4 * radicand == (2 * floor + step) ** 2 and (floor >> shift) & 1))))
        return pack_units(floor + (step if up else 0), width), flags | (16 if inexact else 0)
    if a[0] == "inf" or b[0] == "inf":
        if a[0] == b[0] and not ((a_bits ^ b_bits) & sign):
            return exp | (hidden >> 1), flags | 1
        return exp, flags
    difference = a[1] - b[1]
    magnitude = abs(difference)
    if not magnitude:
        return 0, flags
    if magnitude < hidden and fz:
        return 0, flags | 8
    shift = max(0, magnitude.bit_length() - f - 1)
    step = 1 << shift
    floor, remainder = divmod(magnitude, step)
    up = remainder and ((rm == 0 and (2 * remainder > step or
                         (2 * remainder == step and floor & 1))) or
                        (rm == 1 and difference > 0) or
                        (rm == 2 and difference < 0))
    rounded = (floor + bool(up)) * step
    if rounded.bit_length() > f + 1 + (bias * 2 - 1):
        infinity = rm == 0 or (rm == 1 and difference > 0) or (rm == 2 and difference < 0)
        return exp if infinity else exp - 1, flags | 20
    return pack_units(rounded, width), flags | (16 if remainder else 0)


def main():
    # Fixed IEEE bit patterns / exact powers-of-two anchors also check this
    # oracle, especially directed rounding before FABD clears the result sign.
    assert reference(0, 0x40000000, 0, 4, 0) == (0x3FB504F3, 16)
    assert reference(0, 0x40000000, 0, 4, 1) == (0x3FB504F4, 16)
    assert reference(0, 0x4000000000000000, 0, 8, 0) == (0x3FF6A09E667F3BCD, 16)
    assert reference(0, 0x4000000000000000, 0, 8, 2) == (0x3FF6A09E667F3BCC, 16)
    assert reference(1, 0x3F800000, 0xB3800000, 4, 0) == (0x3F800000, 16)
    assert reference(1, 0x3F800000, 0xB3800000, 4, 1) == (0x3F800001, 16)
    assert reference(1, 0xBF800000, 0x33800000, 4, 1) == (0x3F800000, 16)
    assert reference(1, 0xBF800000, 0x33800000, 4, 2) == (0x3F800001, 16)
    assert reference(1, 0x00800001, 0x00800000, 4, 4) == (0, 8)
    assert reference(0, 1, 0, 8, 0) == (0x1E60000000000000, 0)
    print("static const struct { unsigned kind,width,mode; uint64_t a,b,result,flags,same,sameflags; } vectors[]={")
    rng = random.Random(0x5A17ABDF)
    count = 0
    for width in (4, 8):
        f, bias, hidden, exp, sign = constants(width)
        one = bias << f
        values = [0, 1, hidden - 1, hidden, hidden + 1, one - 1, one, one + 1,
                  one + hidden, exp - 1, exp, exp | 1, exp | (hidden >> 1) | 17]
        values += [v | sign for v in values]
        pairs = [(a, b) for a in values for b in values]
        pairs += [(rng.getrandbits(width * 8), rng.getrandbits(width * 8)) for _ in range(384)]
        # Adjacent values and large exponent gaps stress cancellation/sticky bits.
        for _ in range(128):
            a = rng.randrange(exp)
            pairs.extend([(a, a + 1), (a | sign, a + 1), (a, 1), (1, a)])
        for kind in (0, 1):
            inputs = pairs if kind else [(a, 0) for a in values + [rng.getrandbits(width * 8) for _ in range(512)]]
            for a, b in inputs:
                for mode in range(16):
                    result, flags = reference(kind, a, b, width, mode)
                    same, sameflags = reference(kind, a, a, width, mode)
                    print(f"{{{kind},{width},{mode},UINT64_C(0x{a:x}),UINT64_C(0x{b:x}),UINT64_C(0x{result:x}),{flags},UINT64_C(0x{same:x}),{sameflags}}},")
                    count += 1
    print("};")
    print(f"/* {count} exact synthetic oracle cases */")


if __name__ == "__main__":
    main()
