
/Users/cervantes/.codex/worktrees/quarantine-fixes-20261014/Cider/out/kernel-remediation/build/dlls/ntoskrnl.exe/x86_64-windows/ntoskrnl.exe:     file format pei-x86-64


Disassembly of section .text:

00000001800224e0 <PsDereferencePrimaryToken>:
   1800224e0:	48 83 ec 48          	sub    $0x48,%rsp
   1800224e4:	49 89 ca             	mov    %rcx,%r10
   1800224e7:	f6 05 b2 ae 00 00 08 	testb  $0x8,0xaeb2(%rip)        # 18002d3a0 <__wine_dbch_ntoskrnl>
   1800224ee:	75 10                	jne    180022500 <PsDereferencePrimaryToken+0x20>
   1800224f0:	4c 89 d1             	mov    %r10,%rcx
   1800224f3:	48 83 c4 48          	add    $0x48,%rsp
   1800224f7:	e9 64 ef ff ff       	jmp    180021460 <ObDereferenceObject>
   1800224fc:	0f 1f 40 00          	nopl   0x0(%rax)
   180022500:	48 89 4c 24 20       	mov    %rcx,0x20(%rsp)
   180022505:	4c 8d 0d b7 20 01 00 	lea    0x120b7(%rip),%r9        # 1800345c3 <.rdata+0x4a3>
   18002250c:	4c 8d 05 1d 3a 01 00 	lea    0x13a1d(%rip),%r8        # 180035f30 <__func__.89>
   180022513:	48 89 4c 24 38       	mov    %rcx,0x38(%rsp)
   180022518:	48 8d 15 81 ae 00 00 	lea    0xae81(%rip),%rdx        # 18002d3a0 <__wine_dbch_ntoskrnl>
   18002251f:	b9 03 00 00 00       	mov    $0x3,%ecx
   180022524:	e8 d7 89 00 00       	call   18002af00 <wine_dbg_log>
   180022529:	4c 8b 54 24 38       	mov    0x38(%rsp),%r10
   18002252e:	4c 89 d1             	mov    %r10,%rcx
   180022531:	48 83 c4 48          	add    $0x48,%rsp
   180022535:	e9 26 ef ff ff       	jmp    180021460 <ObDereferenceObject>
   18002253a:	90                   	nop
   18002253b:	90                   	nop
   18002253c:	90                   	nop
   18002253d:	90                   	nop
   18002253e:	90                   	nop
   18002253f:	90                   	nop
