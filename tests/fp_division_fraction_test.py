"""Independent exact Fraction oracle for the generated scalar/vector FDIV C.

Run on Linux: python3 tests/fp_division_fraction_test.py
Uses existing GCC tools only; all operands are synthetic random finite values.
"""
from fractions import Fraction
from pathlib import Path
import random
import subprocess
import tempfile


def power(exponent):
    return Fraction(2**exponent) if exponent >= 0 else Fraction(1, 2**-exponent)


def expected(a, b, width, mode, flush):
    frac, bias = (23, 127) if width == 32 else (52, 1023)
    signbit = 1 << (width - 1)
    sign = (a ^ b) & signbit
    exponent_mask = (2 * bias + 1) << frac
    flags = 0

    def decode(value):
        nonlocal flags
        exponent = (value & exponent_mask) >> frac
        fraction = value & ((1 << frac) - 1)
        if not exponent and fraction and flush:
            flags |= 128
            return Fraction(0)
        return Fraction(fraction + ((1 << frac) if exponent else 0)) * power(
            (exponent - bias if exponent else 1 - bias) - frac)

    x, y = decode(a), decode(b)
    if not y:
        return ((exponent_mask | (1 << (frac - 1)), flags | 1) if not x
                else (sign | exponent_mask, flags | 2))
    if not x:
        return sign, flags
    ratio = x / y
    exponent = ratio.numerator.bit_length() - ratio.denominator.bit_length()
    if ratio < power(exponent):
        exponent -= 1
    tiny = exponent < 1 - bias
    if tiny and flush:
        return sign, flags | 8
    quantum = power(max(exponent, 1 - bias) - frac)
    scaled = ratio / quantum
    whole, residual = divmod(scaled.numerator, scaled.denominator)
    if residual:
        flags |= 16 | (8 if tiny else 0)
        increment = (2 * residual > scaled.denominator or
                     (2 * residual == scaled.denominator and whole & 1)) if mode == 0 else (
                         not sign if mode == 1 else bool(sign) if mode == 2 else False)
        whole += bool(increment)
    rounded = whole * quantum
    if rounded >= power(bias + 1):
        infinity = mode == 0 or (mode == 1 and not sign) or (mode == 2 and sign)
        return sign | (exponent_mask if infinity else exponent_mask - 1), flags | 20
    if rounded < power(1 - bias):
        return sign | whole, flags
    exponent = max(exponent, 1 - bias)
    if whole >= (1 << (frac + 1)):
        whole //= 2
        exponent += 1
    return sign | ((exponent + bias) << frac) | (whole & ((1 << frac) - 1)), flags


def main():
    root = Path(__file__).resolve().parents[1]
    rng = random.Random(0xD1A1DE)
    with tempfile.TemporaryDirectory(prefix="fp-div-fraction-") as directory:
        work = Path(directory)
        gen, cfile, binary = work / "gen", work / "test.c", work / "test"
        subprocess.run(["g++", "-std=c++20", "-I" + str(root / "third_party/suyu/src"),
                        str(root / "tests/fp_division_test.cpp"), "-o", str(gen)], check=True)
        generated = subprocess.check_output([str(gen)], text=True)
        with cfile.open("w") as output:
            output.write("#define main hardware_oracle_main\n" + generated + "\n#undef main\nint main(void){\n")
            for width in (32, 64):
                frac, bias = (23, 127) if width == 32 else (52, 1023)
                expmask = (2 * bias + 1) << frac
                for index in range(500):
                    a, b = rng.getrandbits(width), rng.getrandbits(width)
                    if a & expmask == expmask or b & expmask == expmask:
                        continue
                    for mode in range(4):
                        for flush in range(2):
                            result, flags = expected(a, b, width, mode, flush)
                            shape = (0, 2, 3)[index % 3] if width == 32 else (1, 4)[index % 2]
                            output.write(f"test({shape},{index%3},{a}ULL,{b}ULL,{result}ULL,{flags},"
                                         f"{(mode<<22)|(flush<<24)}ULL);\n")
            output.write('printf("%u/%u exact Fraction division checks passed\\n",checks-fail,checks);return fail!=0;}\n')
        subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                        "-frounding-math", "-fsanitize=undefined", str(cfile), "-lm", "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
