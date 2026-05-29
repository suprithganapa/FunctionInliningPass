; ModuleID = '/mnt/host/d/CD LAB EL/FunctionInliningpass/ir/mixed.ll'
source_filename = "/mnt/host/d/CD LAB EL/FunctionInliningpass/tests/mixed.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-alpine-linux-musl"

; Function Attrs: noinline nounwind optnone sspstrong uwtable
define dso_local i32 @large_helper(i32 noundef %0) #0 {
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  store i32 0, ptr %3, align 4
  store i32 0, ptr %4, align 4
  br label %5

5:                                                ; preds = %22, %1
  %6 = load i32, ptr %4, align 4
  %7 = icmp slt i32 %6, 100
  br i1 %7, label %8, label %25

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
  %19 = ashr i32 %18, 1
  %20 = load i32, ptr %3, align 4
  %21 = xor i32 %20, %19
  store i32 %21, ptr %3, align 4
  br label %22

22:                                               ; preds = %8
  %23 = load i32, ptr %4, align 4
  %24 = add nsw i32 %23, 1
  store i32 %24, ptr %4, align 4
  br label %5, !llvm.loop !6

25:                                               ; preds = %5
  %26 = load i32, ptr %3, align 4
  ret i32 %26
}

; Function Attrs: noinline nounwind optnone sspstrong uwtable
define dso_local i32 @recurse(i32 noundef %0) #0 {
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 %0, ptr %3, align 4
  %4 = load i32, ptr %3, align 4
  %5 = icmp sle i32 %4, 0
  br i1 %5, label %6, label %7

6:                                                ; preds = %1
  store i32 0, ptr %2, align 4
  br label %13

7:                                                ; preds = %1
  %8 = load i32, ptr %3, align 4
  %9 = load i32, ptr %3, align 4
  %10 = sub nsw i32 %9, 1
  %11 = call i32 @recurse(i32 noundef %10)
  %12 = add nsw i32 %8, %11
  store i32 %12, ptr %2, align 4
  br label %13

13:                                               ; preds = %7, %6
  %14 = load i32, ptr %2, align 4
  ret i32 %14
}

; Function Attrs: noinline nounwind optnone sspstrong uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = alloca i32, align 4
  %7 = alloca i32, align 4
  %8 = alloca i32, align 4
  store i32 0, ptr %4, align 4
  call void @llvm.lifetime.start.p0(i64 4, ptr %2)
  call void @llvm.lifetime.start.p0(i64 4, ptr %3)
  store i32 2, ptr %2, align 4
  store i32 3, ptr %3, align 4
  %9 = load i32, ptr %2, align 4
  %10 = load i32, ptr %3, align 4
  %11 = add nsw i32 %9, %10
  call void @llvm.lifetime.end.p0(i64 4, ptr %2)
  call void @llvm.lifetime.end.p0(i64 4, ptr %3)
  store i32 %11, ptr %5, align 4
  %12 = load i32, ptr %5, align 4
  call void @llvm.lifetime.start.p0(i64 4, ptr %1)
  store i32 %12, ptr %1, align 4
  %13 = load i32, ptr %1, align 4
  %14 = mul nsw i32 %13, 3
  call void @llvm.lifetime.end.p0(i64 4, ptr %1)
  store i32 %14, ptr %6, align 4
  %15 = load i32, ptr %6, align 4
  %16 = call i32 @large_helper(i32 noundef %15)
  store i32 %16, ptr %7, align 4
  %17 = call i32 @recurse(i32 noundef 4)
  store i32 %17, ptr %8, align 4
  %18 = load i32, ptr %7, align 4
  %19 = load i32, ptr %8, align 4
  %20 = add nsw i32 %18, %19
  ret i32 %20
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

attributes #0 = { noinline nounwind optnone sspstrong uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }

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
