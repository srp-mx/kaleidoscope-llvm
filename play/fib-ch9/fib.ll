; ModuleID = 'my cool jit'
source_filename = "my cool jit"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"

declare double @putchard(double)

define double @fib(double %x) !dbg !3 {
entry:
  %x1 = alloca double, align 8
  call void @llvm.dbg.declare(metadata double* %x1, metadata !8, metadata !DIExpression()), !dbg !9
  store double %x, double* %x1, align 8
  %x2 = load double, double* %x1, align 8, !dbg !10
  %cmptmp = fcmp ult double %x2, 3.000000e+00, !dbg !11
  %booltmp = uitofp i1 %cmptmp to double, !dbg !11
  %ifcond = fcmp one double %booltmp, 0.000000e+00, !dbg !11
  br i1 %ifcond, label %then, label %else, !dbg !11

then:                                             ; preds = %entry
  br label %ifcont, !dbg !12

else:                                             ; preds = %entry
  %x3 = load double, double* %x1, align 8, !dbg !13
  %subtmp = fsub double %x3, 1.000000e+00, !dbg !14
  %calltmp = call double @fib(double %subtmp), !dbg !14
  %x4 = load double, double* %x1, align 8, !dbg !15
  %subtmp5 = fsub double %x4, 2.000000e+00, !dbg !16
  %calltmp6 = call double @fib(double %subtmp5), !dbg !16
  %addtmp = fadd double %calltmp, %calltmp6, !dbg !16
  br label %ifcont, !dbg !16

ifcont:                                           ; preds = %else, %then
  %iftmp = phi double [ 1.000000e+00, %then ], [ %addtmp, %else ], !dbg !16
  ret double %iftmp, !dbg !16
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #0

define double @main() !dbg !17 {
entry:
  %calltmp = call double @fib(double 1.000000e+01), !dbg !21
  ret double %calltmp, !dbg !21
}

attributes #0 = { nofree nosync nounwind readnone speculatable willreturn }

!llvm.module.flags = !{!0}
!llvm.dbg.cu = !{!1}

!0 = !{i32 2, !"Debug Info Version", i32 3}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Kaleidoscope Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "fib.ks", directory: ".")
!3 = distinct !DISubprogram(name: "fib", scope: !2, file: !2, line: 3, type: !4, scopeLine: 3, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !7)
!4 = !DISubroutineType(types: !5)
!5 = !{!6, !6}
!6 = !DIBasicType(name: "double", size: 64, encoding: DW_ATE_float)
!7 = !{!8}
!8 = !DILocalVariable(name: "x", arg: 1, scope: !3, file: !2, line: 3, type: !6)
!9 = !DILocation(line: 3, scope: !3)
!10 = !DILocation(line: 4, column: 8, scope: !3)
!11 = !DILocation(line: 4, column: 12, scope: !3)
!12 = !DILocation(line: 5, column: 9, scope: !3)
!13 = !DILocation(line: 7, column: 13, scope: !3)
!14 = !DILocation(line: 7, column: 15, scope: !3)
!15 = !DILocation(line: 7, column: 24, scope: !3)
!16 = !DILocation(line: 7, column: 26, scope: !3)
!17 = distinct !DISubprogram(name: "main", scope: !2, file: !2, line: 9, type: !18, scopeLine: 9, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !20)
!18 = !DISubroutineType(types: !19)
!19 = !{!6}
!20 = !{}
!21 = !DILocation(line: 9, column: 5, scope: !17)
