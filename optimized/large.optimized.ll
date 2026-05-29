; ModuleID = '/mnt/host/d/CD LAB EL/FunctionInliningpass/ir/large.ll'
source_filename = "/mnt/host/d/CD LAB EL/FunctionInliningpass/tests/large.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-alpine-linux-musl"

; Function Attrs: noinline nounwind optnone sspstrong uwtable
define dso_local i32 @heavy_compute(i32 noundef %0) #0 {
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  store i32 0, ptr %3, align 4
  store i32 0, ptr %4, align 4
  br label %5

5:                                                ; preds = %25, %1
  %6 = load i32, ptr %4, align 4
  %7 = icmp slt i32 %6, 200
  br i1 %7, label %8, label %28

8:                                                ; preds = %5
  %9 = load i32, ptr %2, align 4
  %10 = load i32, ptr %4, align 4
  %11 = add nsw i32 %9, %10
  %12 = load i32, ptr %2, align 4
  %13 = load i32, ptr %4, align 4
  %14 = sub nsw i32 %12, %13
  %15 = mul nsw i32 %11, %14
  %16 = load i32, ptr %3, align 4
  %17 = add nsw i32 %16, %15
  store i32 %17, ptr %3, align 4
  %18 = load i32, ptr %3, align 4
  %19 = shl i32 %18, 1
  %20 = load i32, ptr %3, align 4
  %21 = xor i32 %20, %19
  store i32 %21, ptr %3, align 4
  %22 = load i32, ptr %4, align 4
  %23 = load i32, ptr %3, align 4
  %24 = add nsw i32 %23, %22
  store i32 %24, ptr %3, align 4
  br label %25

25:                                               ; preds = %8
  %26 = load i32, ptr %4, align 4
  %27 = add nsw i32 %26, 1
  store i32 %27, ptr %4, align 4
  br label %5, !llvm.loop !6

28:                                               ; preds = %5
  %29 = load i32, ptr %3, align 4
  ret i32 %29
}

; Function Attrs: noinline nounwind optnone sspstrong uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  store i32 0, ptr %1, align 4
  %2 = call i32 @heavy_compute(i32 noundef 9)
  ret i32 %2
}

attributes #0 = { noinline nounwind optnone sspstrong uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Alpine clang version 17.0.6"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
