/* Preserve cached full-width values across read-only word accesses using
 * real Hacktarux emitters and register-cache code.
 * No decoder, interpreter, ROM, device initialization, or emulator runtime.
 */
#define _GNU_SOURCE
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "r4300/r4300.h"
#include "r4300/recomph.h"
#include "r4300/hacktarux_dynarec/assemble.h"

#if !defined(__linux__) || !defined(__x86_64__)
#error "This private harness requires Linux x86_64 and the SysV ABI"
#endif

/* Compiler state normally supplied by recomp.c/r4300.c. */
int64_t reg[32], hi, lo;
struct precomp_instr *dst;
struct precomp_block *dst_block;
uint32_t src;
int code_length, max_code_length;
unsigned char **inst_pointer;

enum { CODE_CAPACITY = 65536, INSTR_CAPACITY = 32 };
static unsigned char *code;
static struct precomp_instr decoded[INSTR_CAPACITY];
static struct precomp_block block;
static unsigned instruction_count, chains, failures;
static struct {
    uint64_t gpr[32], hi, lo;
} expected;

/* A real ABI boundary: all SysV callee-saved GPRs survive generated code.
 * Entry RSP is 8 mod 16; six pushes and sub $8 align it before CALL.
 * R15 must be reg itself, exactly as assemble.h::rel_r15_offset assumes.
 * The selected emitters never adjust RSP or call back into C.
 */
extern void execute_native(void *entry, void *register_base);
__asm__(
    ".text\n"
    ".globl execute_native\n"
    ".type execute_native,@function\n"
    "execute_native:\n"
    "push %rbx\n"
    "push %rbp\n"
    "push %r12\n"
    "push %r13\n"
    "push %r14\n"
    "push %r15\n"
    "sub $8,%rsp\n"
    "mov %rsi,%r15\n"
    "call *%rdi\n"
    "add $8,%rsp\n"
    "pop %r15\n"
    "pop %r14\n"
    "pop %r13\n"
    "pop %r12\n"
    "pop %rbp\n"
    "pop %rbx\n"
    "ret\n"
    ".size execute_native,.-execute_native\n");

/* assemble.h retains a growth reference even though these short chains fit.
 * Fail loudly instead of silently replacing its executable buffer allocator.
 */
void *realloc_exec(void *ptr, size_t oldsize, size_t newsize)
{
    (void)ptr; (void)oldsize; (void)newsize;
    fputs("Harness code buffer exhausted\n", stderr);
    abort();
}

/* DEBUG enables the real assembler's R15 displacement diagnostics. */
void DebugMessage(int level, const char *format, ...)
{
    va_list args;
    (void)level;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    abort();
}

static void seed(unsigned index, uint64_t value)
{
    expected.gpr[index] = value;
    memcpy(&reg[index], &value, sizeof(value));
}

static void begin_chain(void)
{
    unsigned i;
    for (i = 0; i < 32; ++i)
        seed(i, i ? UINT64_C(0x13579bdf24680000) + i : 0);
    expected.hi = UINT64_C(0x0123456789abcdef);
    expected.lo = UINT64_C(0xfedcba9876543210);
    memcpy(&hi, &expected.hi, sizeof(hi));
    memcpy(&lo, &expected.lo, sizeof(lo));
    memset(decoded, 0, sizeof(decoded));
    memset(&block, 0, sizeof(block));
    block.block = decoded;
    block.code = code;
    dst_block = &block;
    dst = decoded;
    instruction_count = 0;
    code_length = 0;
    max_code_length = CODE_CAPACITY;
    inst_pointer = &code;
    init_cache(decoded);
}

static void next_instruction(uint32_t opcode)
{
    /* Keep a final sentinel slot for free_all_registers' metadata writes. */
    if (instruction_count >= INSTR_CAPACITY - 1)
        abort();
    dst = &decoded[instruction_count++];
    dst->addr = UINT32_C(0x80000000) + 4 * (instruction_count - 1);
    dst->local_addr = code_length;
    src = opcode;
}

static void emit_i(void (*generate)(void), unsigned op, unsigned rt,
                   unsigned rs, int16_t immediate)
{
    next_instruction((op << 26) | (rs << 21) | (rt << 16) |
                     (uint16_t)immediate);
    dst->f.i.rs = &reg[rs];
    dst->f.i.rt = &reg[rt];
    dst->f.i.immediate = immediate;
    generate();
}

static void emit_r(void (*generate)(void), unsigned function, unsigned rd,
                   unsigned rs, unsigned rt, unsigned sa)
{
    next_instruction((rs << 21) | (rt << 16) | (rd << 11) | (sa << 6) | function);
    dst->f.r.rs = &reg[rs];
    dst->f.r.rt = &reg[rt];
    dst->f.r.rd = &reg[rd];
    dst->f.r.sa = sa;
    dst->f.r.nrd = rd;
    generate();
}

static int compare(const char *name, const char *which, uint64_t actual,
                   uint64_t wanted)
{
    if (actual == wanted)
        return 0;
    fprintf(stderr, "%s: %s got %016" PRIx64 " expected %016" PRIx64 "\n",
            name, which, actual, wanted);
    return 1;
}

static void end_chain(const char *name)
{
    unsigned i;
    int bad = 0;
    char label[16];
    dst = &decoded[instruction_count];
    free_all_registers();
    put8(0xc3); /* RET; all architectural writes are emitted before this. */
    block.code_length = code_length;
    __builtin___clear_cache((char *)code, (char *)code + code_length);
    execute_native(code, reg);
    for (i = 0; i < 32; ++i) {
        snprintf(label, sizeof(label), "GPR[%u]", i);
        bad |= compare(name, label, (uint64_t)reg[i], expected.gpr[i]);
    }
    bad |= compare(name, "HI", (uint64_t)hi, expected.hi);
    bad |= compare(name, "LO", (uint64_t)lo, expected.lo);
    ++chains;
    failures += !!bad;
}

/* Exact dirty64 -> read-only32 -> read64 regression, and its flush-only
 * variant. a0=$4, t0=$8, t1=$9, t2=$10. No intermediate cache flush.
 */
static void dirty64_read32(int consume)
{
    begin_chain();
    seed(4, UINT64_C(0x12345678));
    emit_r(gendsll32, 0x3c, 8, 0, 4, 0); /* DSLL32 t0,a0,0 */
    expected.gpr[8] = UINT64_C(0x1234567800000000);
    emit_r(gensll, 0x00, 9, 0, 8, 0);    /* SLL t1,t0,0 */
    expected.gpr[9] = 0;
    if (consume) {
        emit_r(gendsrl32, 0x3e, 10, 0, 8, 0); /* DSRL32 t2,t0,0 */
        expected.gpr[10] = UINT64_C(0x12345678);
    }
    end_chain(consume ? "dirty64/read32/read64" : "dirty64/read32/flush");
}

static void clean64_read32(void)
{
    begin_chain();
    seed(8, UINT64_C(0x1234567880000001));
    /* DSRL32 first loads t0 as a clean64 cache entry. SLL reads its low word
     * into a separate destination; a second DSRL32 must still see high32.
     */
    emit_r(gendsrl32, 0x3e, 11, 0, 8, 0);
    expected.gpr[11] = UINT64_C(0x12345678);
    emit_r(gensll, 0x00, 9, 0, 8, 0);
    expected.gpr[9] = UINT64_C(0xffffffff80000001);
    emit_r(gendsrl32, 0x3e, 10, 0, 8, 0);
    expected.gpr[10] = UINT64_C(0x12345678);
    end_chain("clean64/read32/read64");
}

static void fresh32_read64(void)
{
    begin_chain();
    seed(8, UINT64_C(0x1234567880000001));
    /* No preload: SLL's read-only32 allocation must preserve the full value
     * for the following 64-bit consumer, including on a cache miss.
     */
    emit_r(gensll, 0x00, 9, 0, 8, 0);      /* SLL t1,t0,0 */
    expected.gpr[9] = UINT64_C(0xffffffff80000001);
    emit_r(gendsrl32, 0x3e, 10, 0, 8, 0); /* DSRL32 t2,t0,0 */
    expected.gpr[10] = UINT64_C(0x12345678);
    end_chain("fresh32/read64");
}

static void fresh_manual32_same64(void)
{
    begin_chain();
    seed(4, UINT64_C(0x1234567800000003));
    /* The uncached a0 is both count and value. Its manual ECX count load
     * occurs before the emitter requests the same operand as a full64 value.
     */
    emit_r(gendsllv, 0x14, 8, 4, 4, 0); /* DSLLV t0,a0,a0 */
    expected.gpr[8] = UINT64_C(0x91a2b3c000000018);
    end_chain("fresh manual32/same64 DSLLV");
}

static void same_register_word_write(void)
{
    begin_chain();
    seed(8, UINT64_C(0x1234567880000001));
    /* Preload t0 as clean64, then actually overwrite it with a word result.
     * Unlike a read-only use, the _w allocation must mark that result as32.
     */
    emit_r(gendsrl32, 0x3e, 11, 0, 8, 0); /* DSRL32 t3,t0,0 */
    expected.gpr[11] = UINT64_C(0x12345678);
    emit_r(gensll, 0x00, 8, 0, 8, 0);     /* SLL t0,t0,0 */
    expected.gpr[8] = UINT64_C(0xffffffff80000001);
    emit_r(gendsrl32, 0x3e, 10, 0, 8, 0); /* DSRL32 t2,t0,0 */
    expected.gpr[10] = UINT64_C(0x00000000ffffffff);
    end_chain("cached64/same-register word write/read64");
}

static void lui_daddiu(void)
{
    begin_chain();
    emit_i(genlui, 0x0f, 8, 0, -32767); /* 0x8001 */
    emit_i(gendaddiu, 0x19, 8, 8, -7);
    expected.gpr[8] = UINT64_C(0xffffffff8000fff9);
    end_chain("negative LUI/same-register DADDIU");
}

static void hilo_moves(void)
{
    begin_chain();
    seed(4, UINT64_C(0x8123456789abcdef));
    seed(5, UINT64_C(0x76543210fedcba98));
    emit_r(genmthi, 0x11, 0, 4, 0, 0);
    expected.hi = expected.gpr[4];
    emit_r(genmtlo, 0x13, 0, 5, 0, 0);
    expected.lo = expected.gpr[5];
    emit_r(genmfhi, 0x10, 8, 0, 0, 0);
    expected.gpr[8] = expected.hi;
    emit_r(genmflo, 0x12, 9, 0, 0, 0);
    expected.gpr[9] = expected.lo;
    end_chain("MTHI/MTLO/MFHI/MFLO");
}

/* Unsigned arithmetic gives an independent, defined arithmetic-right oracle. */
static uint64_t shifted(uint64_t value, unsigned amount, unsigned direction)
{
    amount &= 63;
    if (!amount)
        return value;
    if (direction == 0)
        return value << amount;
    if (direction == 1)
        return value >> amount;
    return (value >> amount) | ((UINT64_C(0) - (value >> 63)) << (64 - amount));
}

static void ds_shifts(void)
{
    static const struct {
        void (*generate)(void);
        unsigned function, direction, extra;
    } immediate[] = {
        {gendsll, 0x38, 0, 0}, {gendsrl, 0x3a, 1, 0}, {gendsra, 0x3b, 2, 0},
        {gendsll32, 0x3c, 0, 32}, {gendsrl32, 0x3e, 1, 32}, {gendsra32, 0x3f, 2, 32}
    }, variable[] = {
        {gendsllv, 0x14, 0, 0}, {gendsrlv, 0x16, 1, 0}, {gendsrav, 0x17, 2, 0}
    };
    static const unsigned amounts[] = {0, 1, 31, 32, 63, 64};
    /* distinct, rd=rt, rd=rs, rs=rt, all equal */
    static const unsigned aliases[][3] = {{8,4,5}, {5,4,5}, {4,4,5}, {8,4,4}, {4,4,4}};
    unsigned op, alias, n;
    char name[80];
    for (op = 0; op < sizeof(immediate) / sizeof(immediate[0]); ++op) {
        for (alias = 0; alias < 2; ++alias) {
            unsigned rd = alias ? 4 : 8;
            begin_chain();
            seed(4, UINT64_C(0x8123456789abcdef));
            emit_r(immediate[op].generate, immediate[op].function, rd, 0, 4, 7);
            expected.gpr[rd] = shifted(expected.gpr[4], 7 + immediate[op].extra,
                                       immediate[op].direction);
            /* Consume the result while it is still cached. */
            emit_i(gendaddiu, 0x19, 10, rd, 3);
            expected.gpr[10] = expected.gpr[rd] + 3;
            snprintf(name, sizeof(name), "DS immediate op=%02x alias=%u", immediate[op].function, alias);
            end_chain(name);
        }
    }
    for (op = 0; op < sizeof(variable) / sizeof(variable[0]); ++op) {
        for (alias = 0; alias < sizeof(aliases) / sizeof(aliases[0]); ++alias) {
            for (n = 0; n < sizeof(amounts) / sizeof(amounts[0]); ++n) {
                unsigned rd = aliases[alias][0], rs = aliases[alias][1], rt = aliases[alias][2];
                uint64_t result;
                begin_chain();
                seed(4, UINT64_C(0x1234567800000000) | amounts[n]);
                seed(5, UINT64_C(0x8123456789abcdef));
                /* Dirty64 writers exercise the manual ECX mapping path. */
                emit_i(gendaddiu, 0x19, 4, 4, 0);
                emit_i(gendaddiu, 0x19, 5, 5, 0);
                result = shifted(expected.gpr[rt], (unsigned)expected.gpr[rs], variable[op].direction);
                emit_r(variable[op].generate, variable[op].function, rd, rs, rt, 0);
                expected.gpr[rd] = result;
                emit_i(gendaddiu, 0x19, 10, rd, 3);
                expected.gpr[10] = result + 3;
                snprintf(name, sizeof(name), "DS variable op=%02x alias=%u count=%u", variable[op].function, alias, amounts[n]);
                end_chain(name);
            }
        }
    }
}

int main(void)
{
    int64_t hi_offset = (int64_t)(uintptr_t)&hi - (int64_t)(uintptr_t)reg;
    int64_t lo_offset = (int64_t)(uintptr_t)&lo - (int64_t)(uintptr_t)reg;
    if (hi_offset < INT32_MIN || hi_offset > INT32_MAX ||
        lo_offset < INT32_MIN || lo_offset > INT32_MAX) {
        fputs("HI/LO outside R15 disp32 range\n", stderr);
        return 2;
    }
    code = mmap(NULL, CODE_CAPACITY, PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code == MAP_FAILED) {
        perror("mmap executable buffer");
        return 2;
    }
    dirty64_read32(1);
    dirty64_read32(0);
    clean64_read32();
    fresh32_read64();
    fresh_manual32_same64();
    same_register_word_write();
    lui_daddiu();
    hilo_moves();
    ds_shifts();
    if (munmap(code, CODE_CAPACITY) != 0) {
        perror("munmap");
        return 2;
    }
    printf("%u native chains, %u failed\n", chains, failures);
    return failures ? 1 : 0;
}
