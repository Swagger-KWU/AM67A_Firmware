#!/bin/sh

# 기본 설정
BASE_DIR="./j722s-evm"
C0_DIR="c75ss0-0_freertos/ti-c7000"
C1_DIR="c75ss1-0_freertos/ti-c7000"
RMAIN_DIR="main-r5fss0-0_freertos/ti-arm-clang"
RMCU_DIR="mcu-r5fss0-0_freertos/ti-arm-clang"
RWAKE_DIR="wkup-r5fss0-0_freertos/ti-arm-clang"

copy_file() {
  TARGET_DIR=$1
  FILE_NAME=$2
  DEST_NAME=$3
  
  if [ -f "$BASE_DIR/$TARGET_DIR/$FILE_NAME" ]; then
    echo "$FILE_NAME 복사 중 -> $DEST_NAME"
    cp "$BASE_DIR/$TARGET_DIR/$FILE_NAME" "$DEST_NAME"
  else
    echo "경고: $BASE_DIR/$TARGET_DIR/$FILE_NAME 파일을 찾을 수 없습니다."
  fi
}

delete_file() {
  FILE_NAME=$1
  if [ -f "$FILE_NAME" ]; then
    echo "$FILE_NAME 삭제 중"
    rm -f "$FILE_NAME"
  else
    echo "삭제할 파일 없음: $FILE_NAME"
  fi
}

do_make() {
  TARGET_DIR=$1
  COMMAND=$2
  
  if [ -d "$BASE_DIR/$TARGET_DIR" ]; then
    echo "[$COMMAND] 실행 중: $TARGET_DIR"
    (cd "$BASE_DIR/$TARGET_DIR" && make $COMMAND)

    # clean이면 복사된 파일 삭제
    if [ "$COMMAND" = "clean" ]; then
      case $TARGET_DIR in
        "$C0_DIR")
          delete_file "j722s-c71_0-fw"
          ;;
        "$C1_DIR")
          delete_file "j722s-c71_1-fw"
          ;;
        "$RMAIN_DIR")
          delete_file "j722s-main-r5f0_0-fw"
          ;;
        "$RMCU_DIR")
          delete_file "j722s-mcu-r5f0_0-fw"
          ;;
        "$RWAKE_DIR")
          delete_file "j722s-wake-r5f0_0-fw"
          ;;
      esac
    else
      # make 빌드 후 복사
      case $TARGET_DIR in
        "$C0_DIR")
          copy_file "$C0_DIR" "ipc_rpmsg_echo_linux.c75ss0-0.release.strip.out" "j722s-c71_0-fw"
          ;;
        "$C1_DIR")
          copy_file "$C1_DIR" "ipc_rpmsg_echo_linux.c75ss1-0.release.strip.out" "j722s-c71_1-fw"
          ;;
        "$RMAIN_DIR")
          copy_file "$RMAIN_DIR" "ipc_rpmsg_echo_linux.main-r5f0_0.release.strip.out" "j722s-main-r5f0_0-fw"
          ;;
        "$RMCU_DIR")
          copy_file "$RMCU_DIR" "ipc_rpmsg_echo_linux.mcu-r5f0_0.release.strip.out" "j722s-mcu-r5f0_0-fw"
          ;;
        "$RWAKE_DIR")
          copy_file "$RWAKE_DIR" "ipc_rpmsg_echo_linux.wkup-r5f0_0.release.strip.out" "j722s-wake-r5f0_0-fw"
          ;;
      esac
    fi
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
    echo "  $0 c0         # c0에서 make 실행"
    echo "  $0 c1 clean   # c1에서 make clean 실행 및 파일 삭제"
    echo "  $0 all        # 모든 타겟에서 make 실행"
    echo "  $0 all clean  # 모든 타겟에서 clean 및 바이너리 삭제"
    exit 1
    ;;
esac
