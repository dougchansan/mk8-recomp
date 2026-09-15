"""Exact rational oracle for scalar reciprocal steps and register compares."""
from fractions import Fraction
import random
from fp_sqrt_abd_vectors import constants, unpack, nan_result


def rounded(value, width, mode):
    f, bias, hidden, exp, sign = constants(width)
    if not value:
        return sign if mode & 3 == 2 else 0, 0
    neg = value < 0
    value = abs(value)
    numerator, denominator = value.numerator, value.denominator
    e = numerator.bit_length() - denominator.bit_length()
    if e >= 0:
        if numerator < denominator << e:
            e -= 1
    elif numerator << -e < denominator:
        e -= 1
    emin = 1 - bias
    if e < emin and mode & 4:
        return sign if neg else 0, 8
    quantum = max(e, emin) - f
    scaled = value / (1 << quantum) if quantum >= 0 else value * (1 << -quantum)
    floor, rem = divmod(scaled.numerator, scaled.denominator)
    rm = mode & 3
    up = rem and ((rm == 0 and (2 * rem > scaled.denominator or
                    (2 * rem == scaled.denominator and floor & 1))) or
                 (rm == 1 and not neg) or (rm == 2 and neg))
    mant = floor + bool(up)
    flags = (16 if rem else 0) | (8 if rem and e < emin else 0)
    e = max(e, emin)
    if mant >= hidden * 2:
        mant >>= 1
        e += 1
    if e > bias:
        inf = rm == 0 or (rm == 1 and not neg) or (rm == 2 and neg)
        result = exp if inf else exp - 1
        flags |= 20
    else:
        result = ((e + bias) << f if mant >= hidden else 0) | (mant & (hidden - 1))
    return result | (sign if neg else 0), flags


def reference(kind, a_bits, b_bits, width, mode):
    f, bias, hidden, exp, sign = constants(width)
    if kind < 2:
        a_bits ^= sign  # Architectural FPNeg occurs even on a NaN operand.
    a, b = unpack(a_bits, width, mode & 4), unpack(b_bits, width, mode & 4)
    flags = a[2] | b[2]
    if kind >= 2:
        if a[0].endswith("nan") or b[0].endswith("nan"):
            return 0, flags | int(kind != 2 or a[0] == "snan" or b[0] == "snan")
        av = (-1 if a_bits & sign else 1) * (1 << 10000) if a[0] == "inf" else a[1]
        bv = (-1 if b_bits & sign else 1) * (1 << 10000) if b[0] == "inf" else b[1]
        yes = av == bv if kind == 2 else av >= bv if kind == 3 else av > bv
        return (1 << (width * 8)) - 1 if yes else 0, flags
    nan = nan_result(a, b, width, mode & 8)
    if nan:
        return nan[0], flags | nan[1]
    if a[0] == "inf" or b[0] == "inf":
        if (a[0] == "finite" and a[1] == 0) or (b[0] == "finite" and b[1] == 0):
            return ((bias << f) | (hidden >> 1)) if kind else (bias + 1) << f, flags
        return exp | ((a_bits ^ b_bits) & sign), flags
    product = Fraction(a[1] * b[1], 1 << (2 * (bias + f - 1)))
    value = (Fraction(3) + product) / 2 if kind else Fraction(2) + product
    result, extra = rounded(value, width, mode)
    return result, flags | extra


def main():
    assert reference(0, 0x3f800000, 0x40000000, 4, 0) == (0, 0)
    assert reference(0, 0x3f800000, 0x40000000, 4, 2) == (0x80000000, 0)
    assert reference(1, 0x3f800000, 0x3f800000, 4, 0) == (0x3f800000, 0)
    assert reference(0, 0, 0x7f800000, 4, 0) == (0x40000000, 0)
    assert reference(1, 0, 0x7f800000, 4, 0) == (0x3fc00000, 0)
    assert reference(2, 0x7fc00001, 0, 4, 0) == (0, 0)
    assert reference(3, 0x7fc00001, 0, 4, 0) == (0, 1)
    print("static const struct { unsigned kind,width,mode; uint64_t a,b,result,flags,same,sameflags; } vectors[]={")
    rng = random.Random(0xA641F00D)
    count = 0
    for width in (4, 8):
        f, bias, hidden, exp, sign = constants(width)
        one = bias << f
        values = [0, 1, hidden-1, hidden, one-1, one, one+1, one+hidden,
                  one+hidden+(hidden>>1), exp-1, exp, exp|1, exp|(hidden>>1)|17]
        values += [v | sign for v in values]
        pairs = [(a, b) for a in values for b in values]
        pairs += [(rng.getrandbits(width*8), rng.getrandbits(width*8)) for _ in range(256)]
        # Products near constants 2/3 expose fusion and cancellation errors.
        pairs += [(one+a, one+hidden+b) for a in range(-4,5) for b in range(-4,5)]
        pairs += [(one+a, one+hidden+(hidden>>1)+b) for a in range(-4,5) for b in range(-4,5)]
        for kind in range(5):
            for a,b in pairs:
                for mode in range(16):
                    r,fl=reference(kind,a,b,width,mode)
                    same,sfl=reference(kind,a,a,width,mode)
                    print(f"{{{kind},{width},{mode},UINT64_C(0x{a:x}),UINT64_C(0x{b:x}),UINT64_C(0x{r:x}),{fl},UINT64_C(0x{same:x}),{sfl}}},")
                    count += 1
    print("};")
    print(f"/* {count} exact oracle cases */")


if __name__ == "__main__":
    main()
