#!/bin/sh

# 기본 설정 (각 타겟별 디렉터리)
C0_DIR="j722s-evm/c75ss0-0_freertos/ti-c7000"
C1_DIR="j722s-evm/c75ss1-0_freertos/ti-c7000"
RMAIN_DIR="j722s-evm/main-r5fss0-0_freertos/ti-arm-clang"
RMCU_DIR="j722s-evm/mcu-r5fss0-0_freertos/ti-arm-clang"
RWAKE_DIR="j722s-evm/wkup-r5fss0-0_freertos/ti-arm-clang"

run_make_syscfg() {
  TARGET_DIR=$1
  if [ -d "$TARGET_DIR" ]; then
    echo "[make syscfg-gui] 실행 중: $TARGET_DIR"
    (cd "$TARGET_DIR" && make syscfg-gui)
  else
    echo "오류: $TARGET_DIR 디렉터리가 존재하지 않습니다."
    exit 1
  fi
}

case "$1" in
  c0) run_make_syscfg "$C0_DIR" ;;
  c1) run_make_syscfg "$C1_DIR" ;;
  rmain) run_make_syscfg "$RMAIN_DIR" ;;
  rmcu) run_make_syscfg "$RMCU_DIR" ;;
  rwake) run_make_syscfg "$RWAKE_DIR" ;;
  *)
    echo "사용법: $0 [c0|c1|rmain|rmcu|rwake]"
    echo "예제:"
    echo "  $0 c0    # c0에서 make syscfg-gui 실행"
    echo "  $0 rmain # rmain에서 make syscfg-gui 실행"
    exit 1
    ;;
esac

