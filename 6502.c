/* Fake6502 CPU emulator core v1.1 *******************
 * (c)2011-2013 Mike Chambers                        *
 *****************************************************/
/* 65C02 instructions by jonathan Foucher */
/* License */
/* https://creativecommons.org/licenses/by-sa/3.0/ */
/* https://codegolf.stackexchange.com/questions/12844/emulate-a-mos-6502-cpu */

#include <stdio.h>
#include <stdint.h>

//externally supplied functions
extern uint8_t read6502(uint16_t address);
extern void write6502(uint16_t address, uint8_t value);

//6502 defines
// #define UNDOCUMENTED //when this is defined, undocumented opcodes are handled.
                     //otherwise, they're simply treated as NOPs.

#define CPU_65C02    // allows 65C02 instructions
//#define NES_CPU      //when this is defined, the binary-coded decimal (BCD)
                     //status flag is not honored by ADC and SBC. the 2A03
                     //CPU in the Nintendo Entertainment System does not
                     //support BCD operation.

#define FLAG_CARRY     0x01
#define FLAG_ZERO      0x02
#define FLAG_INTERRUPT 0x04
#define FLAG_DECIMAL   0x08
#define FLAG_BREAK     0x10
#define FLAG_CONSTANT  0x20
#define FLAG_OVERFLOW  0x40
#define FLAG_SIGN      0x80

#define BASE_STACK     0x100

#define saveaccum(n) a = (uint8_t)((n) & 0x00FF)


//flag modifier macros
#define setcarry() status |= FLAG_CARRY
#define clearcarry() status &= (~FLAG_CARRY)
#define setzero() status |= FLAG_ZERO
#define clearzero() status &= (~FLAG_ZERO)
#define setinterrupt() status |= FLAG_INTERRUPT
#define clearinterrupt() status &= (~FLAG_INTERRUPT)
#define setdecimal() status |= FLAG_DECIMAL
#define cleardecimal() status &= (~FLAG_DECIMAL)
#define setoverflow() status |= FLAG_OVERFLOW
#define clearoverflow() status &= (~FLAG_OVERFLOW)
#define setsign() status |= FLAG_SIGN
#define clearsign() status &= (~FLAG_SIGN)


//flag calculation macros
#define zerocalc(n) {\
    if ((n) & 0x00FF) clearzero();\
        else setzero();\
}

#define signcalc(n) {\
    if ((n) & 0x0080) setsign();\
        else clearsign();\
}

#define carrycalc(n) {\
    if ((n) & 0xFF00) setcarry();\
        else clearcarry();\
}

#define overflowcalc(n, m, o) { /* n = result, m = accumulator, o = memory */ \
    if (((n) ^ (uint16_t)(m)) & ((n) ^ (o)) & 0x0080) setoverflow();\
        else clearoverflow();\
}


//6502 CPU registers
uint16_t pc;
uint8_t sp, a, x, y, status = FLAG_CONSTANT;


//helper variables
uint64_t instructions = 0; //keep track of total instructions executed
uint32_t clockticks6502 = 0, clockgoal6502 = 0;
uint16_t oldpc, ea, reladdr, value, result;
uint8_t opcode, oldstatus;

//a few general functions used by various other functions
void push16(uint16_t pushval) {
    write6502(BASE_STACK + sp, (pushval >> 8) & 0xFF);
    write6502(BASE_STACK + ((sp - 1) & 0xFF), pushval & 0xFF);
    sp -= 2;
}

void push8(uint8_t pushval) {
    write6502(BASE_STACK + sp--, pushval);
}

uint16_t pull16() {
    uint16_t temp16;
    temp16 = read6502(BASE_STACK + ((sp + 1) & 0xFF)) | ((uint16_t)read6502(BASE_STACK + ((sp + 2) & 0xFF)) << 8);
    sp += 2;
    return(temp16);
}

uint8_t pull8() {
    return (read6502(BASE_STACK + ++sp));
}

void reset6502() {
    pc = (uint16_t)read6502(0xFFFC) | ((uint16_t)read6502(0xFFFD) << 8);
    a = 0;
    x = 0;
    y = 0;
    sp = 0xFD;
    status |= FLAG_CONSTANT;
}


static void (*addrtable[256])();
static void (*optable[256])();
uint8_t penaltyop, penaltyaddr;

//addressing mode functions, calculates effective addresses
static void imp() { //implied
}

static void acc() { //accumulator
}

static void imm() { //immediate
    ea = pc++;
}

static void zp() { //zero-page
    ea = (uint16_t)read6502((uint16_t)pc++);
}

static void indzp() { //indirect zero-page
    // get the zero page address to read the address from
    uint16_t zpa = (uint16_t)read6502((uint16_t)pc++);
    // get the effective address from zero page
    ea = (uint16_t)(read6502(zpa) | (read6502((zpa+1) & 0xFF) << 8));
}

static void zpx() { //zero-page,X
    ea = ((uint16_t)read6502((uint16_t)pc++) + (uint16_t)x) & 0xFF; //zero-page wraparound
}

static void zpy() { //zero-page,Y
    ea = ((uint16_t)read6502((uint16_t)pc++) + (uint16_t)y) & 0xFF; //zero-page wraparound
}

static void rel() { //relative for branch ops (8-bit immediate value, sign-extended)
    reladdr = (uint16_t)read6502(pc++);
    if (reladdr & 0x80) reladdr |= 0xFF00;
}

static void rel2() { //relative for bbr (8-bit immediate value, sign-extended)
    reladdr = (uint16_t)read6502(pc+1);
    if (reladdr & 0x80) reladdr |= 0xFF00;
}

static void abso() { //absolute
    ea = (uint16_t)read6502(pc) | ((uint16_t)read6502(pc+1) << 8);
    pc += 2;
}

static void absx() { //absolute,X
    uint16_t startpage;
    ea = ((uint16_t)read6502(pc) | ((uint16_t)read6502(pc+1) << 8));
    startpage = ea & 0xFF00;
    ea += (uint16_t)x;

    if (startpage != (ea & 0xFF00)) { //one cycle penlty for page-crossing on some opcodes
        penaltyaddr = 1;
    }

    pc += 2;
}

static void absy() { //absolute,Y
    uint16_t startpage;
    ea = ((uint16_t)read6502(pc) | ((uint16_t)read6502(pc+1) << 8));
    startpage = ea & 0xFF00;
    ea += (uint16_t)y;

    if (startpage != (ea & 0xFF00)) { //one cycle penlty for page-crossing on some opcodes
        penaltyaddr = 1;
    }

    pc += 2;
}

static void ind() { //indirect
    uint16_t eahelp/*, eahelp2*/;
    eahelp = (uint16_t)read6502(pc) | (uint16_t)((uint16_t)read6502(pc+1) << 8);
    //eahelp2 = (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF); //replicate 6502 page-boundary wraparound bug
    ea = (uint16_t)read6502(eahelp) | ((uint16_t)read6502(eahelp +1) << 8);
    pc += 2;
}

static void aindx() { //indirect
    uint16_t eahelp;
    eahelp = (uint16_t)read6502(pc) | (uint16_t)((uint16_t)read6502(pc+1) << 8);

    ea = (uint16_t)read6502(eahelp+x) | ((uint16_t)read6502(eahelp +1 + x) << 8);
    pc += 2;
}

static void indx() { // (indirect,X)
    uint16_t eahelp;
    eahelp = (uint16_t)(((uint16_t)read6502(pc++) + (uint16_t)x) & 0xFF); //zero-page wraparound for table pointer
    ea = (uint16_t)read6502(eahelp & 0x00FF) | ((uint16_t)read6502((eahelp+1) & 0x00FF) << 8);
}

static void indy() { // (indirect),Y
    uint16_t eahelp, eahelp2, startpage;
    eahelp = (uint16_t)read6502(pc++);
    eahelp2 = (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF); //zero-page wraparound
    ea = (uint16_t)read6502(eahelp) | ((uint16_t)read6502(eahelp2) << 8);
    startpage = ea & 0xFF00;
    ea += (uint16_t)y;

    if (startpage != (ea & 0xFF00)) { //one cycle penlty for page-crossing on some opcodes
        penaltyaddr = 1;
    }
}

static uint16_t getvalue() {
    if (addrtable[opcode] == acc) return((uint16_t)a);
        else return((uint16_t)read6502(ea));
}

static void putvalue(uint16_t saveval) {
    if (addrtable[opcode] == acc) a = (uint8_t)(saveval & 0x00FF);
        else write6502(ea, (saveval & 0x00FF));
}


//instruction handler functions
static void adc() {
    penaltyop = 1;
    value = getvalue();
    
    #ifndef NES_CPU
    if (status & FLAG_DECIMAL) {
        uint16_t s = 0;
        uint16_t ln = (uint16_t)(a & 0xF) + (uint16_t)(value & 0xF) + (uint16_t)(status & FLAG_CARRY);
        if (ln > 9) {
            ln = 0x10 | ((ln + 6) & 0xf);
        }
        uint16_t hn = (uint16_t)(a & 0xf0) + (uint16_t)(value & 0xf0);
        s = hn + (uint16_t)ln;

            
        if (s >= 160) {
            status |= FLAG_CARRY;
            if (((status & FLAG_OVERFLOW) != 0) && (s >= 0x180)) {
                status &= !FLAG_OVERFLOW;
            }
            s += 0x60;
        } else {
            status &= !FLAG_CARRY;
            if ((status & FLAG_OVERFLOW) != 0 && (s < 0x80)) {
                status &= !FLAG_OVERFLOW;
            }
        }
        result = (uint8_t)(s & 0xFF);
        saveaccum(result);
        zerocalc(result);
        signcalc(result);
        clockticks6502++;
    } else {
    #endif
        result = (uint16_t)a + value + (uint16_t)(status & FLAG_CARRY);

        carrycalc(result);

        zerocalc(result);
        signcalc(result);

        overflowcalc(result, a, value);

        saveaccum(result);
    #ifndef NES_CPU
    }
    #endif
    

    
}

static void and() {
    penaltyop = 1;
    value = getvalue();
    result = (uint16_t)a & value;

    zerocalc(result);
    signcalc(result);

    saveaccum(result);
}

static void asl() {
    value = getvalue();
    result = value << 1;

    carrycalc(result);
    zerocalc(result);
    signcalc(result);

    putvalue(result);
}

static void bcc() {
    if ((status & FLAG_CARRY) == 0) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void bcs() {
    if ((status & FLAG_CARRY) == FLAG_CARRY) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void beq() {
    if ((status & FLAG_ZERO) == FLAG_ZERO) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void bit() {
    value = getvalue();
    result = (uint16_t)a & (uint16_t)value;

    zerocalc(result);
    
#ifdef CPU_65C02
    // BIt immediate does not affect N nor V flags
    if (addrtable[opcode] != imm) {
#endif
        status = (status & 0x3F) | (uint8_t)(value & 0xC0);
#ifdef CPU_65C02
    }
#endif
    
}

static void bmi() {
    if ((status & FLAG_SIGN) == FLAG_SIGN) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void bne() {
    if ((status & FLAG_ZERO) == 0) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void bpl() {
    if ((status & FLAG_SIGN) == 0) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void brk() {
    pc++;
    push16(pc); //push next instruction address onto stack
    push8(status | FLAG_BREAK); //push CPU status to stack
    setinterrupt(); //set interrupt flag
#ifdef CPU_65C02
    cleardecimal();
#endif
    pc = (uint16_t)read6502(0xFFFE) | ((uint16_t)read6502(0xFFFF) << 8);
}

static void bvc() {
    if ((status & FLAG_OVERFLOW) == 0) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void bvs() {
    if ((status & FLAG_OVERFLOW) == FLAG_OVERFLOW) {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }
}

static void clc() {
    clearcarry();
}

static void cld() {
    cleardecimal();
}

static void cli() {
    clearinterrupt();
}

static void clv() {
    clearoverflow();
}

static void cmp() {
    penaltyop = 1;
    value = getvalue();
    result = (uint16_t)a - value;

    if (a >= (uint8_t)(value & 0x00FF)) setcarry();
        else clearcarry();
    if (a == (uint8_t)(value & 0x00FF)) setzero();
        else clearzero();
    signcalc(result);
}

static void cpx() {
    value = getvalue();
    result = (uint16_t)x - value;

    if (x >= (uint8_t)(value & 0x00FF)) setcarry();
        else clearcarry();
    if (x == (uint8_t)(value & 0x00FF)) setzero();
        else clearzero();
    signcalc(result);
}

static void cpy() {
    value = getvalue();
    result = (uint16_t)y - value;

    if (y >= (uint8_t)(value & 0x00FF)) setcarry();
        else clearcarry();
    if (y == (uint8_t)(value & 0x00FF)) setzero();
        else clearzero();
    signcalc(result);
}

static void dec() {
    value = getvalue();
    result = value - 1;

    zerocalc(result);
    signcalc(result);

    putvalue(result);
}

static void dex() {
    x--;

    zerocalc(x);
    signcalc(x);
}

static void dey() {
    y--;

    zerocalc(y);
    signcalc(y);
}

static void eor() {
    penaltyop = 1;
    value = getvalue();
    result = (uint16_t)a ^ value;

    zerocalc(result);
    signcalc(result);

    saveaccum(result);
}

static void inc() {
    value = getvalue();
    result = value + 1;

    zerocalc(result);
    signcalc(result);

    putvalue(result);
}

static void inx() {
    x++;

    zerocalc(x);
    signcalc(x);
}

static void iny() {
    y++;

    zerocalc(y);
    signcalc(y);
}

static void jmp() {
    pc = ea;
}

static void jsr() {
    push16(pc - 1);
    pc = ea;
}

static void lda() {
    penaltyop = 1;
    value = getvalue();
    a = (uint8_t)(value & 0x00FF);

    zerocalc(a);
    signcalc(a);
}

static void ldx() {
    penaltyop = 1;
    value = getvalue();
    x = (uint8_t)(value & 0x00FF);

    zerocalc(x);
    signcalc(x);
}

static void ldy() {
    penaltyop = 1;
    value = getvalue();
    y = (uint8_t)(value & 0x00FF);

    zerocalc(y);
    signcalc(y);
}

static void lsr() {
    value = getvalue();
    result = value >> 1;

    if (value & 1) setcarry();
        else clearcarry();
    zerocalc(result);
    signcalc(result);

    putvalue(result);
}

static void nop() {
    switch (opcode) {
        case 0x1C:
        case 0x3C:
        case 0x5C:
        case 0x7C:
        case 0xDC:
        case 0xFC:
            penaltyop = 1;
            break;
    }
}

static void ora() {
    penaltyop = 1;
    value = getvalue();
    result = (uint16_t)a | value;

    zerocalc(result);
    signcalc(result);

    saveaccum(result);
}

static void pha() {
    push8(a);
}

static void php() {
    push8(status | FLAG_BREAK);
}

static void pla() {
    a = pull8();

    zerocalc(a);
    signcalc(a);
}

static void plp() {
    status = pull8() | FLAG_CONSTANT;
}

static void rol() {
    value = getvalue();
    result = (value << 1) | (status & FLAG_CARRY);

    carrycalc(result);
    zerocalc(result);
    signcalc(result);

    putvalue(result);
}

static void ror() {
    value = getvalue();
    result = (value >> 1) | ((status & FLAG_CARRY) << 7);

    if (value & 1) setcarry();
        else clearcarry();
    zerocalc(result);
    signcalc(result);

    putvalue(result);
}

static void rti() {
    status = pull8();
    value = pull16();
    pc = value;
}

static void rts() {
    value = pull16();
    pc = value + 1;
}

static void sbc() {
    penaltyop = 1;
    
    

    #ifndef NES_CPU
    if (status & FLAG_DECIMAL) {
        value = getvalue();
        uint16_t carry = (status & FLAG_CARRY) ? 0 : 1;
        uint16_t low1 = a & 0x0F;
        uint16_t low2 = value & 0x0F;

        uint16_t sublow;
        if (low1 >= (low2 + carry)) {
            sublow = low1 - (low2 + carry);
            carry = 0;
        } else {
            sublow = 10 + low1 - (low2 + carry);
            carry = 1;
        }
        uint16_t hi1 = a >> 4;
        uint16_t hi2 = value >> 4;
        uint16_t subhi;
        if (hi1 >= (hi2 + carry)) {
            subhi = hi1 - (hi2 + carry);
            carry = 1;
        } else {
            subhi = 10 + hi1 - (hi2 + carry);
            carry = 0;
        }
        
        result = subhi << 4 | sublow;

        if (carry) {
            status |= FLAG_CARRY;
        } else {
            status &= ~FLAG_CARRY;
        }
    
        clockticks6502++;
    } else {
    #endif
        value = getvalue() ^ 0x00FF;
        result = (uint16_t)a + value + (uint16_t)(status & FLAG_CARRY);

        carrycalc(result);
        overflowcalc(result, a, value);
    #ifndef NES_CPU
    }
    #endif

    saveaccum(result);
    zerocalc(result);
    signcalc(result);
}

static void sec() {
    setcarry();
}

static void sed() {
    setdecimal();
}

static void sei() {
    setinterrupt();
}

static void sta() {
    putvalue(a);
}

static void stx() {
    putvalue(x);
}

static void sty() {
    putvalue(y);
}

static void tax() {
    x = a;

    zerocalc(x);
    signcalc(x);
}

static void tay() {
    y = a;

    zerocalc(y);
    signcalc(y);
}

static void tsx() {
    x = sp;

    zerocalc(x);
    signcalc(x);
}

static void txa() {
    a = x;

    zerocalc(a);
    signcalc(a);
}

static void txs() {
    sp = x;
}

static void tya() {
    a = y;

    zerocalc(a);
    signcalc(a);
}


//undocumented instructions
#ifdef UNDOCUMENTED
    static void lax() {
        lda();
        ldx();
    }

    static void sax() {
        sta();
        stx();
        putvalue(a & x);
        if (penaltyop && penaltyaddr) clockticks6502--;
    }

    static void dcp() {
        dec();
        cmp();
        if (penaltyop && penaltyaddr) clockticks6502--;
    }

    static void isb() {
        inc();
        sbc();
        if (penaltyop && penaltyaddr) clockticks6502--;
    }

    static void slo() {
        asl();
        ora();
        if (penaltyop && penaltyaddr) clockticks6502--;
    }

    static void rla() {
        rol();
        and();
        if (penaltyop && penaltyaddr) clockticks6502--;
    }

    static void sre() {
        lsr();
        eor();
        if (penaltyop && penaltyaddr) clockticks6502--;
    }

    static void rra() {
        ror();
        adc();
        if (penaltyop && penaltyaddr) clockticks6502--;
    }
#else
    #define lax nop
    #define sax nop
    #define dcp nop
    #define isb nop
    #define slo nop
    #define rla nop
    #define sre nop
    #define rra nop
#endif


// 65C02 instructions
#ifdef CPU_65C02
    static void bra() {
        oldpc = pc;
        pc += reladdr;
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }

    static void stz() {
        putvalue(0);
    }

    static void trb() {
        value = getvalue();
        
        uint8_t t = ~a & value;
        putvalue(t);
        zerocalc(value & a);
    }
    
    static void tsb() {
        value = getvalue();

        putvalue(a | value);
        zerocalc(value & a);
    }
    static void phx() {
        push8(x);
    }

    static void plx() {
        x = pull8();
        zerocalc(x);
        signcalc(x);
    }

    static void phy() {
        push8(y);
    }
    static void ply() {
        y = pull8();
        zerocalc(y);
        signcalc(y);
    }
    

    static void bbr(uint8_t b) {
        uint16_t zpa = (uint16_t)read6502(pc++);
        value = (uint8_t)read6502(zpa);
        
        pc++;
        oldpc = pc;
        if ((value & (1 << b)) == 0) {
            pc += reladdr;
        }
        
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }

    static void bbs(uint8_t b) {
        uint16_t zpa = (uint16_t)read6502(pc++);
        value = (uint8_t)read6502(zpa);
        pc++;
        oldpc = pc;
        if ((value & (1 << b)) != 0) {
            pc += reladdr;
        }
        
        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
            else clockticks6502++;
    }

    static void bbr0() {
        bbr(0);
    }
    static void bbr1() {
        bbr(1);
    }
    static void bbr2() {
        bbr(2);
    }
    static void bbr3() {
        bbr(3);
    }
    static void bbr4() {
        bbr(4);
    }
    static void bbr5() {
        bbr(5);
    }
    static void bbr6() {
        bbr(6);
    }
    static void bbr7() {
        bbr(7);
    }

    static void bbs0() {
        bbs(0);
    }
    static void bbs1() {
        bbs(1);
    }
    static void bbs2() {
        bbs(2);
    }
    static void bbs3() {
        bbs(3);
    }
    static void bbs4() {
        bbs(4);
    }
    static void bbs5() {
        bbs(5);
    }
    static void bbs6() {
        bbs(6);
    }
    static void bbs7() {
        bbs(7);
    }

    static void smb (uint8_t b) {
        // Set specified ZP memory bit
        value = getvalue();
        putvalue(value | (1 << b));
    }

    static void rmb (uint8_t b) {
        // clear specified ZP memory bit
        value = getvalue();
        putvalue(value & ((1 << b) ^ 0XFF));
    }

    static void rmb0() {
        rmb(0);
    }
    static void rmb1() {
        rmb(1);
    }
    static void rmb2() {
        rmb(2);
    }
    static void rmb3() {
        rmb(3);
    }
    static void rmb4() {
        rmb(4);
    }
    static void rmb5() {
        rmb(5);
    }
    static void rmb6() {
        rmb(6);
    }
    static void rmb7() {
        rmb(7);
    }

    static void smb0() {
        smb(0);
    }
    static void smb1() {
        smb(1);
    }
    static void smb2() {
        smb(2);
    }
    static void smb3() {
        smb(3);
    }
    static void smb4() {
        smb(4);
    }
    static void smb5() {
        smb(5);
    }
    static void smb6() {
        smb(6);
    }
    static void smb7() {
        smb(7);
    }
    

    #define abson rel2
    #define absxn rel2
    #define absyn rel2

#else

    #define abson abso
    #define absxn absx
    #define absyn absy
    #define bra nop

    #define bbr0 slo
    #define bbr1 rla
    #define bbr2 rla
    #define bbr3 sre
    #define bbr4 sre
    #define bbr5 rra
    #define bbr6 rra
    #define bbr7 sax
    #define bbs0 nop
    #define bbs1 lax
    #define bbs2 lax
    #define bbs3 dcp
    #define bbs4 dcp
    #define bbs5 isb
    #define bbs6 isb
    #define bbs7 isb

    #define rmb0 slo
    #define rmb1 slo 
    #define rmb2 rla 
    #define rmb3 rla 
    #define rmb4 sre 
    #define rmb5 sre 
    #define rmb6 rra 
    #define rmb7 rra 
    #define smb0 sax 
    #define smb1 sax 
    #define smb2 lax 
    #define smb3 lax 
    #define smb4 dcp 
    #define smb5 dcp 
    #define smb6 isb 
    #define smb7 isb 
#endif


#ifdef CPU_65C02
    static void (*addrtable[256])() = {
/*        |  0  |  1  |   2   |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  A  |  B  |  C  |  D  |  E  |  F  |     */
/* 0 */     imp, indx,  imm,   imp,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imp,  abso, abso, abso, abson, /* 0 */
/* 1 */     rel, indy,  indzp, imp,   zp,  zpx,  zpx,   zp,  imp, absy,  acc,  imp,  abso, absx, absx, absxn, /* 1 */
/* 2 */    abso, indx,  imm,   imp,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imp,  abso, abso, abso, abson, /* 2 */
/* 3 */     rel, indy,  indzp, imp,  zpx,  zpx,  zpx,   zp,  imp, absy,  acc,  imp,  absx, absx, absx, absxn, /* 3 */
/* 4 */     imp, indx,  imm,   imp,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imp,  abso, abso, abso, abson, /* 4 */
/* 5 */     rel, indy,  indzp, imp,  zpx,  zpx,  zpx,   zp,  imp, absy,  imp,  imp,  absx, absx, absx, absxn, /* 5 */
/* 6 */     imp, indx,  imm,   imp,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imp,   ind, abso, abso, abson, /* 6 */
/* 7 */     rel, indy,  indzp, imp,  zpx,  zpx,  zpx,   zp,  imp, absy,  imp,  imp,  aindx, absx, absx, absxn, /* 7 */
/* 8 */     rel, indx,  imm,   imp,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imp,  abso, abso, abso, abson, /* 8 */
/* 9 */     rel, indy,  indzp, imp,  zpx,  zpx,  zpy,   zp,  imp, absy,  imp,  imp,  abso, absx, absx, absyn, /* 9 */
/* A */     imm, indx,  imm,   imp,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imp,  abso, abso, abso, abson, /* A */
/* B */     rel, indy,  indzp, imp,  zpx,  zpx,  zpy,   zp,  imp, absy,  imp,  imp,  absx, absx, absy, absyn, /* B */
/* C */     imm, indx,  imm,   imp,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imp,  abso, abso, abso, abson, /* C */
/* D */     rel, indy,  indzp, imp,  zpx,  zpx,  zpx,   zp,  imp, absy,  imp,  imp,  absx, absx, absx, absxn, /* D */
/* E */     imm, indx,  imm,   imp,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imp,  abso, abso, abso, abson, /* E */
/* F */     rel, indy,  indzp, imp,  zpx,  zpx,  zpx,   zp,  imp, absy,  imp,  imp,  absx, absx, absx, absxn  /* F */
};

#else
static void (*addrtable[256])() = {
/*        |  0  |  1  |   2   |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  A  |  B  |  C  |  D  |  E  |  F  |     */
/* 0 */     imp, indx,  imm,   indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abson, /* 0 */
/* 1 */     rel, indy,  indzp, indy,   zp,  zpx,  zpx,  zpx,  imp, absy,  acc, absy, abso, absx, absx, absxn, /* 1 */
/* 2 */    abso, indx,  imm,   indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abson, /* 2 */
/* 3 */     rel, indy,  indzp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  acc, absy, absx, absx, absx, absxn, /* 3 */
/* 4 */     imp, indx,  imm,   indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abson, /* 4 */
/* 5 */     rel, indy,  indzp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absxn, /* 5 */
/* 6 */     imp, indx,  imm,   indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm,  ind, abso, abso, abson, /* 6 */
/* 7 */     rel, indy,  indzp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absxn, /* 7 */
/* 8 */     rel, indx,  imm,   indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abson, /* 8 */
/* 9 */     rel, indy,  indzp, indy,  zpx,  zpx,  zpy,  zpy,  imp, absy,  imp, absy, abso, absx, absx, absyn, /* 9 */
/* A */     imm, indx,  imm,   indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abson, /* A */
/* B */     rel, indy,  indzp, indy,  zpx,  zpx,  zpy,  zpy,  imp, absy,  imp, absy, absx, absx, absy, absyn, /* B */
/* C */     imm, indx,  imm,   indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abson, /* C */
/* D */     rel, indy,  indzp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absxn, /* D */
/* E */     imm, indx,  imm,   indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abson, /* E */
/* F */     rel, indy,  indzp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absxn  /* F */
};
#endif

static void (*optable[256])() = {
/*        |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  A  |  B  |  C  |  D  |  E  |  F  |      */
/* 0 */      brk,  ora,  nop,  slo,  tsb,  ora,  asl,  rmb0,  php,  ora,  asl,  nop,  tsb,  ora,  asl,  bbr0, /* 0 */
/* 1 */      bpl,  ora,  ora,  slo,  trb,  ora,  asl,  rmb1,  clc,  ora,  inc,  slo,  trb,  ora,  asl,  bbr1, /* 1 */
/* 2 */      jsr,  and,  nop,  rla,  bit,  and,  rol,  rmb2,  plp,  and,  rol,  nop,  bit,  and,  rol,  bbr2, /* 2 */
/* 3 */      bmi,  and,  and,  rla,  bit,  and,  rol,  rmb3,  sec,  and,  dec,  rla,  bit,  and,  rol,  bbr3, /* 3 */
/* 4 */      rti,  eor,  nop,  sre,  nop,  eor,  lsr,  rmb4,  pha,  eor,  lsr,  nop,  jmp,  eor,  lsr,  bbr4, /* 4 */
/* 5 */      bvc,  eor,  eor,  sre,  nop,  eor,  lsr,  rmb5,  cli,  eor,  phy,  sre,  nop,  eor,  lsr,  bbr5, /* 5 */
/* 6 */      rts,  adc,  nop,  rra,  stz,  adc,  ror,  rmb6,  pla,  adc,  ror,  nop,  jmp,  adc,  ror,  bbr6, /* 6 */
/* 7 */      bvs,  adc,  adc,  rra,  stz,  adc,  ror,  rmb7,  sei,  adc,  ply,  rra,  jmp,  adc,  ror,  bbr7, /* 7 */
/* 8 */      bra,  sta,  nop,  sax,  sty,  sta,  stx,  smb0,  dey,  bit,  txa,  nop,  sty,  sta,  stx,  bbs0, /* 8 */
/* 9 */      bcc,  sta,  sta,  nop,  sty,  sta,  stx,  smb1,  tya,  sta,  txs,  nop,  stz,  sta,  stz,  bbs1, /* 9 */
/* A */      ldy,  lda,  ldx,  lax,  ldy,  lda,  ldx,  smb2,  tay,  lda,  tax,  nop,  ldy,  lda,  ldx,  bbs2, /* A */
/* B */      bcs,  lda,  lda,  lax,  ldy,  lda,  ldx,  smb3,  clv,  lda,  tsx,  lax,  ldy,  lda,  ldx,  bbs3, /* B */
/* C */      cpy,  cmp,  nop,  dcp,  cpy,  cmp,  dec,  smb4,  iny,  cmp,  dex,  nop,  cpy,  cmp,  dec,  bbs4, /* C */
/* D */      bne,  cmp,  cmp,  dcp,  nop,  cmp,  dec,  smb5,  cld,  cmp,  phx,  dcp,  nop,  cmp,  dec,  bbs5, /* D */
/* E */      cpx,  sbc,  nop,  isb,  cpx,  sbc,  inc,  smb6,  inx,  sbc,  nop,  nop,  cpx,  sbc,  inc,  bbs6, /* E */
/* F */      beq,  sbc,  sbc,  isb,  nop,  sbc,  inc,  smb7,  sed,  sbc,  plx,  isb,  nop,  sbc,  inc,  bbs7  /* F */
};

static const uint32_t ticktable[256] = {
/*        |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  A  |  B  |  C  |  D  |  E  |  F  |     */
/* 0 */      7,    6,    2,    8,    5,    3,    5,    5,    3,    2,    2,    2,    6,    4,    6,    6,  /* 0 */
/* 1 */      2,    5,    5,    8,    5,    4,    6,    6,    2,    4,    2,    7,    6,    4,    7,    7,  /* 1 */
/* 2 */      6,    6,    2,    8,    3,    3,    5,    5,    4,    2,    2,    2,    4,    4,    6,    6,  /* 2 */
/* 3 */      2,    5,    5,    8,    4,    4,    6,    6,    2,    4,    2,    7,    4,    4,    7,    7,  /* 3 */
/* 4 */      6,    6,    2,    8,    3,    3,    5,    5,    3,    2,    2,    2,    3,    4,    6,    6,  /* 4 */
/* 5 */      2,    5,    5,    8,    4,    4,    6,    6,    2,    4,    3,    7,    4,    4,    7,    7,  /* 5 */
/* 6 */      6,    6,    2,    8,    3,    3,    5,    5,    4,    2,    2,    2,    5,    4,    6,    6,  /* 6 */
/* 7 */      2,    5,    5,    8,    4,    4,    6,    6,    2,    4,    3,    7,    6,    4,    7,    7,  /* 7 */
/* 8 */      2,    6,    2,    6,    3,    3,    3,    3,    2,    2,    2,    2,    4,    4,    4,    4,  /* 8 */
/* 9 */      2,    6,    5,    6,    4,    4,    4,    4,    2,    5,    2,    5,    4,    5,    5,    5,  /* 9 */
/* A */      2,    6,    2,    6,    3,    3,    3,    3,    2,    2,    2,    2,    4,    4,    4,    4,  /* A */
/* B */      2,    5,    5,    5,    4,    4,    4,    4,    2,    4,    2,    4,    4,    4,    4,    4,  /* B */
/* C */      2,    6,    2,    8,    3,    3,    5,    5,    2,    2,    2,    2,    4,    4,    6,    6,  /* C */
/* D */      2,    5,    5,    8,    4,    4,    6,    6,    2,    4,    3,    7,    4,    4,    7,    7,  /* D */
/* E */      2,    6,    2,    8,    3,    3,    5,    5,    2,    2,    2,    2,    4,    4,    6,    6,  /* E */
/* F */      2,    5,    5,    8,    4,    4,    6,    6,    2,    4,    3,    7,    4,    4,    7,    7   /* F */
};


void nmi6502() {
    push16(pc);
    push8(status);
    status |= FLAG_INTERRUPT;
    pc = (uint16_t)read6502(0xFFFA) | ((uint16_t)read6502(0xFFFB) << 8);
}

void irq6502() {
    push16(pc);
    push8(status);
    status |= FLAG_INTERRUPT;
    pc = (uint16_t)read6502(0xFFFE) | ((uint16_t)read6502(0xFFFF) << 8);
}

uint8_t callexternal = 0;
void (*loopexternal)();

void exec6502(uint32_t tickcount) {
    clockgoal6502 += tickcount;

    while (clockticks6502 < clockgoal6502) {
        opcode = read6502(pc++);

        penaltyop = 0;
        penaltyaddr = 0;

        (*addrtable[opcode])();
        (*optable[opcode])();
        clockticks6502 += ticktable[opcode];
        if (penaltyop && penaltyaddr) clockticks6502++;

        instructions++;

        if (callexternal) (*loopexternal)();
    }

}

void step6502() {
    opcode = read6502(pc++);
    
    penaltyop = 0;
    penaltyaddr = 0;

    (*addrtable[opcode])();
    (*optable[opcode])();
    clockticks6502 += ticktable[opcode];
    if (penaltyop && penaltyaddr) clockticks6502++;
    clockgoal6502 = clockticks6502;

    instructions++;

    if (callexternal) (*loopexternal)();
}

void hookexternal(void *funcptr) {
    if (funcptr != (void *)NULL) {
        loopexternal = funcptr;
        callexternal = 1;
    } else callexternal = 0;
}