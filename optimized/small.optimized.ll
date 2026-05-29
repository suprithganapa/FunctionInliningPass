; ModuleID = '/mnt/host/d/CD LAB EL/FunctionInliningpass/ir/small.ll'
source_filename = "/mnt/host/d/CD LAB EL/FunctionInliningpass/tests/small.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-alpine-linux-musl"

; Function Attrs: noinline nounwind optnone sspstrong uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = alloca i32, align 4
  store i32 0, ptr %3, align 4
  store i32 2, ptr %4, align 4
  store i32 3, ptr %5, align 4
  %7 = load i32, ptr %4, align 4
  %8 = load i32, ptr %5, align 4
  call void @llvm.lifetime.start.p0(i64 4, ptr %1)
  call void @llvm.lifetime.start.p0(i64 4, ptr %2)
  store i32 %7, ptr %1, align 4
  store i32 %8, ptr %2, align 4
  %9 = load i32, ptr %1, align 4
  %10 = load i32, ptr %2, align 4
  %11 = add nsw i32 %9, %10
  call void @llvm.lifetime.end.p0(i64 4, ptr %1)
  call void @llvm.lifetime.end.p0(i64 4, ptr %2)
  store i32 %11, ptr %6, align 4
  %12 = load i32, ptr %6, align 4
  ret i32 %12
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
