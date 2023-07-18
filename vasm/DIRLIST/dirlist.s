  .org $1000

  .include "hwconfig.s"
  .include "libsd.s"
  .include "libfat32.s"
  .include "liblcd.s"
  
zp_sd_address = $40         ; 2 bytes
zp_sd_currentsector = $42   ; 4 bytes
zp_fat32_variables = $46    ; 24 bytes

fat32_workspace = $200      ; two pages

buffer      = $400
  
CR          = $0D                 ; Carriage Return
LF          = $0A                 ; Line Feed
SP          = $20                 ; Space
ESC         = $1B                 ; Escape
NUL         = $00                 ; Null
MSGL        = $81
MSGH        = $82
ECHO        = $FFEF
GETLINE     = $FF1F

DIRLIST
            jsr via_init
            jsr sd_init
            jsr fat32_init
            bcc initsuccess


  ; Error during FAT32 initialization
            lda #'Z'
            JSR ECHO
            jsr print_char
            lda fat32_errorstage
            jsr print_hex
            jmp DIRLIST ; For testing try again!

initsuccess
  ; Open root directory
            jsr fat32_openroot

  ; Find subdirectory by name
            ldx #<subdirname
            ldy #>subdirname
            jsr fat32_finddirent
            bcc foundsubdir

  ; Subdirectory not found
            lda #'X'
            JSR ECHO
            jsr print_char
            jmp DIRLIST ; For testing try again!

foundsubdir
            ; Open subdirectory
            jsr fat32_opendirent

; ------------------------------------------------
; Don Mod Start
; ------------------------------------------------
; Set your memory positon to $0800
; set pointer at $00 to $1000
; ------------------------------------------------
            PHA             ; PHA operation pushes a copy of the content of Accumulator onto the Stack Structure.
            LDA #$00        ; Put a value of zero into the accumulator
            STA $00         ; Store the accumulator value 00 at location 00  
            LDA #$10        ; Put a value of 08 into the accumulator
            STA $01         ; Store the accumulator at location 01, now position 0001 = 0080 (little endian)
            PLA  

            ldx #$B         ; max value for $01 - 11 chars per entry
            ldy #0          ; index
            sty $781        ; Debug
; ----------------------------
; READ IN one byte at a then 9
; ----------------------------
my_read_directory:
            ;sta fat32_address+1 Not serving any purpose I can tell?
            jsr fat32_readdirent ; A is set to the file's attribute byte and zp_sd_address points at the returned directory entry
            bcs done_read ; Carry set when at end of directory
            sta $790 ; DEBUG
            jsr ECHO ; DEBUG
            ; FIRST TIME
            lda #1 ; Just an indicator
            cmp $791
            BNE PRINT_HEADING ; First cycle so have not printed heading yet
print_dirlist
            ldy #0
            lda (zp_sd_address),y
            jsr ECHO
            iny
            lda (zp_sd_address),y
            jsr ECHO
            iny
            lda (zp_sd_address),y
            jsr ECHO
            iny
            lda (zp_sd_address),y
            jsr ECHO
            iny
            lda (zp_sd_address),y
            jsr ECHO
            iny
            lda (zp_sd_address),y
            jsr ECHO
            iny
            lda (zp_sd_address),y
            jsr ECHO  
            iny
            lda (zp_sd_address),y
            jsr ECHO
            iny
            lda (zp_sd_address),y
            jsr ECHO
            iny
            lda (zp_sd_address),y
            iny
            lda (zp_sd_address),y
            iny
            lda (zp_sd_address),y

            JSR PRINTCR ; Was jus this one
            JSR PRINTLF
            

            bra my_read_directory   ; Branch till we don't need to branch no more

            jsr done_read           ; instead of brk, jmp done - probably return to wozmon
            RTS
  done_read:
            LDA #0
            STA $791                ; Just an indicator
            clc                     ; CLEAR THE CARRY
            JSR PRINTCR
            JSR PRINTLF
                  
            ; Lets now disable the VIA else continues to spin SD Card
            lda #0
            sta SD_CS

            ;PORTA_DISABLE = LCD_E | LCD_RW | LCD_RS | SD_CS | SD_SCK | SD_MOSI
            ;lda #PORTA_DISABLE   ; Set various pins on port A to output
            ;sta DDRA
            ; Reset the lCD
            jsr lcd_cleardisplay

            RTS
            
;print2:     lda message,x
;            beq DONELCD2
;            jsr print_char
;            inx
;            jmp print2        
;DONELCD2:   NOP
;            rts ; Return
            ;JMP INPUT           ; INPUT CYCLE 
            ;JMP GETLINE
            ;JMP $FF00


PRINT_HEADING
            LDA #1  ; Just an indicator
            STA $791  

            JSR PRINTCR
            JSR PRINTLF
            ;  

            LDA #<MSGTOPBAR
            STA MSGL
            LDA #>MSGTOPBAR
            STA MSGH
            JSR SHWMSG

            JSR PRINTCR ; was just this one
            JSR PRINTLF            

            LDA #<MSGDIRLST
            STA MSGL
            LDA #>MSGDIRLST
            STA MSGH
            JSR SHWMSG

            JSR PRINTCR  ; Was just this one
            JSR PRINTLF

            LDA #<MSGUSEINF
            STA MSGL
            LDA #>MSGUSEINF
            STA MSGH
            JSR SHWMSG

            JSR PRINTCR  ; Was just this one
            JSR PRINTLF

            LDA #<MSGLOWBAR
            STA MSGL
            LDA #>MSGLOWBAR
            STA MSGH
            JSR SHWMSG

            JSR PRINTCR  ; Was just this one
            JSR PRINTLF

            JSR print_dirlist
            RTS
 
SHWMSG      LDY #$0
PRINT       LDA (MSGL),Y
            BEQ DONE
            JSR ECHO
            INY 
            BNE PRINT
DONE        RTS   

PRINTCR
            lda #CR 
            jsr ECHO
            rts
  
PRINTLF
            lda #LF 
            jsr ECHO
            rts

message:     .asciiz "19200 kbs Serial                        8 / N / 1 READY!"
MSGTOPBAR   .BYTE "---------------------------------",0
MSGDIRLST   .BYTE "     DIRECTORY LIST             ",0
MSGUSEINF   .BYTE "     SUBFOLDERS LEFT ALIGNED     ",0
MSGLOWBAR   .BYTE "--------------------------------",0

;.export subdirname
;subdirname:  .asciiz   "SUBFOLDR   "
subdirname:  .asciiz    "FORTH      "
;.export filename
;filename: .asciiz "KRUSADER   "

;      .segment "VCTRS"
      .word DIRLIST            
;            RTS
