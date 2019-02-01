#define IMMEDIATE(x) (x & 0x00FFFFFF)
#define SIGN_EXTEND(x) ((x) & 0x00800000 ? (x) | 0xFF000000 : (x))
#define OPCODE(x) (x >> 24)

#define MSB (1 << (8 * sizeof(unsigned int) -1))
#define IS_PRIM(objRef) ((((objRef) -> size) & MSB) == 0)
#define GET_SIZE(objRef) ((objRef)->size & ~BHMSB)
#define GET_REFS(objRef) ((ObjRef *)(objRef)->data)

#define SIZE_INT_ARRAY(a) (sizeof(a)/sizeof(int))

#define BHMSB (MSB | BROKEN_HEART)

#define BROKEN_HEART (1 << (8 * sizeof(unsigned int) -2))
#define HAS_BROKEN_HEART(objRef) (((objRef -> size) & BROKEN_HEART))
#define FORWARD_POINTER(objRef) (objRef -> size & ~BHMSB)