#!/bin/sh

# 기본 설정
BASE_DIR="./j722s-evm"
C0_DIR="c75ss0-0_freertos/ti-c7000"
C1_DIR="c75ss1-0_freertos/ti-c7000"
RMAIN_DIR="main-r5fss0-0_freertos/ti-arm-clang"
RMCU_DIR="mcu-r5fss0-0_freertos/ti-arm-clang"
RWAKE_DIR="wkup-r5fss0-0_freertos/ti-arm-clang"

do_make() {
  TARGET_DIR=$1
  COMMAND=$2
  
  if [ -d "$BASE_DIR/$TARGET_DIR" ]; then
    echo "[$COMMAND] 실행 중: $TARGET_DIR"
    (cd "$BASE_DIR/$TARGET_DIR" && make $COMMAND)
  else
    echo "경고: $TARGET_DIR 디렉터리가 존재하지 않습니다."
  fi
}

case "$1" in
  c0) do_make "$C0_DIR" "$2" ;;
  c1) do_make "$C1_DIR" "$2" ;;
  rmain) do_make "$RMAIN_DIR" "$2" ;;
  rmcu) do_make "$RMCU_DIR" "$2" ;;
  rwake) do_make "$RWAKE_DIR" "$2" ;;
  all) 
    do_make "$C0_DIR" "$2"
    do_make "$C1_DIR" "$2"
    do_make "$RMAIN_DIR" "$2"
    do_make "$RMCU_DIR" "$2"
    do_make "$RWAKE_DIR" "$2"
    ;;
  *)
    echo "사용법: $0 [c0|c1|rmain|rmcu|rwake|all] [make 옵션]"
    echo "예제:"
    echo "  $0 c0 all    # c0에서 make 실행"
    echo "  $0 c1 clean  # c1에서 make clean 실행"
    echo "  $0 all all   # 모든 타겟에서 make 실행"
    exit 1
    ;;
esac