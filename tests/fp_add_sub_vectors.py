"""Exact full-state FADD/FSUB oracle, independent of the integer C emitter."""
from fractions import Fraction
import random
from fp_sqrt_abd_vectors import constants, unpack, nan_result
from fp_scalar_step_vectors import rounded


def reference(sub, a_bits, b_bits, width, mode):
    f,bias,hidden,exp,sign=constants(width)
    a,b=unpack(a_bits,width,mode&4),unpack(b_bits,width,mode&4)
    flags=a[2]|b[2]
    nan=nan_result(a,b,width,mode&8)
    if nan:return nan[0],flags|nan[1]
    sa,sb=bool(a_bits&sign),bool(b_bits&sign)^bool(sub)
    if a[0]=="inf" or b[0]=="inf":
        if a[0]==b[0] and sa!=sb:return exp|(hidden>>1),flags|1
        return exp|(sign if (sa if a[0]=="inf" else sb) else 0),flags
    value=Fraction(a[1]+(-b[1] if sub else b[1]),1<<(bias+f-1))
    if not value:
        negative=sa if a[1]==b[1]==0 and sa==sb else mode&3==2
        return sign if negative else 0,flags
    result,extra=rounded(value,width,mode)
    return result,flags|extra


def main():
    assert reference(0,0x3f800000,0x33800000,4,0)==(0x3f800000,16)
    assert reference(1,0,0x80000000,4,0)==(0,0)
    assert reference(0,0x80000000,0x80000000,4,0)==(0x80000000,0)
    print("static const struct {unsigned sub,width,mode;uint64_t a,b,result,flags,same,sameflags;} vectors[]={")
    rng=random.Random(0xc7615a4bc0928ed1)
    for width in (4,8):
        f,bias,hidden,exp,sign=constants(width)
        one=bias<<f
        vals=[0,1,hidden-1,hidden,hidden+1,one-1,one,one+1,exp-1,exp,exp|1,exp|(hidden>>1)|17]
        vals += [v|sign for v in vals]
        pairs=[(a,b) for a in vals for b in vals]
        pairs += [(rng.getrandbits(width*8),rng.getrandbits(width*8)) for _ in range(512)]
        pairs += [(one,one-(f<<f)),(one|sign,one-(f<<f))]
        for sub in range(2):
            for a,b in pairs:
                for mode in range(16):
                    r,flags=reference(sub,a,b,width,mode)
                    same,sf=reference(sub,a,a,width,mode)
                    print(f"{{{sub},{width},{mode},UINT64_C(0x{a:x}),UINT64_C(0x{b:x}),UINT64_C(0x{r:x}),{flags},UINT64_C(0x{same:x}),{sf}}},")
    print("};")


if __name__=="__main__":main()
