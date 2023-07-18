PORTB = $7000
PORTA = $7001
DDRB =  $7002
DDRA =  $7003

E  = %10000000
RW = %01000000
RS = %00100000

  .org $4000

reset:
  ;ldx #$ff
  ;txs

  lda #%11111111 ; Set all pins on port B to output
  sta DDRB
  JSR short_delay
  lda #%11100000 ; Set top 3 pins on port A to output
  sta DDRA
  JSR short_delay

  lda #%00111000 ; Set 8-bit mode; 2-line display; 5x8 font
  jsr lcd_instruction
  lda #%00001110 ; Display on; cursor on; blink off
  jsr lcd_instruction
  lda #%00000110 ; Increment and shift cursor; don't shift display
  jsr lcd_instruction
  lda #$00000001 ; Clear display
  jsr lcd_instruction

  ldx #0
print:
  lda message,x
  beq loop
  jsr print_char
  inx
  jmp print

loop:
  ;jmp loop
  ;jmp $FFF0
  RTS
;message: .asciiz "Hello, world!"
message: .asciiz "Hello, world!                           Don's 65C02 PICO"


lcd_wait:
  pha
  lda #%00000000  ; Port B is input
  sta DDRB
  PHX
  PHY
  JSR short_delay
  PLY
  PLX
lcdbusy:
  lda #RW
  sta PORTA
  lda #(RW | E)
  sta PORTA
  lda PORTB
  and #%10000000
  bne lcdbusy

  lda #RW
  sta PORTA
  lda #%11111111  ; Port B is output
  sta DDRB
  pla
  rts

lcd_instruction:
  jsr lcd_wait
  sta PORTB
  lda #0         ; Clear RS/RW/E bits
  sta PORTA
  lda #E         ; Set E bit to send instruction
  sta PORTA
  lda #0         ; Clear RS/RW/E bits
  sta PORTA
  rts

print_char:
  jsr lcd_wait
  sta PORTB
  lda #RS         ; Set RS; Clear RW/E bits
  sta PORTA
  lda #(RS | E)   ; Set E bit to send instruction
  sta PORTA
  lda #RS         ; Clear E bits
  sta PORTA
  rts

short_delay
  ldx #128
lp
  dex
  bne lp
  rts
  

delay
  ldx #0
  ldy #0
.loop
  dey
  bne .loop
  dex
  bne .loop
  rts

longdelay
  jsr mediumdelay
  jsr mediumdelay
  jsr mediumdelay
mediumdelay
  jsr delay
  jsr delay
  jsr delay
  jmp delay

  .word reset
