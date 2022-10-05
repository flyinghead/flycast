FILES=$(find core/gdxsv -type f -name "*.cpp" -or -name "*.h" -not -name "*.pb.h")
clang-format -style='{BasedOnStyle: google, UseTab: Always, IndentWidth: 4, TabWidth: 4, ColumnLimit: 140}' -i ${FILES}
