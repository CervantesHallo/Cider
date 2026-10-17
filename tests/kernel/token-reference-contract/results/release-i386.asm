
/Users/cervantes/.codex/worktrees/quarantine-fixes-20261014/Cider/out/kernel-remediation/build/dlls/ntoskrnl.exe/i386-windows/ntoskrnl.exe:     file format pei-i386


Disassembly of section .text:

10022550 <_PsDereferencePrimaryToken@4>:
10022550:	55                   	push   %ebp
10022551:	89 e5                	mov    %esp,%ebp
10022553:	83 ec 18             	sub    $0x18,%esp
10022556:	8b 55 08             	mov    0x8(%ebp),%edx
10022559:	f6 05 08 d2 02 10 08 	testb  $0x8,0x1002d208
10022560:	75 0e                	jne    10022570 <_PsDereferencePrimaryToken@4+0x20>
10022562:	89 55 08             	mov    %edx,0x8(%ebp)
10022565:	c9                   	leave
10022566:	e9 05 f0 ff ff       	jmp    10021570 <_ObDereferenceObject@4>
1002256b:	2e 8d 74 26 00       	lea    %cs:0x0(%esi,%eiz,1),%esi
10022570:	89 54 24 10          	mov    %edx,0x10(%esp)
10022574:	c7 44 24 0c c7 44 03 	movl   $0x100344c7,0xc(%esp)
1002257b:	10 
1002257c:	c7 44 24 08 90 5b 03 	movl   $0x10035b90,0x8(%esp)
10022583:	10 
10022584:	c7 44 24 04 08 d2 02 	movl   $0x1002d208,0x4(%esp)
1002258b:	10 
1002258c:	c7 04 24 03 00 00 00 	movl   $0x3,(%esp)
10022593:	89 55 fc             	mov    %edx,-0x4(%ebp)
10022596:	e8 05 8d 00 00       	call   1002b2a0 <_wine_dbg_log>
1002259b:	8b 55 fc             	mov    -0x4(%ebp),%edx
1002259e:	89 55 08             	mov    %edx,0x8(%ebp)
100225a1:	c9                   	leave
100225a2:	e9 c9 ef ff ff       	jmp    10021570 <_ObDereferenceObject@4>
100225a7:	90                   	nop
100225a8:	90                   	nop
100225a9:	90                   	nop
100225aa:	90                   	nop
100225ab:	90                   	nop
100225ac:	90                   	nop
100225ad:	90                   	nop
100225ae:	90                   	nop
100225af:	90                   	nop
